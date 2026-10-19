# SPDX-License-Identifier: MIT
"""
Rewrites of a page's markdown, so that pages written for GitHub (or GitLab)
render correctly on the documentation site.
"""

import os
import re

from .repo import BRANCH, LINK_RE, REPO_ROOT, is_external, resolve

# Table of contents marker line: GitLab "[[_TOC_]]", Python-Markdown "[TOC]".
TOC_MARKER_RE = re.compile(r"^\s*(\[\[_TOC_\]\]|\[TOC\])\s*$")
# "**Table of Contents**" label line, written above a marker.
TOC_LABEL_RE = re.compile(r"^\s*\*\*Table of Contents\*\*\s*$", re.IGNORECASE)

# First line of a GitHub alert: groups are (indentation, alert type).
ALERT_RE = re.compile(r"^(\s*)>\s*\[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\]\s*$", re.IGNORECASE)
# Admonition (box type and title) of the theme used for each GitHub alert type.
ALERT_ADMONITION = {
  "NOTE": 'note "Note"',
  "TIP": 'tip "Tip"',
  "IMPORTANT": 'info "Important"',
  "WARNING": 'warning "Warning"',
  "CAUTION": 'danger "Caution"',
}


def remove_toc_markers(markdown):
  """
  Remove table of contents markers, which the site would show as plain text:
  The theme already shows the table of contents of each page on the right.

  A "**Table of Contents**" label is removed too when a marker follows it. A
  label above a hand-written list is kept, and so is anything in code blocks.
  """
  lines, out, fenced = markdown.split("\n"), [], False
  for i, line in enumerate(lines):
    if line.lstrip().startswith(("```", "~~~")):
      fenced = not fenced
    if not fenced:
      if TOC_MARKER_RE.match(line):
        continue
      if TOC_LABEL_RE.match(line):
        following = next((l for l in lines[i + 1:] if l.strip()), "")
        if TOC_MARKER_RE.match(following):
          continue
    out.append(line)
  return "\n".join(out)


def github_alerts(markdown):
  """
  Convert GitHub alerts to admonitions (boxes) of the theme, so that both GitHub and the
  site render them as boxes:

    > [!NOTE]                 !!! note "Note"
    > Some text.       ->         Some text.

  Alerts inside code blocks are left as is.
  """
  lines, out, fenced, i = markdown.split("\n"), [], False, 0
  while i < len(lines):
    line = lines[i]
    if line.lstrip().startswith(("```", "~~~")):
      fenced = not fenced
    m = None if fenced else ALERT_RE.match(line)
    if not m:
      out.append(line)
      i += 1
      continue
    indent = m.group(1)
    out.append(f"{indent}!!! {ALERT_ADMONITION[m.group(2).upper()]}")
    i += 1
    # The alert body is the following quoted lines: unquote and indent them.
    while i < len(lines) and lines[i].startswith(indent + ">"):
      body = lines[i][len(indent) + 1:]
      body = body[1:] if body.startswith(" ") else body
      out.append(f"{indent}    {body}" if body.strip() else "")
      i += 1
    # The admonition ends at the first line that is not indented: keep a blank line after it.
    if i < len(lines) and lines[i].strip():
      out.append("")
  return "\n".join(out)


def link_unpublished_files(markdown, src, published, repo_url):
  """
  Point relative links to repository files that are not published on the site
  (sources, configs, directories) to GitHub, where they would otherwise be
  broken. src is the page's repo-relative path, published the set of
  repo-relative paths of the published pages and images.
  """
  blob = f"{repo_url.rstrip('/')}/blob/{BRANCH}"
  tree = f"{repo_url.rstrip('/')}/tree/{BRANCH}"

  def rewrite(m):
    bang, label, target = m.groups()
    if is_external(target):
      return m.group(0)
    path = resolve(src, target)
    if path in published:
      return m.group(0)  # published page or image: the site generator handles it
    full = os.path.join(REPO_ROOT, path)
    if os.path.isdir(full):
      return f"{bang}[{label}]({tree}/{path})"
    if os.path.isfile(full):
      anchor = target[len(target.split("#", 1)[0]):]  # keep e.g. "#L91" line anchors
      return f"{bang}[{label}]({blob}/{path}{anchor})"
    return m.group(0)  # target does not exist: leave it so the build reports the broken link

  return LINK_RE.sub(rewrite, markdown)
