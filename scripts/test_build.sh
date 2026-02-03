#! /bin/bash

SCRIPT_DIR="$(realpath "$(dirname "$0")")"
# shellcheck source=scripts/test_setup.sh
source "$SCRIPT_DIR/test_setup.sh" 

echo ">> Building and installing"
if [[ $TEST_DRYRUN != 1 ]]; then
    set -x
    run_make || add_fatal_failure "make"

    # Check that 'make install' works
    DESTDIR=/tmp/aktualizr run_make install || add_failure "make install"
    set +x
fi

collect_failures