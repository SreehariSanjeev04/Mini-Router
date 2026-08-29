#!/usr/bin/env bash
#
# Aggregate test runner for the mini-router.
#
# Structure:
#   tests/run-all.sh          this runner (discover + summarize)
#   tests/lib/common.sh       shared helpers (topology helpers, raw ARP probe)
#   tests/test-*.sh           one integration test per file (any*'s feature)
#
# Discovers every tests/test-*.sh script, runs it, classifies the result, and
# prints a colored Passed / Failed / Skipped summary. A test exits 0 on pass,
# 1 on failure (logs printed), and 2 when it was skipped (e.g. needs root).
#
# Usage:
#   tests/run-all.sh            # run every discovered test
#   NO_COLOR=1 tests/run-all.sh # force plain output

set -u

TESTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    C_RESET=$'\033[0m'
    C_BOLD=$'\033[1m'
    C_GREEN=$'\033[32m'
    C_RED=$'\033[31m'
    C_YELLOW=$'\033[33m'
else
    C_RESET=''; C_BOLD=''; C_GREEN=''; C_RED=''; C_YELLOW=''
fi

LOGDIR="$(mktemp -d)"
trap 'rm -rf "$LOGDIR"' EXIT

mapfile -t tests < <(find "$TESTS_DIR" -maxdepth 1 -name 'test-*.sh' -type f | sort)

if [ "${#tests[@]}" -eq 0 ]; then
    echo "No tests found in $TESTS_DIR" >&2
    exit 1
fi

passed=0
failed=0
skipped=0
declare -a failed_names
failing_logs=0

printf '%s\n' "${C_BOLD}================ Running ${#tests[@]} test(s) ================${C_RESET}"

for t in "${tests[@]}"; do
    name="$(basename "$t")"
    printf '== %s\n' "$name"
    log="$LOGDIR/$name.log"
    if "$t" >"$log" 2>&1; then
        printf '   %s%s%s\n' "${C_GREEN}PASSED${C_RESET}" "${C_BOLD}  $name${C_RESET}" ""
        passed=$((passed + 1))
    else
        rc=$?
        if [ "$rc" -eq 2 ]; then
            printf '   %s%s%s\n' "${C_YELLOW}SKIPPED${C_RESET}" "${C_BOLD}  $name${C_RESET}" ""
            skipped=$((skipped + 1))
        else
            printf '   %s%s%s\n' "${C_RED}FAILED${C_RESET}" "${C_BOLD}  $name${C_RESET}" ""
            failed=$((failed + 1))
            failed_names+=("$name")
            printf '   %s\n' "${C_RED}--- tail of $log ---${C_RESET}"
            tail -15 "$log" | sed 's/^/     /'
            failing_logs=1
        fi
    fi
done

printf '%s\n' "${C_BOLD}============================================================${C_RESET}"
printf '  %s: %d\n' "${C_GREEN}Passed${C_RESET}"  "$passed"
printf '  %s: %d\n' "${C_RED}Failed${C_RESET}"   "$failed"
printf '  %s: %d\n' "${C_YELLOW}Skipped${C_RESET}" "$skipped"

if [ "$failed" -eq 0 ]; then
    printf '%s\n' "${C_BOLD}Result: ${C_GREEN}SUCCESS${C_RESET}"
    [ "$skipped" -gt 0 ] && printf '%s\n' "${C_YELLOW}($skipped skipped -- re-run with sudo to execute them)${C_RESET}"
else
    printf 'Failed: %s' "${failed_names[*]}" >&2
    printf '\n%s\n' "${C_BOLD}Result: ${C_RED}FAILURE${C_RESET}"
    exit 1
fi