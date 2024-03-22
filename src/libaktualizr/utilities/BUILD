genrule(
    name = "version",
    srcs = ["read_stable_keys.sh"],
    outs = ["autogen_version.hpp"],
    cmd_bash = "./$(location read_stable_keys.sh) > $@",
    stamp = 1,
)

cc_library(
    name = "utilities",
    srcs = [
        "aktualizr_version.cc",
        "apiqueue.cc",
        "dequeue_buffer.cc",
        "results.cc",
        "sig_handler.cc",
        "timer.cc",
        "types.cc",
        "utils.cc"
    ],
    hdrs = glob(["*.h"]) + [
        "autogen_version.hpp",
        ],
    copts = ["-Isrc/libaktualizr",],
    visibility = ["//visibility:public"],
    deps = [
        "@jsoncpp",
        "@boost.filesystem",
        "@boost.log",
        "@boost.thread",
        "@libcurl",
        "@libarchive",
        "//:pub_includes"
        ]
)

cc_test(
    name = "api_queue_test",
    srcs = ["api_queue_test.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":utilities",
        "@googletest//:gtest_main",
    ]
)

cc_test(
    name = "dequeue_buffer_test",
    srcs = ["dequeue_buffer_test.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":utilities",
        "@googletest//:gtest_main",
    ]
)