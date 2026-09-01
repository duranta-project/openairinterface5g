#!/usr/bin/env python3
"""
CI helper: given a set of changed files (normally a PR's diff), determine
which specific executables -- from a caller-supplied list -- are affected,
by walking the real Ninja dependency graph of one or more already-configured
CMake/Ninja build directories. Also reports the dependency chain that makes
each affected target affected, for auditability.

Why the Ninja graph and not CMakeLists.txt:
  - CMake options (e.g. --nrUE / --eNB / --RU, phy simulators, unit tests,
    NBIoT, USRP vs no RF driver, ...) change which sources get compiled into
    which targets, and even which targets exist at all. The *build.ninja*
    already encodes the outcome of all of that for one configured tree, so
    reading it is more reliable than re-deriving it from CMake source.
  - build.ninja only has *static* edges (source -> object, object+libs ->
    executable). Header dependencies are tracked dynamically by Ninja in the
    deps log (populated from compiler -MD/-MMD output), so those are read
    separately via `ninja -t deps`.

Because different CMake option sets can produce different graphs (a file may
be compiled into a target under one configuration and be dead code under
another, or a whole target may not exist), pass one build directory per
option set you care about; results are reported per build dir.

Some OAI components (the LDPC decoder, RF/transport drivers, telnet plugins,
...) are loaded at RUNTIME via dlopen rather than linked at build time -- so
even the real Ninja graph has no edge from e.g. libldpc.so to nr-uesoftmodem,
and a change to the LDPC decoder would otherwise be invisible to this tool.
See ci_runtime_dependencies.json (and --extra-dep) to declare those by hand.

Usage (CI):

    git diff --name-only origin/develop...HEAD | \\
        tools/find_affected_executables.py \\
            --build-dir cmake_targets/ran_build/build \\
            --files-from - \\
            --targets nr-softmodem nr-uesoftmodem lte-softmodem \\
            --json

Requirements: each build dir must already be `cmake`+`ninja` configured
(build.ninja present) and must have been built at least once (.ninja_deps
populated) so header dependencies are known. It does not need to be built at
the PR's revision -- building once is enough, since this tool only reasons
about which existing targets *could* be touched by a given file, not about
whether the code still compiles.
"""

import argparse
import glob
import json
import os
import pickle
import re
import subprocess
import sys
from collections import deque

REPO_ROOT = subprocess.run(
    ["git", "rev-parse", "--show-toplevel"], cwd=os.path.dirname(os.path.abspath(__file__)),
    capture_output=True, text=True, check=True,
).stdout.strip()

EXECUTABLE_RULE_RE = re.compile(r"EXECUTABLE_LINKER")
MODULE_RULE_RE = re.compile(r"MODULE_LIBRARY_LINKER|SHARED_LIBRARY_LINKER")
BUILD_LINE_RE = re.compile(r"^build\s+(.+?)\s*:\s*(\S+)(.*)$")
BUILD_SYSTEM_FILE_RE = re.compile(r"(^|/)CMakeLists\.txt$|\.cmake$", re.IGNORECASE)
HEADER_EXTENSIONS = {".h", ".hh", ".hpp", ".hxx", ".h++", ".inc", ".ipp", ".tpp"}


def unescape_ninja(token):
    return token.replace("$ ", " ").replace("$:", ":").replace("$$", "$")


def split_ninja_tokens(text):
    # Placeholder-protect escaped spaces so a plain .split() doesn't cut
    # filenames that legitimately contain a space.
    protected = text.replace("$ ", "\x00")
    return [unescape_ninja(tok.replace("\x00", "$ ")) for tok in protected.split()]


