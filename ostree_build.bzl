load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _ostree_impl_(module_ctx):
    http_archive(
        name = "ostree_src",
        url = "https://github.com/ostreedev/ostree/releases/download/v2025.7/libostree-2025.7.tar.xz",
        strip_prefix = "libostree-2025.7",
        build_file_content = """
load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all_srcs",
    srcs = glob(["**"], exclude = ["BUILD.bazel"]),
)

configure_make(
    name = "libostree",
    lib_source = ":all_srcs",
    env = {"CFLAGS": "-Wno-error=missing-prototypes"},
    configure_options = [
        "--with-libarchive",
        "--disable-gtk-doc",
        "--disable-gtk-doc-html",
        "--disable-gtk-doc-pdf",
        "--disable-man",
        "--with-builtin-grub2-mkconfig",
        "--with-curl",
        "--without-soup",
        "--prefix=/usr",
    ],
    out_shared_libs = ["libostree-1.so"],
    out_include_dir = "include",
    visibility = ["//visibility:public"],
)
""",
    )

ostree_build = module_extension(
    implementation = _ostree_impl_,
)