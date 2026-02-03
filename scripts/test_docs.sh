#! /bin/bash

SCRIPT_DIR="$(realpath "$(dirname "$0")")"
# shellcheck source=scripts/test_setup.sh
source "$SCRIPT_DIR/test_setup.sh" 

echo ">> Running make docs"
if [[ $TEST_DRYRUN != 1 ]]; then
    set -x
    run_make docs || add_failure "make docs"
    set +x
fi

collect_failures
