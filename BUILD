package(default_visibility = ["//visibility:public"])

[genrule(
    name = "mosh_chrome" + track,
    srcs = [
        "//mosh_app:mosh_assets",
        "//mosh_app:mosh_css",
        "//mosh_app:mosh_html",
        "//mosh_app:mosh_js",
        "//mosh_app:manifest" + track,
        "//mosh_nacl:mosh_client_armv7",
        "//mosh_nacl:mosh_client_i686",
        "//mosh_nacl:mosh_client_manifest",
        "//mosh_nacl:mosh_client_x86_64",
        "@libapps//:hterm",
    ],
    outs = ["mosh_chrome" + track + ".zip"],
    cmd = "/usr/bin/zip --quiet --junk-paths $(OUTS) $(SRCS)",
    message = "Packaging Chrome App",
) for track in ("", "_dev")]

# This is a convenience rule which only builds the x86_64 translation, saving
# build time while testing.
genrule(
    name = "mosh_chrome_dev_x86_64",
    srcs = [
        "//mosh_app:mosh_assets",
        "//mosh_app:mosh_css",
        "//mosh_app:mosh_html",
        "//mosh_app:mosh_js",
        "//mosh_app:manifest_dev",
        "//mosh_nacl:mosh_client_manifest_x86_64",
        "//mosh_nacl:mosh_client_x86_64",
        "@libapps//:hterm",
    ],
    outs = ["mosh_chrome_x86_64.zip"],
    cmd = "/usr/bin/zip --quiet --junk-paths $(OUTS) $(SRCS)",
    message = "Packaging x86_64-only Chrome App",
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
