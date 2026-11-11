# SPDX-License-Identifier: LicenseRef-CSSL-1.0

import re
import logging
from collections import deque

class Default:
	def run(file, opt=None):
		success = True
		logs = []
		with open(file, "r") as f:
			for line in f.readlines():
				result = re.search('[Aa]ssertion', line)
				if result:
					logs.append(line)
					success = False
		return success, "\n".join(logs)

class ContainsString:
	def run(file, needle):
		with open(file, "r") as f:
			for line in f.readlines():
				if re.search(needle, line):
					return True, ""
		return False, f"could not find '{needle}' in logs"

class LastLineContains:
	def run(file, string):
		with open(file, "r") as f:
			last_line = deque(f, maxlen=1).pop()
		success = string in last_line
		log = "" if success else f"could not find '{string}' in last line"
		return success, log

class EndsWithBye:
	def run(file, option):
		# no option: we ignore any
		with open(file, "r") as f:
			n = 0
			for line in reversed(f.readlines()):
				if re.search(r'^Bye.\n', line, re.MULTILINE) is not None:
					return True, ""
				n = n + 1
				if n > 50: # if not found in last 50 lines, assume it does not exist
					break
		return False, "No Bye. message found, did not stop properly"

class RetxCheck():
	def _parseList(s):
		if not s:
			return []
		return [int(n) for n in s.split(",")]

	def _get_ul_dl(s):
		dl, ul = (None, None)
		for o in s.split(";", 1):
			if o.startswith("dl="):
				dl = o[3:]
			elif o.startswith("ul="):
				ul = o[3:]
			else:
				raise ValueError(f"unrecognized option {s}: needs format '[ul=,dl=]X,Y,...[;ul=Z,V,...]'")
		return dl, ul

	def _parseRetxCheckers(options):
		if not "=" in options:
			dlul = RetxCheck._parseList(options)
			return dlul, dlul
		dl, ul = RetxCheck._get_ul_dl(options)
		return RetxCheck._parseList(dl), RetxCheck._parseList(ul)

	def _analyzeUeRetx(rounds, checkers, regex):
		perc = list(0 for i in checkers) # results in %
		stats = list(False for i in checkers) # status if succeeded
		tmp = re.match(regex, rounds)
		if tmp is None:
			logging.error('_analyzeUeRetx: did not match regex for DL retx analysis')
			return stats
		retx_data = [float(x) for x in tmp.groups()]
		for i in range(0, len(perc)):
			#case where numerator > denumerator with denum ==0 is disregarded, cannot hapen in principle, will lead to 0%
			perc[i] = 0 if (retx_data[i] == 0) else 100 * retx_data[i + 1] / retx_data[i]
			#treating % > 100 , % > requirement
			stats[i] = perc[i] <= 100 and perc[i] <= checkers[i]
		return stats

	def _filter_retx_rounds(l):
		# map to round number if False, else None
		l = [i+1 if x is False else None for i, x in enumerate(l)]
		# filter out None
		return [x for x in l if x is not None]

	def run(file, options):
		# aggregate statistics per UE
		dlsch_ulsch_stats = {}
		status = True
		with open(file, "r") as f:
			for line in f.readlines():
				keys = {'dlsch_rounds','ulsch_rounds'}
				for k in keys:
					result = re.search(k, line)
					if result is None:
						continue
					result = re.search('UE (?:RNTI )?([0-9a-f]{4})', line)
					if result is None:
						logging.error(f'did not find RNTI while matching key {k}')
						status = False
						continue
					rnti = result.group(1)
					if not rnti in dlsch_ulsch_stats:
						dlsch_ulsch_stats[rnti] = {}
					dlsch_ulsch_stats[rnti][k]=re.sub(r'^.*\]\s+', r'' , line.rstrip())

		dl, ul = RetxCheck._parseRetxCheckers(options)
		msgs = []
		for ue in dlsch_ulsch_stats:
			dlulstat = dlsch_ulsch_stats[ue]

			for key in dlulstat:
				msgs.append(dlulstat[key])

			retx_dl = RetxCheck._analyzeUeRetx(dlulstat['dlsch_rounds'], dl, r'^.*dlsch_rounds\s+(\d+)\/(\d+)\/(\d+)\/(\d+),\s+dlsch_errors\s+(\d+)')
			retx_ul = RetxCheck._analyzeUeRetx(dlulstat['ulsch_rounds'], ul, r'^.*ulsch_rounds\s+(\d+)\/(\d+)\/(\d+)\/(\d+),\s+ulsch_errors\s+(\d+)')

			if False in retx_dl or False in retx_ul:
				dlstr = RetxCheck._filter_retx_rounds(retx_dl)
				ulstr = RetxCheck._filter_retx_rounds(retx_ul)
				msg = f"UE {ue}: retx rounds"
				if dlstr:
					msg = f"{msg} DL {dlstr}"
				if ulstr:
					msg = f"{msg} UL {ulstr}"
				msg = f"{msg} exceeded threshold"
				msgs.append('!!! Failure: ' + msg)
				logging.error(f'\u001B[1;37;41m {msg}\u001B[0m')
				status = False
			else:
				msg = f"UE {ue} retransmissions within bounds"
				logging.debug(msg)
		return status, "\n".join(msgs)
