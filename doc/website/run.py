# SPDX-License-Identifier: MIT
"""
Build or preview the Duranta openairinterface5g documentation site with Zensical.

  python3 doc/website/run.py build [--strict]        static site in build/website/
  python3 doc/website/run.py serve [-a 127.0.0.1:8000] preview, updated on every save

Documentation is distributed throughout the repository, with READMEs in `doc/`
and alongside source code in directories such as `radio/`, `common/`, and
`openair1/`. The docs_site package next to this file does the work (see
docs_site/server.py); this file only provides the command line.
"""

import argparse
import logging
import sys

from docs_site import server


def main():
  logging.basicConfig(level=logging.INFO, format="%(levelname)-8s-  %(message)s")
  parser = argparse.ArgumentParser(description="Build or preview the documentation site with Zensical.")
  commands = parser.add_subparsers(dest="command", required=True)
  build_parser = commands.add_parser("build", help="build the static site in build/website/")
  build_parser.add_argument("-s", "--strict", action="store_true", help="fail on warnings, e.g. broken links")
  serve_parser = commands.add_parser("serve", help="preview the site, updated on every save")
  serve_parser.add_argument("-a", "--dev-addr", default="127.0.0.1:8000", help="address and port (default: %(default)s)")
  args = parser.parse_args()
  if args.command == "build":
    return server.build(args.strict)
  return server.serve(args.dev_addr)


if __name__ == "__main__":
  sys.exit(main())
