#!/usr/bin/env bash
# Build ran-base and oai-gnb-aerial, tagged with the current git branch.
#
# Usage (from anywhere in the repo):
#   ./build.sh
#
# The tag is derived from the current branch name, with '/' replaced by '-'
# (e.g. feature/foo -> feature-foo). If HEAD is detached (no branch, e.g. a
# commit was checked out directly) it falls back to the short commit SHA. A
# '-dirty' suffix is appended when the working tree has uncommitted changes.
# No manual tag/version to remember or edit.
#
# Dockerfile.gNB.aerial.ubuntu's FROM line references ran-base:latest. Before
# building oai-gnb-aerial, the script patches it to use the branch-tagged
# ran-base image, then restores the original line and removes the ran-base
# image once the build finishes.
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
DOCKERFILE="${REPO_ROOT}/docker/Dockerfile.gNB.aerial.ubuntu"

cd "$REPO_ROOT"

BRANCH_RAW=$(git symbolic-ref --short -q HEAD || git rev-parse --short HEAD)
BRANCH_TAG=$(echo "$BRANCH_RAW" | tr '/' '-')
if [ -n "$(git status --porcelain)" ]; then
  BRANCH_TAG="${BRANCH_TAG}-dirty"
fi

echo "==> Building ran-base:${BRANCH_TAG}"
docker build -f docker/Dockerfile.base.ubuntu . \
  -t ran-base:"${BRANCH_TAG}" \
  --progress=plain

restore_dockerfile() {
  sed -i "s|FROM ran-base:${BRANCH_TAG} AS ran-build|FROM ran-base:latest AS ran-build|" "$DOCKERFILE"
}
trap restore_dockerfile EXIT

echo "==> Patching ${DOCKERFILE} to use ran-base:${BRANCH_TAG}"
sed -i "s|FROM ran-base:latest AS ran-build|FROM ran-base:${BRANCH_TAG} AS ran-build|" "$DOCKERFILE"

echo "==> Building oai-gnb-aerial:${BRANCH_TAG}"
docker build -f docker/Dockerfile.gNB.aerial.ubuntu -t oai-gnb-aerial:"${BRANCH_TAG}" \
  --progress=plain .

echo "==> Removing ran-base:${BRANCH_TAG}"
docker rmi ran-base:"${BRANCH_TAG}"

COMPOSE_DIR="$(git rev-parse --show-toplevel)/targets/PROJECTS/GENERIC-NR-5GC/CONF/configs_multicell"
printf "TAG=%s\nREGISTRY=\n" "${BRANCH_TAG}" > "${COMPOSE_DIR}/.env"
echo "==> Wrote ${COMPOSE_DIR}/.env (TAG=${BRANCH_TAG})"

echo "==> Done. Image:"
echo "    oai-gnb-aerial:${BRANCH_TAG}"