class NinjaGraph:
    def __init__(self, build_dir):
        self.build_dir = os.path.abspath(build_dir)
        # node (path string exactly as it appears in build.ninja) -> set of
        # nodes that directly depend on it (i.e. reverse edges).
        self.reverse = {}
        # absolute, real path -> node string as used in the graph, for every
        # path token we've seen (inputs and outputs).
        self.path_index = {}
        # executable name (basename) -> node string of its linked output.
        self.executables = {}
        # dlopen-able module/shared library name (basename) -> node string.
        # These are NOT statically linked into any executable (that's the
        # whole point of dlopen), so they never show up as ancestors of an
        # executable through normal build edges -- see add_runtime_dependency.
        self.modules = {}
        # (from_node, to_node) pairs added via add_runtime_dependency, kept
        # separately so chain reports can flag which hops are declared
        # runtime/dlopen dependencies rather than real build edges.
        self.synthetic_edges = set()
        self._parse_file(os.path.join(self.build_dir, "build.ninja"), seen=set())

    def _index(self, token):
        node_path = token if os.path.isabs(token) else os.path.join(self.build_dir, token)
        self.path_index[os.path.realpath(node_path)] = token

    def _parse_file(self, path, seen):
        path = os.path.realpath(path)
        if path in seen or not os.path.isfile(path):
            return
        seen.add(path)
        with open(path, "r", errors="surrogateescape") as f:
            raw = f.read()

        # Join ninja line continuations ("...$\n   ...") before splitting.
        raw = raw.replace("$\n", " ")
        for line in raw.split("\n"):
            if not line or line[0] in " \t#":
                continue
            if line.startswith(("subninja ", "include ")):
                ref = line.split(None, 1)[1].strip()
                self._parse_file(os.path.join(self.build_dir, ref), seen)
                continue
            m = BUILD_LINE_RE.match(line)
            if not m:
                continue
            outputs_part, rule, rest = m.groups()

            deps_part, _, order_only = rest.strip().partition(" || ")
            explicit_in, _, implicit_in = deps_part.partition(" | ")
            # Order-only ("||") inputs are build-*ordering* constraints (e.g.
            # "run codegen before this"), not data dependencies -- a target
            # does not actually consume that input's content. Many unrelated
            # executables order-depend on the same shared phony barriers, so
            # treating "||" as a real edge causes massive false-positive
            # fan-out (a change to one header would "affect" nearly every
            # target). They're deliberately excluded from the reverse graph,
            # but still indexed below so path lookups don't miss them.
            order_only_toks = split_ninja_tokens(order_only)
            inputs = split_ninja_tokens(explicit_in) + split_ninja_tokens(implicit_in)

            explicit_out, _, implicit_out = outputs_part.partition(" | ")
            explicit_outputs = split_ninja_tokens(explicit_out)
            outputs = explicit_outputs + split_ninja_tokens(implicit_out)

            for tok in inputs + outputs + order_only_toks:
                self._index(tok)

            for i in inputs:
                self.reverse.setdefault(i, set()).update(outputs)

            if EXECUTABLE_RULE_RE.search(rule):
                for o in explicit_outputs:
                    self.executables[os.path.basename(o)] = o
            elif MODULE_RULE_RE.search(rule):
                for o in explicit_outputs:
                    self.modules[os.path.basename(o)] = o

    def node_for_path(self, abspath):
        return self.path_index.get(os.path.realpath(abspath))

    def add_runtime_dependency(self, source, target_exe):
        """Declare that `target_exe` depends on `source` (a module/shared
        library name, e.g. "libldpc.so") at RUNTIME (dlopen) rather than at
        link time. build.ninja has no edge for this -- CMake only records it
        as an order-only build-ordering hint, which this tool otherwise
        ignores on purpose (see the order-only comment above) -- so it has to
        be supplied out of band. See ci_runtime_dependencies.json.

        Returns a status string: "ok", "unknown-source" (the library isn't
        part of this build config, e.g. its CMake option is OFF), or
        "unknown-target" (the executable isn't part of this build config)."""
        source_node = self.modules.get(source)
        if source_node is None:
            # Not a known module/shared-lib basename -- accept it only if
            # it's already some real node in this build's graph (e.g. a
            # literal build.ninja output path), otherwise the declaration
            # doesn't refer to anything this build config actually produces
            # (a plausible reason: its CMake option is OFF here).
            if source in self.reverse or source in self.path_index.values():
                source_node = source
            else:
                return "unknown-source"
        target_node = self.executables.get(target_exe)
        if target_node is None:
            return "unknown-target"
        self.reverse.setdefault(source_node, set()).add(target_node)
        self.synthetic_edges.add((source_node, target_node))
        return "ok"

    def ancestors_with_parents(self, start_nodes):
        """BFS over reverse edges from start_nodes. Returns (visited, parent)
        where parent maps each reached node to the node it was discovered
        from, letting the caller reconstruct a shortest chain back to one of
        start_nodes for any node in `visited`."""
        visited = set(start_nodes)
        parent = {}
        queue = deque(start_nodes)
        while queue:
            n = queue.popleft()
            for dependent in self.reverse.get(n, ()):
                if dependent not in visited:
                    visited.add(dependent)
                    parent[dependent] = n
                    queue.append(dependent)
        return visited, parent


