# SPDX-License-Identifier: LicenseRef-CSSL-1.0

import sys
import datetime
import os
import tempfile
import unittest

sys.path.append('./') # to find OAI imports below
import cls_verdict
from cls_verdict import Verdict

def kf(**kw):
	e = {'xml': 'a.xml', 'desc': 'Ping', 'issue': 'https://x/1', 'expires': '2026-12-01'}
	e.update(kw)
	return cls_verdict.KnownFailure(e, 1)

TODAY = datetime.date(2026, 11, 3)

class TestVerdict(unittest.TestCase):
	def test_pass(self):
		c = cls_verdict.Classifier([], TODAY)
		self.assertEqual(c.classify('x/a.xml', '000001', 'Ping', 'Ping', True).verdict, Verdict.PASS)
		self.assertEqual(c.overall(), Verdict.PASS)

	def test_new_fail(self):
		c = cls_verdict.Classifier([], TODAY)
		self.assertEqual(c.classify('a.xml', '000001', 'Ping', 'Ping', False).verdict, Verdict.NEW_FAIL)
		self.assertEqual(c.overall(), Verdict.NEW_FAIL)

	def test_known_fail_matches_basename(self):
		c = cls_verdict.Classifier([kf()], TODAY)
		r = c.classify('xml_files/a.xml', '000001', 'Ping', 'Ping', False)
		self.assertEqual(r.verdict, Verdict.KNOWN_FAIL)
		self.assertEqual(r.issue, 'https://x/1')
		self.assertEqual(c.overall(), Verdict.KNOWN_FAIL)

	def test_expired_is_new_fail(self):
		c = cls_verdict.Classifier([kf(expires='2026-11-02')], TODAY)
		r = c.classify('a.xml', '000001', 'Ping', 'Ping', False)
		self.assertEqual(r.verdict, Verdict.NEW_FAIL)
		self.assertIn('expired', r.note)

	def test_expiry_day_still_known(self):
		c = cls_verdict.Classifier([kf(expires='2026-11-03')], TODAY)
		self.assertEqual(c.classify('a.xml', '000001', 'Ping', 'Ping', False).verdict, Verdict.KNOWN_FAIL)

	def test_occurrence(self):
		c = cls_verdict.Classifier([kf(occurrence=2)], TODAY)
		self.assertEqual(c.classify('a.xml', '000001', 'Ping', 'Ping', False).verdict, Verdict.NEW_FAIL)
		self.assertEqual(c.classify('a.xml', '000002', 'Ping', 'Ping', False).verdict, Verdict.KNOWN_FAIL)

	def test_occurrence_counts_skipped(self):
		c = cls_verdict.Classifier([kf(occurrence=2)], TODAY)
		c.classify('a.xml', '000001', 'Ping', 'Ping', False, skipped=True)
		self.assertEqual(c.classify('a.xml', '000002', 'Ping', 'Ping', False).verdict, Verdict.KNOWN_FAIL)

	def test_other_xml_not_matched(self):
		c = cls_verdict.Classifier([kf()], TODAY)
		self.assertEqual(c.classify('b.xml', '000001', 'Ping', 'Ping', False).verdict, Verdict.NEW_FAIL)

	def test_known_but_passed_is_flagged(self):
		c = cls_verdict.Classifier([kf()], TODAY)
		r = c.classify('a.xml', '000001', 'Ping', 'Ping', True)
		self.assertEqual(r.verdict, Verdict.PASS)
		self.assertIn('but passed', r.note)

	def test_may_fail(self):
		c = cls_verdict.Classifier([], TODAY)
		self.assertEqual(c.classify('a.xml', '000001', 'Ping', 'Ping', False, may_fail=True).verdict, Verdict.KNOWN_FAIL)

	def test_infra(self):
		c = cls_verdict.Classifier([kf(desc='Pull')], TODAY)
		self.assertEqual(c.classify('a.xml', '000001', 'Pull_Local_Registry', 'Pull', False).verdict, Verdict.INFRA)
		self.assertEqual(c.classify('a.xml', '000002', 'Ping', 'Ping', False, exception=True).verdict, Verdict.INFRA)
		self.assertEqual(c.overall(), Verdict.INFRA)

	def test_new_fail_dominates_infra(self):
		c = cls_verdict.Classifier([], TODAY)
		c.classify('a.xml', '000001', 'Pull_Local_Registry', 'Pull', False)
		c.classify('a.xml', '000002', 'Ping', 'Ping', False)
		self.assertEqual(c.overall(), Verdict.NEW_FAIL)

	def test_checks_subset_is_known(self):
		c = cls_verdict.Classifier([kf(desc='Undeploy', checks=['oai-gnb:RetxCheck'])], TODAY)
		r = c.classify('a.xml', '000001', 'Undeploy_Object', 'Undeploy', False, failed_checks=['oai-gnb:RetxCheck'])
		self.assertEqual(r.verdict, Verdict.KNOWN_FAIL)

	def test_checks_extra_failure_is_new(self):
		c = cls_verdict.Classifier([kf(desc='Undeploy', checks=['oai-gnb:RetxCheck'])], TODAY)
		r = c.classify('a.xml', '000001', 'Undeploy_Object', 'Undeploy', False,
		               failed_checks=['oai-gnb:RetxCheck', 'oai-gnb:EndsWithBye'])
		self.assertEqual(r.verdict, Verdict.NEW_FAIL)
		self.assertIn('not all covered', r.note)

	def test_checks_without_recorded_checks_is_new(self):
		c = cls_verdict.Classifier([kf(desc='Undeploy', checks=['oai-gnb:RetxCheck'])], TODAY)
		self.assertEqual(c.classify('a.xml', '000001', 'Undeploy_Object', 'Undeploy', False).verdict, Verdict.NEW_FAIL)

	def test_invalid_entries(self):
		for bad in ({'issue': ''}, {'expires': '01/12/2026'}, {'occurrence': 0}, {'checks': []}, {'checks': ['RetxCheck']}):
			with self.assertRaises(ValueError):
				kf(**bad)
		with self.assertRaises(ValueError):
			cls_verdict.KnownFailure('not a dict', 1)

	def test_missing_manifest_is_empty(self):
		self.assertEqual(cls_verdict.load_manifest('/nonexistent/known_failures.yaml'), [])

	def test_shipped_manifest_is_valid(self):
		path = os.path.join(os.path.dirname(cls_verdict.__file__), 'known_failures.yaml')
		known = cls_verdict.load_manifest(path)
		self.assertTrue(known)
		for k in known:
			self.assertTrue(os.path.isfile(os.path.join('xml_files', k.xml)), k.xml)

	def test_json(self):
		c = cls_verdict.Classifier([kf()], TODAY)
		c.classify('a.xml', '000001', 'Ping', 'Ping', False)
		c.classify('a.xml', '000002', 'Iperf', 'Iperf', True)
		with tempfile.TemporaryDirectory() as d:
			p = os.path.join(d, 'v.json')
			c.write_json(p)
			import json
			j = json.load(open(p))
		self.assertEqual(j['verdict'], 'KNOWN-FAIL')
		self.assertEqual(len(j['steps']), 1)
		self.assertEqual(j['counts']['PASS'], 1)

if __name__ == '__main__':
	unittest.main()
