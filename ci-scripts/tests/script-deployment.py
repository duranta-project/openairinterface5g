# SPDX-License-Identifier: LicenseRef-CSSL-1.0

import sys
import logging
logging.basicConfig(
	level=logging.DEBUG,
	stream=sys.stdout,
	format="[%(asctime)s] %(levelname)8s: %(message)s"
)
import os
import tempfile

import unittest

sys.path.append('./') # to find OAI imports below
import cls_oaicitest
import cls_oai_html
import cls_cmd
from cls_ci_helper import TestCaseCtx

class TestScriptDeployment(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.html = cls_oai_html.HTMLManagement()
        self.node = 'localhost'
        self.tag = 'develop-12345678'
        self.ctx = TestCaseCtx.Default(self.tmpdir)
    def tearDown(self):
        with cls_cmd.LocalCmd() as c:
            c.run(f'rm -rf {self.ctx.logPath}')

    def test_simple_deployment(self):
        script = 'tests/scripts/deploy-with-script.sh'
        options = 'WILL PASS'
        success = cls_oaicitest.Custom_Script(self.html, self.ctx, self.node, script, options)
        self.assertTrue(success)

    def test_simple_deployment_fail(self):
        script = 'tests/scripts/deploy-with-script.sh'
        options = 'WILLFAIL'
        success = cls_oaicitest.Custom_Script(self.html, self.ctx, self.node, script, options)
        self.assertFalse(success)

    def test_simple_undeployment(self):
        script = 'tests/scripts/undeploy-with-script.sh'
        options = '%%log_dir%% WILL PASS'
        success = cls_oaicitest.Custom_Script(self.html, self.ctx, self.node, script, options)
        self.assertTrue(success)
        # verify logs were created
        files = os.listdir(self.tmpdir)
        self.assertIn('112233-log1.txt', files)
        self.assertIn('112233-log2.txt', files)

    def test_simple_undeployment_fail(self):
        script = 'tests/scripts/undeploy-with-script.sh'
        options = '%%log_dir%% WILLFAILL'
        success = cls_oaicitest.Custom_Script(self.html, self.ctx, self.node, script, options)
        self.assertFalse(success)
        # verify logs were created
        files = os.listdir(self.tmpdir)
        self.assertIn('112233-log1.txt', files)
        self.assertIn('112233-log2.txt', files)

if __name__ == '__main__':
    unittest.main()
