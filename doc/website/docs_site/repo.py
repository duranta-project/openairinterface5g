# SPDX-License-Identifier: MIT
"""Repository paths and settings, and helpers shared by the other modules."""

import os
import posixpath
import re
import subprocess

# Repository root (this file is in doc/website/docs_site/).
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
# Page whose headings and links define the navigation bar.
NAV_SOURCE = "doc/README.md"
# Branch used for links to files browsed on GitHub.
BRANCH = "develop"
# Files published on the site: pages, and images they embed. Both are tuples
# (note the comma in (".md",)): str.endswith() accepts a tuple of suffixes.
PAGE_EXT = (".md",)
ASSET_EXT = (".png", ".jpg", ".jpeg", ".gif", ".svg")

# Markdown inline link or image: groups are ("!" for images, text, target).
LINK_RE = re.compile(r"(!?)\[([^\]]*)\]\(([^)\s]+)\)")


def tracked_files():
  """
  Repo-relative paths of all pages and images known to git.

  Includes new files not committed yet (but not git-ignored ones, e.g. build/),
  and skips files deleted in the working tree, so local edits preview correctly.
  Files in hidden directories (e.g. a .venv/ virtual environment) are skipped.
  """
  cmd = ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"]
  out = subprocess.run(cmd, cwd=REPO_ROOT, check=True, capture_output=True, text=True).stdout
  files = sorted(set(out.split("\0")))
  return [
    f for f in files
    if f.lower().endswith(PAGE_EXT + ASSET_EXT)
    and not any(part.startswith(".") for part in f.split("/")[:-1])
    and os.path.isfile(os.path.join(REPO_ROOT, f))
  ]


def is_external(target):
  """True for links that must not be resolved in the repository: URLs (http:, mailto:, ...), anchors and absolute paths."""
  return re.match(r"^[a-z][a-z0-9+.-]*:", target, re.I) or target.startswith(("#", "/"))


def resolve(base_file, target):
  """Resolve a relative link target found in base_file to a repo-relative path (anchor removed)."""
  path = target.split("#", 1)[0]
  return posixpath.normpath(posixpath.join(posixpath.dirname(base_file), path))


def strip_md(text):
  """Plain text of inline markdown: keep link text, drop backticks and bold/italic markers."""
  text = LINK_RE.sub(r"\2", text).replace("`", "")
  return re.sub(r"(\*\*|__|\*)(.+?)\1", r"\2", text).strip()
