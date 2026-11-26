# SPDX-License-Identifier: LicenseRef-CSSL-1.0
"""Exercise cleanup after Docker stop/removal failures."""
import importlib.util
import io
from pathlib import Path
import subprocess
import unittest
from unittest.mock import Mock, patch

spec = importlib.util.spec_from_file_location("run_sib9", Path(__file__).resolve().parents[1] / "run_sib9.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class CleanupTest(unittest.TestCase):
    def setUp(self):
        self.run = runner.Run.__new__(runner.Run)
        self.proc = Mock(pid=123, returncode=0)
        self.proc.poll.return_value = 0
        self.stream = io.StringIO()
        self.run.children = {"ue": (self.proc, self.stream, "sib9-test-ue")}
        self.run.history = [{"role": "ue"}]

    def test_clean_exit(self):
        with patch.object(runner.subprocess, "run") as command:
            self.run.stop("ue")
        self.assertEqual([c.args[0][1] for c in command.call_args_list], ["stop", "rm"])
        self.assertNotIn("ue", self.run.children)
        self.assertEqual(self.run.history[0]["exit_code"], 0)
        self.assertTrue(self.stream.closed)

    def test_stop_timeout_still_removes_container(self):
        failure = subprocess.TimeoutExpired(["docker", "stop"], 20)
        with patch.object(runner.subprocess, "run", side_effect=[failure, Mock()]) as command:
            with self.assertRaises(RuntimeError):
                self.run.stop("ue")
        self.assertEqual([c.args[0][1] for c in command.call_args_list], ["stop", "rm"])
        self.assertNotIn("ue", self.run.children)
        self.assertTrue(self.stream.closed)

    def test_failed_removal_remains_tracked_for_retry(self):
        failure = subprocess.CalledProcessError(1, ["docker", "rm"])
        with patch.object(runner.subprocess, "run", side_effect=[Mock(), failure]):
            with self.assertRaises(RuntimeError):
                self.run.stop("ue")
        self.assertIn("ue", self.run.children)
        with patch.object(runner.subprocess, "run"):
            self.run.stop("ue")
        self.assertNotIn("ue", self.run.children)


if __name__ == "__main__":
    unittest.main()
