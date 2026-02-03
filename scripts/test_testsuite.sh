#! /bin/bash

SCRIPT_DIR="$(realpath "$(dirname "$0")")"
# shellcheck source=scripts/test_setup.sh
source "$SCRIPT_DIR/test_setup.sh" 

if [[ $TEST_WITH_COVERAGE = 1 ]]; then
    echo ">> Running test suite with coverage"
    if [[ $TEST_DRYRUN != 1 ]]; then
        set -x
        run_make coverage || add_failure "testsuite with coverage"

        if [[ -n ${CODECOV_TOKEN:-} ]]; then
            bash <(curl -s https://codecov.io/bash) -f '!*/#usr*' -f '!*/^#third_party*' -R "${GITREPO_ROOT}" -s . > /dev/null
        else
            echo "Skipping codecov.io upload"
        fi
        set +x
    fi
else
    echo ">> Running test suite"
    if [[ $TEST_DRYRUN != 1 ]]; then
        set -x
        run_make check || add_failure "testsuite"
        set +x
    fi
fi

collect_failures