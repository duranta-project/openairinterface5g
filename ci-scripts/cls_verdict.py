# SPDX-License-Identifier: LicenseRef-CSSL-1.0

# Classify the outcome of each testCase step so that a red pipeline can be
# told apart from a regression: a step either passes, fails in a way that is
# already tracked (known_failures.yaml or <may_fail>), fails for the first
# time, or fails because the CI itself broke.

import datetime
import enum
import json
import os

import yaml

DEFAULT_MANIFEST = os.environ.get('OAI_CI_KNOWN_FAILURES') or \
	os.path.join(os.path.dirname(os.path.abspath(__file__)), 'known_failures.yaml')

# Steps that only prepare the test server; a failure there says nothing about
# the code under test.
INFRA_CLASSES = {
	'Pull_Local_Registry',
	'Push_Local_Registry',
	'Clean_Test_Server_Images',
	'Create_Workspace',
	'Pull_Cluster_Image',
}

class Verdict(str, enum.Enum):
	PASS = 'PASS'
	KNOWN_FAIL = 'KNOWN-FAIL'
	NEW_FAIL = 'NEW-FAIL'
	INFRA = 'INFRA'
	SKIP = 'SKIP'

class KnownFailure:
	REQUIRED = ('xml', 'desc', 'issue', 'expires')

	def __init__(self, entry, idx):
		if not isinstance(entry, dict):
			raise ValueError(f'known failure #{idx}: expected a mapping, got {type(entry).__name__}')
		missing = [k for k in self.REQUIRED if not entry.get(k)]
		if missing:
			raise ValueError(f'known failure #{idx}: missing {", ".join(missing)}')
		self.xml = os.path.basename(str(entry['xml']))
		self.desc = str(entry['desc'])
		self.issue = str(entry['issue'])
		self.reason = str(entry.get('reason', ''))
		self.occurrence = entry.get('occurrence')
		self.checks = entry.get('checks')
		if self.checks is not None and (not isinstance(self.checks, list) or not self.checks
		                                or not all(isinstance(c, str) and ':' in c for c in self.checks)):
			raise ValueError(f'known failure #{idx}: checks must be a non-empty list of "service:Check"')
		if self.occurrence is not None and (not isinstance(self.occurrence, int) or self.occurrence < 1):
			raise ValueError(f'known failure #{idx}: occurrence must be a positive integer')
		expires = entry['expires']
		if isinstance(expires, str):
			try:
				expires = datetime.date.fromisoformat(expires)
			except ValueError:
				raise ValueError(f'known failure #{idx}: expires "{expires}" is not YYYY-MM-DD')
		if not isinstance(expires, datetime.date):
			raise ValueError(f'known failure #{idx}: expires must be a date')
		self.expires = expires

	def matches(self, xml, desc, occurrence):
		if self.xml != os.path.basename(xml) or self.desc != desc:
			return False
		return self.occurrence is None or self.occurrence == occurrence

	def covers(self, failed_checks):
		# without `checks`, the entry covers any failure of the step; with it,
		# only failures limited to the listed log-analysis checks
		if self.checks is None:
			return True
		return bool(failed_checks) and set(failed_checks) <= set(self.checks)

	def expired(self, today):
		return today > self.expires

def load_manifest(path=DEFAULT_MANIFEST):
	if not os.path.isfile(path):
		return []
	with open(path) as f:
		data = yaml.safe_load(f) or {}
	entries = data.get('known_failures') or []
	if not isinstance(entries, list):
		raise ValueError(f'{path}: known_failures must be a list')
	return [KnownFailure(e, i) for i, e in enumerate(entries, start=1)]

class StepResult:
	def __init__(self, xml, idx, cls, desc, occurrence, verdict, issue=None, note='', failed_checks=()):
		self.xml = os.path.basename(xml)
		self.idx = idx
		self.cls = cls
		self.desc = desc
		self.occurrence = occurrence
		self.verdict = verdict
		self.issue = issue
		self.note = note
		self.failed_checks = list(failed_checks)

	def to_dict(self):
		return {
			'xml': self.xml, 'idx': self.idx, 'class': self.cls, 'desc': self.desc,
			'occurrence': self.occurrence, 'verdict': self.verdict.value,
			'issue': self.issue, 'note': self.note, 'failed_checks': self.failed_checks,
		}

