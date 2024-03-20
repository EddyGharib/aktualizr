cc_library(
    name = "campaign",
    srcs = ["campaign.cc"],
    visibility = ["//visibility:public"],
    deps = [
        "@jsoncpp",
        "@boost.filesystem",
        "@boost.log",
        ]
)

cc_test(
    name = "campaign_test",
    srcs = ["campaign_test.cc"],
    visibility = ["//visibility:public"],
    deps = [":campaign"]
)