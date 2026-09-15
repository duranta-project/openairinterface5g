#!/usr/bin/env python3
"""Map a git commit's changed files to the CMake executables they affect.

Requires one or more CMake build directories configured with the File API
codemodel-v2 query already in place, e.g.:

    mkdir -p build/.cmake/api/v1/query
    touch build/.cmake/api/v1/query/codemodel-v2
    cmake -S . -B build -DENABLE_TESTS=ON

A target only exists in the codemodel if the CMake options used to configure
that build directory actually enable it (e.g. nr-oru needs
-DOAI_RU_FRONTHAUL=ON). Pass --build-dir multiple times to union the results
across build directories configured with different option sets, so
option-gated executables aren't silently missed.

Some dependencies are invisible to CMake entirely: radio drivers and a few
codec/DFT MODULE libraries are dlopen'd at runtime (via shlib_loader) rather
than linked, so no target_link_libraries/add_dependencies edge connects them
to the executables that load them. commit_impact_manual_deps.json (next to
this script, or --manual-deps) fills that gap with hand-maintained path-prefix
-> executables rules, unioned in regardless of what any build dir reports.

Usage:
    ci-scripts/commit_impact.py                              # diff HEAD vs its parent
    ci-scripts/commit_impact.py <sha>                         # diff <sha> vs its parent
    ci-scripts/commit_impact.py <sha1>..<sha2>                # diff a range
    ci-scripts/commit_impact.py working                       # uncommitted changes
    ci-scripts/commit_impact.py staged                        # staged changes only
    ci-scripts/commit_impact.py <sha> --build-dir build-default --build-dir build-fronthaul
    ci-scripts/commit_impact.py <sha> --json > impact.json
"""
import argparse
import json
import subprocess
import sys
from pathlib import Path


def sh(cmd):
    return subprocess.check_output(cmd, text=True).strip()


def changed_files(rng, source_root):
    git = ["git", "-C", source_root]
    if rng == "working":
        out = sh(git + ["diff", "--name-only", "HEAD"])
    elif rng == "staged":
        out = sh(git + ["diff", "--name-only", "--cached"])
    elif ".." in rng:
        out = sh(git + ["diff", "--name-only", rng])
    else:
        try:
            out = sh(git + ["diff", "--name-only", f"{rng}^", rng])
        except subprocess.CalledProcessError:
            # root commit (no parent)
            out = sh(git + ["show", "--name-only", "--pretty=format:", rng])
    return [c for c in out.splitlines() if c]


def load_targets(reply_dir):
    codemodel_files = list(reply_dir.glob("codemodel-v2-*.json"))
    if not codemodel_files:
        sys.exit(f"No codemodel-v2 reply found in {reply_dir}.\n"
                  f"Configure cmake with the codemodel-v2 query file first, see --help.")
    codemodel = json.loads(codemodel_files[0].read_text())

    targets = {}  # id -> {name, type, sources:set(), deps:set(id)}
    for cfg in codemodel["configurations"]:
        for t in cfg["targets"]:
            tjson = json.loads((reply_dir / t["jsonFile"]).read_text())
            srcs = {s["path"] for s in tjson.get("sources", []) if not s.get("isGenerated")}
            deps = {d["id"] for d in tjson.get("dependencies", [])}
            targets[t["id"]] = {
                "name": t["name"],
                "type": tjson.get("type"),
                "sources": srcs,
                "deps": deps,
            }
    return targets


def affected_targets(targets, changed_set):
    reverse = {tid: set() for tid in targets}
    for tid, info in targets.items():
        for dep in info["deps"]:
            if dep in reverse:
                reverse[dep].add(tid)

    directly_hit = {}
    for tid, info in targets.items():
        hits = info["sources"] & changed_set
        if hits:
            directly_hit[tid] = hits

    affected = set()
    stack = list(directly_hit.keys())
    while stack:
        tid = stack.pop()
        if tid in affected:
            continue
        affected.add(tid)
        stack.extend(reverse.get(tid, ()))

    return directly_hit, affected


def load_manual_rules(path):
    if path is None or not Path(path).exists():
        return []
    data = json.loads(Path(path).read_text())
    return data.get("rules", [])


def apply_manual_rules(changed, rules):
    """Return {rule_name: {"files": [...], "executables": [...]}} for rules
    whose path_prefixes matched at least one changed file."""
    hits = {}
    for rule in rules:
        prefixes = rule["path_prefixes"]
        matched = sorted(f for f in changed if any(f.startswith(p) for p in prefixes))
        if matched:
            hits[rule["name"]] = {
                "files": matched,
                "executables": sorted(rule["executables"]),
            }
    return hits


