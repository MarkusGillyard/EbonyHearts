#!/usr/bin/env bash
# Runs the headless test suite. Usage: tools/run_tests.sh [file-filter]
# Fails if any test fails or if any script error is printed (a runtime error aborts a test
# function without failing an assertion, so the log is checked as well).
set -uo pipefail
cd "$(dirname "$0")/.."
GODOT="${GODOT:-godot}"
"$GODOT" --headless --import >/dev/null 2>&1 || true
LOG="$(mktemp)"
"$GODOT" --headless --path . res://tests/test_runner.tscn -- --profile=tests "$@" 2>&1 | tee "$LOG"
status=${PIPESTATUS[0]}
if grep -q "SCRIPT ERROR" "$LOG"; then
	echo "Script errors were printed during the run (see above)."
	status=1
fi
rm -f "$LOG"
exit "$status"
