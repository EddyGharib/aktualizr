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

echo ">> Running make docs"
if [[ $TEST_DRYRUN != 1 ]]; then
    set -x
    run_make docs || add_failure "make docs"
    set +x
fi

echo ">> Building and installing"
if [[ $TEST_DRYRUN != 1 ]]; then
    set -x
    run_make || add_fatal_failure "make"

    # Check that 'make install' works
    DESTDIR=/tmp/aktualizr run_make install || add_failure "make install"
    set +x
fi

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