def reconstruct_chain(parent, start_nodes, target):
    """Shortest node -> ... -> target chain, per the BFS `parent` map."""
    if target in start_nodes:
        return [target]
    if target not in parent:
        return None
    chain = [target]
    cur = target
    while cur not in start_nodes:
        cur = parent[cur]
        chain.append(cur)
    chain.reverse()
    return chain


def load_header_to_objects(build_dir):
    """Reverse index built from `ninja -t deps`: header abspath -> set of
    object-file node strings (relative to build_dir) that #include it,
    directly or transitively. Only available for objects Ninja has actually
    compiled at least once.

    This is the expensive step (the deps log covers every object file in the
    tree), so its result is cached on disk keyed by the deps log's own
    mtime+size -- invalidated automatically the next time the build dir is
    rebuilt."""
    ninja_deps = os.path.join(build_dir, ".ninja_deps")
    cache_path = os.path.join(build_dir, ".find_affected_executables_cache.pkl")
    try:
        st = os.stat(ninja_deps)
        cache_key = (st.st_mtime_ns, st.st_size)
    except FileNotFoundError:
        cache_key = None

    if cache_key is not None and os.path.isfile(cache_path):
        try:
            with open(cache_path, "rb") as f:
                saved_key, saved_data = pickle.load(f)
            if saved_key == cache_key:
                return saved_data
        except Exception:
            pass  # corrupt/incompatible cache -- fall through and rebuild it

    proc = subprocess.run(
        ["ninja", "-t", "deps"], cwd=build_dir,
        capture_output=True, text=True, check=True,
    )
    header_to_objects = {}
    current_obj = None
    for line in proc.stdout.split("\n"):
        if not line:
            current_obj = None
            continue
        if line[0] not in " \t":
            current_obj = line.split(":", 1)[0].strip()
            continue
        if current_obj is None:
            continue
        header = line.strip()
        header_to_objects.setdefault(os.path.realpath(header), set()).add(current_obj)

    if cache_key is not None:
        try:
            with open(cache_path, "wb") as f:
                pickle.dump((cache_key, header_to_objects), f, protocol=pickle.HIGHEST_PROTOCOL)
        except OSError:
            pass  # e.g. read-only build dir -- caching is a pure optimization

    return header_to_objects


def changed_files_from_git(base, head):
    out = subprocess.run(
        ["git", "diff", "--name-only", "--diff-filter=ACMRTUXB", f"{base}...{head}"],
        cwd=REPO_ROOT, capture_output=True, text=True, check=True,
    ).stdout
    return [line.strip() for line in out.split("\n") if line.strip()]


def discover_build_dirs():
    return sorted(
        os.path.dirname(p)
        for p in glob.glob(os.path.join(REPO_ROOT, "cmake_targets", "*", "build", "build.ninja"))
    )


def analyze_build_dir(build_dir, changed_files, targets, runtime_deps):
    """Returns (graph, per_file, target_nodes, dep_diagnostics).

    per_file[rel_path] is one of:
      {"status": "build-system-file"}                          -- affects every target
      {"status": "not-in-graph"}                                -- irrelevant to this build
      {"status": "ok", "ancestors": set, "parent": dict, "start_nodes": set}

    target_nodes maps each requested target present in this build to its
    linked output node. dep_diagnostics lists the requested runtime
    dependencies that didn't resolve in this build config (status != "ok").
    """
    graph = NinjaGraph(build_dir)

    dep_diagnostics = []
    for dep in runtime_deps:
        for exe in dep["to"]:
            status = graph.add_runtime_dependency(dep["from"], exe)
            if status != "ok":
                dep_diagnostics.append({"from": dep["from"], "to": exe, "status": status})

    per_file = {}
    maybe_headers = []
    for rel_path in changed_files:
        abspath = os.path.join(REPO_ROOT, rel_path)
        node = graph.node_for_path(abspath)

        if BUILD_SYSTEM_FILE_RE.search(rel_path):
            per_file[rel_path] = {"status": "build-system-file"}
        elif node is not None:
            per_file[rel_path] = {"status": "pending", "start_nodes": {node}}
        elif os.path.splitext(rel_path)[1].lower() in HEADER_EXTENSIONS:
            # Not a direct node in the static graph, but header-like --
            # only discoverable via Ninja's dynamic deps log, loaded lazily
            # below (it's expensive) only if such a file is actually present.
            maybe_headers.append(rel_path)
        else:
            per_file[rel_path] = {"status": "not-in-graph"}

    if maybe_headers:
        header_to_objects = load_header_to_objects(build_dir)
        for rel_path in maybe_headers:
            abspath = os.path.join(REPO_ROOT, rel_path)
            objs = header_to_objects.get(os.path.realpath(abspath), ())
            start_nodes = {o for o in objs if o in graph.reverse or o in graph.executables.values()}
            per_file[rel_path] = (
                {"status": "pending", "start_nodes": start_nodes} if start_nodes
                else {"status": "not-in-graph"}
            )

    target_nodes = {t: graph.executables[t] for t in targets if t in graph.executables}
    for rel_path, info in per_file.items():
        if info["status"] != "pending":
            continue
        ancestors, parent = graph.ancestors_with_parents(info["start_nodes"])
        info["status"] = "ok"
        info["ancestors"] = ancestors
        info["parent"] = parent

    return graph, per_file, target_nodes, dep_diagnostics


