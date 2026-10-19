# SPDX-License-Identifier: MIT
"""
Navigation bar, generated from doc/README.md (NAV_SOURCE).

Rules:
- "## X" starts a top-level section, "### Y" a sub-section of the last "##".
- A bullet with links adds one nav entry per link (pages, or external URLs).
  Its sub-bullets only describe the page and are not added.
- A bullet without a link but with linked sub-bullets becomes a sub-section
  (e.g. "Component tests (in tests/):").
- Text outside bullets is ignored, and sections without any page are dropped.
"""

import os
import posixpath
import re

from .repo import LINK_RE, NAV_SOURCE, PAGE_EXT, REPO_ROOT, is_external, resolve, strip_md

# List item: groups are (indentation, item text).
BULLET_RE = re.compile(r"^(\s*)[-*+]\s+(.*)$")
# Level 2 or 3 heading: groups are ("##" or "###", heading text).
HEADING_RE = re.compile(r"^(#{2,3})\s+(.*?)\s*#*$")


def build_nav():
  """Build the site `nav` setting (nested list of {title: path or children}) from NAV_SOURCE."""
  with open(os.path.join(REPO_ROOT, NAV_SOURCE), encoding="utf-8") as f:
    lines = f.read().splitlines()

  root = [{"Overview": NAV_SOURCE}]
  section = root  # children list of the current heading
  h2 = None  # last "##" node, parent of following "###" headings
  stack = []  # open bullets as (indent, children list), to attach nested bullets
  for line in lines:
    m = HEADING_RE.match(line)
    if m:
      node = {m.group(2).strip(): []}
      if len(m.group(1)) == 2:
        root.append(node)
        h2 = node
      else:
        (list(h2.values())[0] if h2 else root).append(node)
      section = list(node.values())[0]
      stack = []
      continue
    m = BULLET_RE.match(line)
    if not m:
      continue
    indent, text = len(m.group(1).expandtabs(4)), m.group(2)
    # Close bullets at the same or deeper indentation: the parent is the closest less indented one.
    while stack and stack[-1][0] >= indent:
      stack.pop()
    parent = stack[-1][1] if stack else section
    entries = _nav_entries(text)
    if entries:
      parent.extend(entries)
      stack.append((indent, []))  # sub-bullets of a page go to a discarded list
    else:
      children = []
      parent.append({_bullet_label(text): children})
      stack.append((indent, children))
  return _prune(root)


def _nav_entries(text):
  """Nav entries ({title: path or URL}) for the links found in one bullet of NAV_SOURCE."""
  entries = []
  for _, label, target in LINK_RE.findall(text):
    if is_external(target):
      if target.startswith("http"):
        entries.append({strip_md(label): target})
      continue
    path = resolve(NAV_SOURCE, target)
    # Only markdown pages go in the nav; links to sources, .txt files or missing files are skipped.
    if not path.lower().endswith(PAGE_EXT) or not os.path.isfile(os.path.join(REPO_ROOT, path)):
      continue
    label = strip_md(label)
    # A file name as link text (e.g. "[BUILD.md](./BUILD.md)") is replaced by the page title.
    if not label or label.lower().endswith(".md"):
      label = _page_title(path)
    entries.append({label: path})
  return entries


def _page_title(path):
  """
  Title of a page for the navigation bar.

  Use the H1 heading if the page has exactly one (headings inside code blocks
  are ignored, e.g. shell comments). Otherwise use the file name, or the
  directory name for a README.md (e.g. radio/USRP/README.md -> "USRP").
  """
  h1, fenced = [], False
  try:
    with open(os.path.join(REPO_ROOT, path), encoding="utf-8") as f:
      for line in f:
        if line.lstrip().startswith(("```", "~~~")):
          fenced = not fenced
        elif not fenced and line.startswith("# "):
          h1.append(line[2:].strip())
  except OSError:
    pass
  if len(h1) == 1:
    return strip_md(h1[0])
  name = posixpath.basename(path)
  if name.lower() == "readme.md":
    return posixpath.basename(posixpath.dirname(path))
  return posixpath.splitext(name)[0]


def _bullet_label(text):
  """Sub-section title from a bullet without link: its first sentence, without trailing colon."""
  label = strip_md(text).rstrip(":")
  return re.split(r"\.\s", label, maxsplit=1)[0]


def _prune(items):
  """Recursively drop empty sections (headings or bullets without any linked page)."""
  result = []
  for item in items:
    (title, value), = item.items()
    if isinstance(value, list):
      value = _prune(value)
      if not value:
        continue
    result.append({title: value})
  return result
