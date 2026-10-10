#!/usr/bin/env bash
# On CI failure: surface the important lines of a build log as GitHub annotations
# (readable through the REST API) and push the full log to the `ci-logs` branch.
# Usage: ci-report-failure.sh <log-file> [label]
set -uo pipefail
LOG="${1:?log file}"
LABEL="${2:-build}"
[[ -f "$LOG" ]] || { echo "::error title=$LABEL::no log file at $LOG"; exit 0; }

enc() { sed -e 's/%/%25/g' -e 's/\r/%0D/g' | awk 'BEGIN{ORS="%0A"}{print}'; }

# 1) Key error lines (with a little context), capped to stay under annotation limits.
errs=$(grep -nE -B2 -A6 "CMake Error|CMake Warning \(dev\) at|error:|Error [0-9]+|FAILED:|undefined reference|No such file|Could NOT find|not found" "$LOG" | head -c 60000)
[[ -n "$errs" ]] && echo "::error title=$LABEL errors::$(printf '%s\n' "$errs" | enc)"

# 2) Tail of the log.
echo "::error title=$LABEL tail::$(tail -n 120 "$LOG" | head -c 60000 | enc)"

# 3) Full log to the ci-logs branch (needs `permissions: contents: write`).
if [[ -n "${GITHUB_TOKEN:-}" && -n "${GITHUB_REPOSITORY:-}" ]]; then
  tmp=$(mktemp -d)
  git -C "$tmp" init -q -b ci-logs
  name="${GITHUB_RUN_ID:-local}-${LABEL}.log"
  cp "$LOG" "$tmp/$name"
  [[ -f build/engine/CMakeFiles/CMakeConfigureLog.yaml ]] && cp build/engine/CMakeFiles/CMakeConfigureLog.yaml "$tmp/${GITHUB_RUN_ID:-local}-configure.yaml"
  git -C "$tmp" add -A
  git -C "$tmp" -c user.name=ci -c user.email=ci@localhost commit -qm "logs for run ${GITHUB_RUN_ID:-local} ($LABEL)"
  git -C "$tmp" push -qf "https://x-access-token:${GITHUB_TOKEN}@github.com/${GITHUB_REPOSITORY}.git" ci-logs \
    && echo "Full log pushed to branch ci-logs as $name"
fi
exit 0
