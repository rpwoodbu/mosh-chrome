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

filegroup(
    name = "toolchain",
    srcs = [
        ":cc-compiler-pnacl",
        # Also include local compiler for building tools. (Not sure if this works, though.)
        #"@bazel_tools//tools/cpp:cc-compiler-local",
    ],
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
