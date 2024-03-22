load("//third_party:repositories.bzl", "local_dependencies")
def _non_module_local_deps(_ctx):
    local_dependencies()

non_module_local_deps = module_extension(
    implementation = _non_module_local_deps,
)
