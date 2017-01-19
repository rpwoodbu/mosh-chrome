package(default_visibility = ["//visibility:public"])

load("//:nacl_sdk/nacl.bzl", "nacl_manifest_js")

# Labels needed by all variants of the app package.
base_package_srcs = [
        "//mosh_app:mosh_assets",
        "//mosh_app:mosh_css",
        "//mosh_app:mosh_html",
        "//mosh_app:mosh_js",
        "@libapps//:hterm",
]

# Labels needed by all dev variants of the app package.
dev_package_srcs = base_package_srcs + ["//mosh_app:manifest_dev"]

# Builds both the distributable dev and release tracks.
[genrule(
    name = "mosh_chrome" + track,
    srcs = base_package_srcs + [
        ":mosh_chrome_all_architectures_manifest_js",
        "//mosh_app:manifest" + track,
        "//mosh_nacl:mosh_client_all_architectures",
        "//mosh_nacl:mosh_client_manifest_all_architectures",
    ],
    outs = ["mosh_chrome" + track + ".zip"],
    cmd = "/usr/bin/zip --quiet --junk-paths $(OUTS) $(SRCS)",
    message = "Packaging Chrome App",
) for track in ("", "_dev")]

nacl_manifest_js(
    name = "mosh_chrome_all_architectures_manifest_js",
    src = "//mosh_nacl:mosh_client_manifest_all_architectures",
    out = "all_architectures/mosh_manifest.js",
)

# Builds versions suitable for packaging for Windows.
# TODO: Build seperate ones for x64 and ia32 to reduce the size.
genrule(
    name = "mosh_chrome_windows",
    srcs = base_package_srcs + [
        ":mosh_chrome_all_architectures_manifest_js",
        "//mosh_app:manifest_windows",
        "//mosh_nacl:mosh_client_all_architectures",
        "//mosh_nacl:mosh_client_manifest_all_architectures",
    ],
    outs = ["mosh_chrome_windows.zip"],
    cmd = "/usr/bin/zip --quiet --junk-paths $(OUTS) $(SRCS)",
    message = "Packaging Windows Chrome App",
)

# This is a convenience rule which only builds the x86_64 translation, saving
# build time while testing.
genrule(
    name = "mosh_chrome_dev_x86_64",
    srcs = dev_package_srcs + [
        ":mosh_chrome_x86_64_manifest_js",
        "//mosh_nacl:mosh_client_manifest_x86_64",
        "//mosh_nacl:mosh_client_x86_64",
    ],
    outs = ["mosh_chrome_x86_64.zip"],
    cmd = "/usr/bin/zip --quiet --junk-paths $(OUTS) $(SRCS)",
    message = "Packaging x86_64-only Chrome App",
)

nacl_manifest_js(
    name = "mosh_chrome_x86_64_manifest_js",
    src = "//mosh_nacl:mosh_client_manifest_x86_64",
    out = "x86_64/mosh_manifest.js",
)

# This is a convenience rule which only builds a package without any
# translations, saving time while building, but pushing the burden onto the
# client.
#
# NB: There have been issues with the translator that comes with Chrome OS on
#     armv7. Sometimes the binary crashes. Not sure if this is fixed yet.
genrule(
    name = "mosh_chrome_dev_pexe",
    srcs = dev_package_srcs + [
        ":mosh_chrome_pexe_manifest_js",
        "//mosh_nacl:mosh_client_manifest_pexe",
        "//mosh_nacl:mosh_client_pexe",
    ],
    outs = ["mosh_chrome_pexe.zip"],
    cmd = "/usr/bin/zip --quiet --junk-paths $(OUTS) $(SRCS)",
    message = "Packaging Portable Chrome App",
)

nacl_manifest_js(
    name = "mosh_chrome_pexe_manifest_js",
    src = "//mosh_nacl:mosh_client_manifest_pexe",
    out = "pexe/mosh_manifest.js",
)

exports_files(["version.txt"])

#
# The following are rules for the PNaCl and local compiler.
#

cc_toolchain_suite(
    name = "toolchain",
    toolchains = {
        "pnacl|clang": ":cc-compiler-pnacl",
    },
)

cc_toolchain(
    name = "cc-compiler-pnacl",
    all_files = "@nacl_sdk//:pnacl_all_files",
    compiler_files = "@nacl_sdk//:pnacl_toolchain",
    cpu = "pnacl",
    dwp_files = ":empty",
    dynamic_runtime_libs = [":empty"],
    linker_files = "@nacl_sdk//:pnacl_toolchain",
    objcopy_files = ":empty",
    static_runtime_libs = [":empty"],
    strip_files = "@nacl_sdk//:pnacl_strip",
    supports_param_files = 1,
)

# This rule is copied from:
#   https://github.com/bazelbuild/bazel.git
#   bazel/tools/cpp/CROSSTOOL
#   commit 8240b748391d85d68b5ddc502d30f3a2391ba343
cc_toolchain(
    name = "cc-compiler-local",
    all_files = ":empty",
    compiler_files = ":empty",
    cpu = "local",
    dwp_files = ":empty",
    dynamic_runtime_libs = [":empty"],
    linker_files = ":empty",
    objcopy_files = ":empty",
    static_runtime_libs = [":empty"],
    strip_files = ":empty",
    supports_param_files = 1,
)

filegroup(
    name = "empty",
    srcs = [],
)