def annotate_synthetic_hops(graph, chain):
    """Maps chain index -> a human-readable note, for any hop that came from
    a declared runtime/dlopen dependency rather than a real build edge."""
    notes = {}
    for i in range(1, len(chain)):
        if (chain[i - 1], chain[i]) in graph.synthetic_edges:
            notes[i] = (f"declared runtime (dlopen) dependency: {chain[i - 1]} -> {chain[i]} "
                        f"-- not a build-time link, see ci_runtime_dependencies.json")
    return notes


def build_target_report(build_dir, graph, per_file, targets, target_nodes):
    report = {}
    for t in targets:
        if t not in target_nodes:
            report[t] = {"present": False, "affected": False, "chains": []}
            continue
        out_node = target_nodes[t]
        chains = []
        for rel_path, info in per_file.items():
            if info["status"] == "build-system-file":
                chains.append({"changed_file": rel_path, "chain": [rel_path, "(build-system file: conservatively affects every target)"], "notes": {}})
                continue
            if info["status"] != "ok" or out_node not in info["ancestors"]:
                continue
            raw_chain = reconstruct_chain(info["parent"], info["start_nodes"], out_node)
            notes = annotate_synthetic_hops(graph, raw_chain)
            chain = [rel_path] + raw_chain[1:]  # use the original repo-relative path as the human-readable head
            chains.append({"changed_file": rel_path, "chain": chain, "notes": {str(i): n for i, n in notes.items()}})
        report[t] = {"present": True, "affected": bool(chains), "chains": chains}
    return report


DEFAULT_RUNTIME_DEPS_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ci_runtime_dependencies.json")


