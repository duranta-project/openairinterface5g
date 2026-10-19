# SPDX-License-Identifier: MIT
"""
Build or serve the documentation site with Zensical.

Both commands first prepare the documentation (see staging.py), then run
Zensical on it. "serve" also prepares edited documents again while Zensical
serves the site, so the preview updates without a restart.
"""

import importlib
import logging
import os
import subprocess
import sys
import time

from . import labels, nav, repo, staging, transforms

# Interval at which "serve" looks for changed documents, in seconds.
POLL_INTERVAL = 1.0
PACKAGE_DIR = os.path.dirname(os.path.abspath(__file__))


def build(strict):
  """Prepare the documentation and build the static site; returns Zensical's exit code."""
  staging.prepare()
  return subprocess.call(_zensical("build", *(["--strict"] if strict else [])), cwd=repo.REPO_ROOT)


def serve(addr):
  """Prepare the documentation, serve it, and prepare every change again until interrupted."""
  state = staging.prepare()
  server = subprocess.Popen(_zensical("serve", "-a", addr), cwd=repo.REPO_ROOT)
  code_mtime = _code_mtime()
  try:
    while server.poll() is None:
      time.sleep(POLL_INTERVAL)
      # Edits to the docs_site modules apply without a restart: reload them and
      # prepare everything again (edits to this module need a restart).
      if _code_mtime() != code_mtime:
        code_mtime = _code_mtime()
        for module in (repo, nav, transforms, labels, staging):
          importlib.reload(module)
        state = {}
      try:
        state = staging.sync(state)
      except Exception:  # keep serving if a document or an edit to docs_site is broken
        logging.exception("Could not prepare the documentation")
  except KeyboardInterrupt:
    pass
  finally:
    server.terminate()
    server.wait()
  return 0


def _zensical(command, *args):
  """Command line of Zensical, run with the same Python as run.py."""
  return [sys.executable, "-m", "zensical", command, "-f", staging.CONFIG_FILE, *args]


def _code_mtime():
  """Latest modification time of the docs_site modules."""
  return max(os.path.getmtime(os.path.join(PACKAGE_DIR, f)) for f in os.listdir(PACKAGE_DIR) if f.endswith(".py"))
