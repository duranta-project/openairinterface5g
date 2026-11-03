<!-- SPDX-License-Identifier: CC-BY-4.0 -->

In this directory and subdirectories are various CI code test cases and helper
scripts.

# Unit test

This repository contains unit tests. To run them, switch to the parent
directory, i.e., `ci-scripts/`, and run

    python3 -m unittest tests/*.py

To run individual unit tests, start them like so:

    python tests/analysis.py -v
    python tests/build.py -v
    python tests/cmd.py -v
    python tests/corenetwork.py -v
    python tests/deployment.py -v
    python tests/iperf-analysis.py -v
    python tests/log-analysis.py -v
    python tests/ping-iperf.py -v
    python tests/pull-clean-int-registry.py -v
    python tests/script-deployment.py -v
    python tests/verdict.py -v

The logs will indicate if all tests passed. `tests/deployment.py` requires
these images to be present:

- `oai-ci/oai-nr-ue:develop-12345678`
- `oai-ci/oai-gnb:develop-12345678`

It will try to download `oaisoftwarealliance/oai-{gnb,nr-ue}:develop`
automatically and retag the images.

# test-runner test

This is not a true test, because the results need to be manually inspected. To
run this "test", run

    ./run.sh

inside the `test-runner/` directory (important!).
