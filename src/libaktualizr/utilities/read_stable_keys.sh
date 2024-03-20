#!/bin/bash

git_rev=$(grep -Po '^STABLE_BUILD_SCM_REVISION\s+\K.*' bazel-out/stable-status.txt)
tree_status=$(grep -Po '^STABLE_BUILD_SCM_STATUS\s+\K.*' bazel-out/stable-status.txt)
echo -e "#define AKTUALIZR_VERSION \"${git_rev}\"\n#define BUILD_SCM_STATUS \"${tree_status}\""