#!/bin/bash

# This script will be run bazel when building process starts to
# generate key-value information that represents the status of the
# workspace. 
# If the script exits with non-zero code, it's considered as a failure
# and the output will be discarded.

git status

if [[ $? != 0 ]];
then
    git_rev="GIT NOT FOUND"
    tree_status="GIT NOT FOUND"
else
    # The code below presents an implementation that works for git repository
    git_rev=$(git describe --tags)
    if [[ $? != 0 ]];
    then
        exit 1
    fi
    # Check whether there are any uncommitted changes
    git diff-index --quiet HEAD --
    if [[ $? == 0 ]];
    then
        tree_status="Clean"
    else
        tree_status="Modified"
    fi
fi
echo "STABLE_BUILD_SCM_REVISION ${git_rev}"
echo "STABLE_BUILD_SCM_STATUS ${tree_status}"
