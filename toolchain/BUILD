package(default_visibility = ['//visibility:public'])
load(":pnacl_toolchain_config.bzl", "cc_toolchain_config")

cc_toolchain_suite(
    name = "toolchain",
    toolchains = {
        "pnacl": ":pnacl_toolchain",
        "k8|clang": "@rbe_default//cc:cc-compiler-k8",
        "k8": "@rbe_default//cc:cc-compiler-k8",
        "armeabi-v7a|compiler": "@rbe_default//cc:cc-compiler-armeabi-v7a",
        "armeabi-v7a": "@rbe_default//cc:cc-compiler-armeabi-v7a",
    },
)

filegroup(
    name = "toolchain_files",
    srcs = [
        "@nacl_sdk//:pnacl_includes",
        "@nacl_sdk//:pnacl_toolchain",
        "pnacl_ar.sh",
        "pnacl_clang++.sh",
        "pnacl_clang.sh",
        "pnacl_ld.sh",
        "pnacl_strip.sh",
    ],
)

filegroup(name = "empty")

cc_toolchain(
    name = "pnacl_toolchain",
    toolchain_identifier = "pnacl-toolchain",
    toolchain_config = ":pnacl_toolchain_config",
    ar_files = ":toolchain_files",
    compiler_files = ":toolchain_files",
    linker_files = ":toolchain_files",
    strip_files = ":toolchain_files",

    # The following are no longer used, but still must exist.
    all_files = ":empty",
    dwp_files = ":empty",
    objcopy_files = ":empty",

    supports_param_files = 0,
)

cc_toolchain_config(name = "pnacl_toolchain_config")
