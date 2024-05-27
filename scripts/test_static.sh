#! /bin/bash

SCRIPT_DIR="$(realpath $(dirname $0))"
source "$SCRIPT_DIR/test_setup.sh" 

echo ">> Running static checks"
if [[ $TEST_DRYRUN != 1 ]]; then
    set -x
    run_make check-format -k 0 || add_failure "formatting"
    run_make clang-tidy -k 0 || add_failure "static checks"
    set +x
fi

collect_failures