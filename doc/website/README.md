<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Documentation Website for openairinterface5g (Zensical)

The Duranta openairinterface5g documentation is written in Markdown
and maintained throughout the repository, from `doc/` to README files
alongside the source code in directories such as `radio/`, `common/`, and `openair1/`.

This directory brings the documentation together into a single, searchable website
using [Zensical](https://zensical.org/), the successor of
[Material for MkDocs](https://squidfunk.github.io/mkdocs-material/), customized
with the Duranta branding. The site is configured in `zensical.toml` at the
repository root.

The original Markdown files remain unchanged, preserving their readability and
rendering on GitHub while making the documentation accessible through a unified website.

## Prerequisites

- [Python 3.11 or newer](https://www.python.org/downloads/)
- [pip](https://pypi.org/project/pip/)
- [Git](https://git-scm.com/) and a Git clone of the repository: the build
  uses Git to find the documentation files

## Virtual environment (optional)

To keep the documentation tools separate from your system Python, create and
activate a virtual environment from the repository root before the Quick start:

```bash
python3 -m venv .venv
source .venv/bin/activate
```

All Quick start commands then run inside it. When finished, leave it with
`deactivate`.

The `.venv/` directory is local to your environment and must not be committed
to Git. The site build skips hidden directories, so it is not published either.

## Quick start

From the repository root, install the dependencies:

```bash
pip install -r doc/website/requirements.txt
```

Start the development server to preview the documentation locally.
The site rebuilds automatically when files change.

```bash
python3 doc/website/run.py serve
```

Open [http://127.0.0.1:8000/](http://127.0.0.1:8000/) to view the site.

To use a different port for the preview, replace `<PORT_NUMBER>` with your preferred port (e.g., `8005`):

```bash
python3 doc/website/run.py serve -a 127.0.0.1:<PORT_NUMBER>
```

To build the static site for deployment, run:

```bash
python3 doc/website/run.py build
```

The generated site is written to `build/website/`.
To serve the generated static files locally, run:

```bash
python3 -m http.server 8000 --directory build/website
```

For more details, run `python3 doc/website/run.py --help`.

## Validation

Before submitting a pull request, run a strict build to check for warnings and
errors:

```bash
python3 doc/website/run.py build --strict
```

The strict build fails if the build reports any warning, such as a link to a
missing file or to a missing section (`page.md#section`). This catches broken
documentation before it is merged.

Unlike `serve`, which only prints warnings and keeps serving the site,
`build --strict` stops with an error code, so it can also be used in CI.

## Files and directories

```text
.
├── zensical.toml                           # Site configuration, theme, and Markdown extensions
└── doc/
    └── website/
        ├── README.md                       # Overview and usage instructions
        ├── requirements.txt                # Pinned Python dependencies
        ├── run.py                          # Command line: build or serve the site
        ├── docs_site/                      # Python modules for documentation site logic
        │   ├── __init__.py                 # Package overview
        │   ├── repo.py                     # Repository paths and documentation file discovery
        │   ├── nav.py                      # Navigation generation from doc/README.md
        │   ├── transforms.py               # Markdown transformations for the site
        │   ├── labels.py                   # GitHub label badges and colors
        │   ├── staging.py                  # Documentation and configuration prepared for Zensical
        │   └── server.py                   # Builds or serves the site with Zensical
        └── overrides/                      # Custom theme templates and styles
            ├── assets/
            │   └── stylesheets/
            │       └── duranta-theme.css   # Duranta colors and layout
            └── partials/
                └── copyright.html          # Custom footer with logo and issue link
```

> [!NOTE]
> `run.py` stores generated files under `build/`, which is ignored by Git.
> The `build/docs-src/` directory contains processed copies of the documentation.
> Edit the original files in the repository, as these copies are regenerated on each build.

## Troubleshooting

- **Port already in use** (`Address already in use`): another server is
  running on that port; pick another one with
  `python3 doc/website/run.py serve -a 127.0.0.1:8005`.
- **Missing dependencies** (`ModuleNotFoundError`, e.g.
  `No module named 'zensical'`): activate the virtual environment, or install them
  with `pip install -r doc/website/requirements.txt` (see
  [Quick start](#quick-start)).
- **Build errors**: check the terminal output for invalid configuration in
  `zensical.toml`, missing files, or errors in `run.py` and the `docs_site/`
  modules.
- **`Could not fetch GitHub label colors`**: harmless. It happens offline or
  when the GitHub API rate limit is reached. The label pills (e.g. in
  `doc/TESTBenches.md`) then use the last colors cached in
  `build/website-cache/`, or a default color. To raise the rate limit, run with
  a token: `GITHUB_TOKEN=<token> python3 doc/website/run.py serve`.
