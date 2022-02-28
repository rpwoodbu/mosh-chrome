load("@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
     "feature",
     "flag_group",
     "flag_set",
     "tool_path",
)
load("@bazel_tools//tools/build_defs/cc:action_names.bzl", "ACTION_NAMES")

def _impl(ctx):
    tool_paths = [
        tool_path(
            name = "gcc",
            path = "pnacl_clang.sh",
        ),
        tool_path(
            name = "ld",
            path = "pnacl_ld.sh",
        ),
        tool_path(
            name = "ar",
            path = "pnacl_ar.sh",
        ),
        tool_path(
            name = "cpp",
            path = "pnacl_clang++.sh",
        ),
        tool_path(
            name = "nm",
            path = "pnacl_nm.sh",
        ),
        tool_path(
            name = "objdump",
            path = "/bin/false",
        ),
        tool_path(
            name = "strip",
            path = "pnacl_strip.sh",
        ),
    ]

    cxx_flags_feature = feature(
        name = "cxx_flags",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = [
                    ACTION_NAMES.cpp_compile,
                    ACTION_NAMES.cpp_header_parsing,
                    ACTION_NAMES.cpp_module_compile,
                    ACTION_NAMES.cpp_module_codegen,
                ],
                flag_groups = [
                    flag_group(
                        flags = [
                            "-std=gnu++11",
                        ],
                    ),
                ],
            ),
        ],
    )

    cpp_link_executable_flags_feature = feature(
        name = "cpp_link_executable_flags",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = [
                    ACTION_NAMES.cpp_link_executable,
                ],
                flag_groups = [
                    flag_group(
                        flags = [
                            "-lc++"
                        ],
                    ),
                ],
            ),
        ],
    )

    default_flags_feature = feature(
        name = "default_flags",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = [
                    ACTION_NAMES.assemble,
                    ACTION_NAMES.preprocess_assemble,
                    ACTION_NAMES.linkstamp_compile,
                    ACTION_NAMES.c_compile,
                    ACTION_NAMES.cpp_compile,
                    ACTION_NAMES.cpp_header_parsing,
                    ACTION_NAMES.cpp_module_compile,
                    ACTION_NAMES.cpp_module_codegen,
                    ACTION_NAMES.lto_backend,
                    ACTION_NAMES.clif_match,
                ],
                flag_groups = [
                    flag_group(
                        flags = [
                            "-Wall",
                            "-Werror",
                            # Protobufs won't build with this warning:
                            "-Wno-inconsistent-missing-override",
                            "-isystem",
                            "external/nacl_sdk/include/pnacl",
                            "-isystem",
                            "external/nacl_sdk/toolchain/linux_pnacl/le32-nacl/include",
                            "-isystem",
                            "external/nacl_sdk/toolchain/linux_pnacl/le32-nacl/include/c++/v1",
                            "-isystem",
                            "external/nacl_sdk/toolchain/linux_pnacl/lib/clang/3.7.0/include",
                            "-isystem",
                            "external/glibc_compat/include",
                        ],
                    ),
                ],
            ),
        ],
    )

    return cc_common.create_cc_toolchain_config_info(
        ctx = ctx,
        toolchain_identifier = "pnacl-toolchain",
        host_system_name = "local",
        target_system_name = "pnacl",
        target_cpu = "pnacl",
        target_libc = "pnacl",
        compiler = "clang",
        abi_version = "pnacl",
        abi_libc_version = "pnacl",
        tool_paths = tool_paths,
        features = [
            default_flags_feature,
            cxx_flags_feature,
            cpp_link_executable_flags_feature,
        ],
    )

cc_toolchain_config = rule(
    implementation = _impl,
    attrs = {},
    provides = [CcToolchainConfigInfo],
)
