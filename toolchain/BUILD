package(default_visibility = ['//visibility:public'])
load(":pnacl_toolchain_config.bzl", "cc_toolchain_config")

cc_toolchain_suite(
    name = "pnacl_clang",
    toolchains = {
        "pnacl": ":pnacl_toolchain",
    },
)

filegroup(
    name = "all",
    srcs = [
        "pnacl_clang.sh",
        "pnacl_ld.sh",
        "pnacl_ar.sh",
        "pnacl_cpp.sh",
        "pnacl_nm.sh",
        "pnacl_strip.sh",
        "@nacl_sdk//:pnacl_all_files",
    ],
)

cc_toolchain(
    name = "pnacl_toolchain",
    toolchain_identifier = "pnacl-toolchain",
    toolchain_config = ":pnacl_toolchain_config",
    all_files = ":all",
    compiler_files = ":all",
    dwp_files = ":all",
    linker_files = ":all",
    objcopy_files = ":all",
    strip_files = ":all",
    supports_param_files = 0,
)

cc_toolchain_config(name = "pnacl_toolchain_config")