def analyze_build_dir(build_dir, changed_set):
    reply_dir = Path(build_dir).resolve() / ".cmake" / "api" / "v1" / "reply"
    if not reply_dir.exists():
        sys.exit(f"{reply_dir} not found — configure cmake with the codemodel-v2 "
                  f"query file first, see --help.")
    targets = load_targets(reply_dir)
    directly_hit, affected = affected_targets(targets, changed_set)
    return targets, directly_hit, affected


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("commit_range", nargs="?", default="HEAD",
                     help="commit, a..b range, 'working', or 'staged' (default: HEAD)")
    ap.add_argument("--build-dir", action="append", dest="build_dirs",
                     help="CMake build directory (default: build). Repeat to union "
                          "results across build dirs configured with different options.")
    ap.add_argument("--source-root", default=".",
                     help="repo root, for resolving git diffs (default: .)")
    ap.add_argument("--all-targets", action="store_true",
                     help="also print/emit affected non-executable targets (libs/objects)")
    ap.add_argument("--json", action="store_true",
                     help="emit machine-readable JSON instead of text")
    default_manual_deps = Path(__file__).resolve().parent / "commit_impact_manual_deps.json"
    ap.add_argument("--manual-deps", default=str(default_manual_deps),
                     help=f"JSON file of manual path-prefix -> executables rules "
                          f"(default: {default_manual_deps.name} next to this script)")
    ap.add_argument("--no-manual-deps", action="store_true",
                     help="disable manual dependency rules, use only the CMake graph")
    args = ap.parse_args()
    build_dirs = args.build_dirs or ["build"]

    changed = changed_files(args.commit_range, args.source_root)
    if not changed:
        if args.json:
            print(json.dumps({"changed_files": [], "executables": [], "other_targets": []}))
        else:
            print(f"No changed files for '{args.commit_range}'.", file=sys.stderr)
        return
    changed_set = set(changed)

    manual_rules = [] if args.no_manual_deps else load_manual_rules(args.manual_deps)
    manual_hits = apply_manual_rules(changed, manual_rules)
    manual_executables = {e for hit in manual_hits.values() for e in hit["executables"]}

    executables = set(manual_executables)
    other_targets = set()
    per_build_dir = {}  # build_dir -> {"directly_hit": {name: [files]}, "executables": [...]}

    for build_dir in build_dirs:
        targets, directly_hit, affected = analyze_build_dir(build_dir, changed_set)

        directly_hit_named = {
            targets[tid]["name"]: sorted(hits) for tid, hits in directly_hit.items()
        }
        bd_executables = sorted(targets[tid]["name"] for tid in affected
                                 if targets[tid]["type"] == "EXECUTABLE")
        bd_others = sorted(f'{targets[tid]["name"]} ({targets[tid]["type"]})'
                            for tid in affected if targets[tid]["type"] != "EXECUTABLE")

        executables.update(bd_executables)
        other_targets.update(bd_others)
        per_build_dir[build_dir] = {
            "directly_hit": directly_hit_named,
            "executables": bd_executables,
            "other_targets": bd_others,
        }

    executables = sorted(executables)
    other_targets = sorted(other_targets)

    if args.json:
        result = {
            "commit_range": args.commit_range,
            "changed_files": changed,
            "manual_dep_hits": manual_hits,
            "build_dirs": per_build_dir,
            "executables": executables,
        }
        if args.all_targets:
            result["other_targets"] = other_targets
        print(json.dumps(result, indent=2))
        return

    print(f"Changed files ({len(changed)}):")
    for c in changed:
        print(f"  {c}")
    print()

    if manual_hits:
        print(f"Manual dependency rules matched ({len(manual_hits)}):")
        for name, hit in sorted(manual_hits.items()):
            print(f"  {name}: {', '.join(hit['files'])}")
            print(f"    -> {', '.join(hit['executables'])}")
        print()

    if not manual_hits and not any(bd["directly_hit"] for bd in per_build_dir.values()):
        print("No CMake target directly compiles any changed file, and no manual "
              "dependency rule matched (docs/scripts/config only?), in any of:",
              ", ".join(build_dirs))
        return

    for build_dir, bd in per_build_dir.items():
        if not bd["directly_hit"]:
            continue
        print(f"Directly touched targets in {build_dir} ({len(bd['directly_hit'])}):")
        for name, hits in sorted(bd["directly_hit"].items()):
            print(f"  {name}: {', '.join(hits)}")
        print()

    print(f"Affected executables, union across {len(build_dirs)} build dir(s) "
          f"({len(executables)}):")
    for e in executables:
        print(f"  {e}")

    if args.all_targets:
        print()
        print(f"Other affected targets ({len(other_targets)}):")
        for o in other_targets:
            print(f"  {o}")


if __name__ == "__main__":
    main()