class Classifier:
	def __init__(self, known_failures, today=None):
		self.known = known_failures
		self.today = today or datetime.date.today()
		self.results = []
		self._seen = {}

	def _occurrence(self, xml, desc):
		key = (os.path.basename(xml), desc)
		self._seen[key] = self._seen.get(key, 0) + 1
		return self._seen[key]

	def classify(self, xml, idx, cls, desc, succeeded, may_fail=False, exception=False, skipped=False, failed_checks=()):
		occurrence = self._occurrence(xml, desc)
		listed = [k for k in self.known if k.matches(xml, desc, occurrence)]
		match = next((k for k in listed if k.covers(failed_checks)), None)
		if skipped:
			r = StepResult(xml, idx, cls, desc, occurrence, Verdict.SKIP, failed_checks=failed_checks)
		elif succeeded:
			note = f'listed as known failure ({listed[0].issue}) but passed' if listed else ''
			r = StepResult(xml, idx, cls, desc, occurrence, Verdict.PASS, failed_checks=failed_checks, note=note)
		elif exception or cls in INFRA_CLASSES:
			r = StepResult(xml, idx, cls, desc, occurrence, Verdict.INFRA, failed_checks=failed_checks,
			               note='CI code exception' if exception else 'test server preparation failed')
		elif match and not match.expired(self.today):
			r = StepResult(xml, idx, cls, desc, occurrence, Verdict.KNOWN_FAIL, failed_checks=failed_checks,
			               issue=match.issue, note=match.reason)
		elif match:
			r = StepResult(xml, idx, cls, desc, occurrence, Verdict.NEW_FAIL, failed_checks=failed_checks, issue=match.issue,
			               note=f'known failure entry expired on {match.expires.isoformat()}')
		elif may_fail:
			r = StepResult(xml, idx, cls, desc, occurrence, Verdict.KNOWN_FAIL, failed_checks=failed_checks,
			               note='may_fail in XML')
		elif listed:
			r = StepResult(xml, idx, cls, desc, occurrence, Verdict.NEW_FAIL, failed_checks=failed_checks, issue=listed[0].issue,
			               note=f'known failure covers only {", ".join(listed[0].checks)}')
		else:
			r = StepResult(xml, idx, cls, desc, occurrence, Verdict.NEW_FAIL, failed_checks=failed_checks)
		self.results.append(r)
		return r

	def counts(self, xml=None):
		c = {v.value: 0 for v in Verdict}
		for r in self.results:
			if xml is None or r.xml == os.path.basename(xml):
				c[r.verdict.value] += 1
		return c

	def _worst(self, results):
		for v in (Verdict.NEW_FAIL, Verdict.INFRA, Verdict.KNOWN_FAIL):
			if any(r.verdict == v for r in results):
				return v
		return Verdict.PASS

	def overall(self):
		c = self.counts()
		if c[Verdict.NEW_FAIL.value]:
			return Verdict.NEW_FAIL
		if c[Verdict.INFRA.value]:
			return Verdict.INFRA
		if c[Verdict.KNOWN_FAIL.value]:
			return Verdict.KNOWN_FAIL
		return Verdict.PASS

	def write_json(self, path):
		with open(path, 'w') as f:
			json.dump({
				'verdict': self.overall().value,
				'counts': self.counts(),
				'steps': [r.to_dict() for r in self.results if r.verdict != Verdict.PASS or r.note],
			}, f, indent=2)

	def write_markdown(self, path):
		# One header line with the overall verdict, then one line per testcase
		# XML that did not pass cleanly; meant to be nested under a list item
		# in a PR comment, where emoji stand in for the colours of the HTML report.
		by_xml = {}
		for r in self.results:
			by_xml.setdefault(r.xml, []).append(r)
		lines = [f'{EMOJI[self.overall()]} {SUMMARY[self.overall()]}']
		passed = 0
		order = (Verdict.NEW_FAIL, Verdict.INFRA, Verdict.KNOWN_FAIL)
		rows = sorted(by_xml.items(), key=lambda kv: order.index(self._worst(kv[1])) if self._worst(kv[1]) in order else len(order))
		for xml, results in rows:
			worst = self._worst(results)
			if worst == Verdict.PASS:
				passed += 1
				continue
			parts = []
			for v in order:
				steps = '; '.join(_step_text(r) for r in results if r.verdict == v)
				if steps:
					parts.append(f'{EMOJI[v] + " " if v != worst else ""}{LABEL[v]}: {steps}')
			lines.append(f'   * {EMOJI[worst]} `{xml}`: ' + ' \u00B7 '.join(parts))
		if passed:
			lines.append(f'   * {EMOJI[Verdict.PASS]} {passed} testcase file(s) passed')
		with open(path, 'w', encoding='utf-8') as f:
			f.write('\n'.join(lines) + '\n')

EMOJI = {
	Verdict.PASS: '\U0001F7E2',        # green circle
	Verdict.KNOWN_FAIL: '\U0001F7E1',  # yellow circle
	Verdict.NEW_FAIL: '\U0001F534',    # red circle
	Verdict.INFRA: '\U0001F7E3',       # purple circle
	Verdict.SKIP: '\u26AA',            # white circle
}
LABEL = {
	Verdict.NEW_FAIL: 'new failure',
	Verdict.INFRA: 'CI infrastructure',
	Verdict.KNOWN_FAIL: 'known failure',
}
SUMMARY = {
	Verdict.PASS: 'passed',
	Verdict.KNOWN_FAIL: 'passed with known failures',
	Verdict.NEW_FAIL: 'new failures',
	Verdict.INFRA: 'failed on CI infrastructure',
}

def _step_text(r):
	text = f'{r.desc} (step {int(r.idx)})'
	if r.failed_checks:
		text += ' ' + ', '.join(f'`{c}`' for c in r.failed_checks)
	if r.verdict == Verdict.KNOWN_FAIL and r.issue:
		text += f' [issue]({r.issue})'
	elif r.verdict == Verdict.KNOWN_FAIL and r.note:
		text += f' ({r.note})'
	elif r.verdict == Verdict.INFRA and r.note:
		text += f' ({r.note})'
	elif r.verdict == Verdict.NEW_FAIL and r.note:
		text += f' ({r.note})'
	return text
