#! /bin/bash

SCRIPT_DIR="$(realpath "$(dirname "$0")")"
# shellcheck source=scripts/test_setup.sh
source "$SCRIPT_DIR/test_setup.sh" 

echo ">> Running static checks"
if [[ $TEST_DRYRUN != 1 ]]; then
    set -x
    # run_make check-format -k 0 || add_failure "formatting"
    run_make clang-tidy -k 0 || add_failure "static checks"
    run_make cppcheck -k 0 || add_failure "cppcheck"
    run_make shellcheck -k 0 || add_failure "shellcheck"
    run_make cspell -k 0 || add_failure "spell checks"

    set +x
fi

collect_failures