def load_runtime_deps(args):
    """Loads declared runtime (dlopen) dependencies: entries the static
    Ninja graph structurally cannot contain, e.g. that nr-softmodem loads
    libldpc.so at runtime even though it never links it. See
    ci_runtime_dependencies.json and NinjaGraph.add_runtime_dependency."""
    deps = []
    if args.runtime_deps_file:
        path = args.runtime_deps_file
    elif not args.no_runtime_deps and os.path.isfile(DEFAULT_RUNTIME_DEPS_FILE):
        path = DEFAULT_RUNTIME_DEPS_FILE
    else:
        path = None
    if path:
        with open(path) as f:
            deps.extend(json.load(f)["dependencies"])

    for spec in args.extra_dep:
        source, _, tos = spec.partition("=")
        if not tos:
            sys.exit(f"--extra-dep {spec!r} must be of the form SOURCE=EXE1,EXE2")
        deps.append({"from": source, "to": [t for t in tos.split(",") if t]})
    return deps


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build-dir", action="append", default=[],
                     help="A configured+built CMake/Ninja build dir (contains build.ninja). "
                          "May be repeated, once per CMake option set. Default: auto-discover "
                          "every cmake_targets/*/build with a build.ninja.")
    ap.add_argument("--base", default=None, help="Base ref for git diff (default: origin/develop, falling back to develop).")
    ap.add_argument("--head", default="HEAD", help="Head ref for git diff (default: HEAD).")
    ap.add_argument("--files", nargs="+", default=None, help="Explicit list of repo-relative changed files, instead of git diff.")
    ap.add_argument("--files-from", default=None, help="Read changed files (one per line) from this path, or '-' for stdin.")
    ap.add_argument("--targets", nargs="+", required=True,
                     help="Executable name(s) to check (basename of the linked binary, e.g. nr-softmodem).")
    ap.add_argument("--runtime-deps-file", default=None,
                     help="JSON file declaring extra dlopen/runtime dependencies the static build graph "
                          f"can't see (default: {os.path.basename(DEFAULT_RUNTIME_DEPS_FILE)} next to this "
                          "script, if present). Format: {\"dependencies\": [{\"from\": \"libldpc.so\", "
                          "\"to\": [\"nr-softmodem\"]}, ...]}.")
    ap.add_argument("--extra-dep", action="append", default=[],
                     help="One-off runtime dependency: SOURCE=EXE1,EXE2 (e.g. "
                          "libldpc.so=nr-softmodem,nr-uesoftmodem). Repeatable; adds to, doesn't replace, "
                          "the runtime-deps file.")
    ap.add_argument("--no-runtime-deps", action="store_true",
                     help="Don't auto-load the default runtime-deps file (--runtime-deps-file/--extra-dep still apply).")
    ap.add_argument("--json", action="store_true", help="Emit machine-readable JSON instead of a text report.")
    ap.add_argument("--fail-if-affected", action="store_true",
                     help="Exit non-zero if any requested target is affected, in any build dir.")
    args = ap.parse_args()

    runtime_deps = load_runtime_deps(args)

    build_dirs = args.build_dir or discover_build_dirs()
    if not build_dirs:
        sys.exit("No build directories given and none auto-discovered under cmake_targets/*/build. "
                  "Configure+build one with cmake_targets/build_oai first, or pass --build-dir.")
    for d in build_dirs:
        if not os.path.isfile(os.path.join(d, "build.ninja")):
            sys.exit(f"{d}: no build.ninja found (did you run cmake -GNinja there?)")

    if args.files_from is not None:
        text = sys.stdin.read() if args.files_from == "-" else open(args.files_from).read()
        changed_files = [l.strip() for l in text.split("\n") if l.strip()]
    elif args.files is not None:
        changed_files = args.files
    else:
        base = args.base
        if base is None:
            for candidate in ("origin/develop", "develop"):
                probe = subprocess.run(["git", "rev-parse", "--verify", candidate],
                                        cwd=REPO_ROOT, capture_output=True)
                if probe.returncode == 0:
                    base = candidate
                    break
            if base is None:
                sys.exit("Could not find origin/develop or develop; pass --base explicitly.")
        changed_files = changed_files_from_git(base, args.head)

    results = {}
    dep_diagnostics = {}
    for build_dir in build_dirs:
        graph, per_file, target_nodes, dep_diagnostics[build_dir] = analyze_build_dir(
            build_dir, changed_files, args.targets, runtime_deps)
        results[build_dir] = build_target_report(build_dir, graph, per_file, args.targets, target_nodes)

    any_affected = any(t["affected"] for report in results.values() for t in report.values())

    if args.json:
        print(json.dumps({
            "changed_files": changed_files,
            "by_build_dir": results,
            "runtime_dependency_diagnostics": dep_diagnostics,
        }, indent=2))
    else:
        print(f"Changed files ({len(changed_files)}):")
        for f in changed_files:
            print(f"  {f}")
        print()
        for build_dir, report in results.items():
            print(f"=== {build_dir} ===")
            for t, info in report.items():
                if not info["present"]:
                    print(f"  {t}: NOT PRESENT in this build config")
                    continue
                if not info["affected"]:
                    print(f"  {t}: not affected")
                    continue
                print(f"  {t}: AFFECTED")
                for c in info["chains"]:
                    print(f"    via {c['changed_file']}:")
                    for idx, node in enumerate(c["chain"]):
                        line = f"      {node}" if idx == 0 else f"      -> {node}"
                        note = c["notes"].get(str(idx))
                        print(f"{line}   [{note}]" if note else line)

            unresolved_sources = sorted({d["from"] for d in dep_diagnostics[build_dir] if d["status"] == "unknown-source"})
            if unresolved_sources:
                print(f"  note: runtime-dep source(s) not built in this config, so declared dependency ignored here: {', '.join(unresolved_sources)}")
            print()

    if args.fail_if_affected and any_affected:
        sys.exit(1)


if __name__ == "__main__":
    main()
