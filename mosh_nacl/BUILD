package(default_visibility = ["//visibility:private"])

# TODO: PNaCl rules deserve to be in Skylark, not genrules.

genrule(
    name = "mosh_client_x86_64",
    srcs = [":mosh_client_pexe"],
    outs = ["mosh_client_x86_64.nexe"],
    tools = ["@nacl_sdk//:pnacl_toolchain"],
    message = "Translating bytecode to native code",
    cmd = """
        external/nacl_sdk/toolchain/linux_pnacl/bin/pnacl-translate \
        -arch x86-64 \
        $(SRCS) -o $(OUTS)
    """,
    visibility = ["//visibility:public"],
)

genrule(
    name = "mosh_client_i686",
    srcs = [":mosh_client_pexe"],
    outs = ["mosh_client_i686.nexe"],
    tools = ["@nacl_sdk//:pnacl_toolchain"],
    message = "Translating bytecode to native code",
    cmd = """
        external/nacl_sdk/toolchain/linux_pnacl/bin/pnacl-translate \
        -arch i686 \
        $(SRCS) -o $(OUTS)
    """,
    visibility = ["//visibility:public"],
)

genrule(
    name = "mosh_client_armv7",
    srcs = [":mosh_client_pexe"],
    outs = ["mosh_client_armv7.nexe"],
    tools = ["@nacl_sdk//:pnacl_toolchain"],
    message = "Translating bytecode to native code",
    # ARM translation seems broken; see Issue #90. Setting -threads=seq at
    # least prevents the file size from varying, and _may_ reduce the number of
    # incidents of broken nexes.
    cmd = """
        external/nacl_sdk/toolchain/linux_pnacl/bin/pnacl-translate \
        -arch armv7 \
        -threads=seq \
        $(SRCS) -o $(OUTS)
    """,
    visibility = ["//visibility:public"],
)

genrule(
    name = "mosh_client_pexe",
    srcs = [":mosh_client_bc"],
    outs = ["mosh_client.pexe"],
    tools = ["@nacl_sdk//:pnacl_toolchain"],
    cmd = "external/nacl_sdk/toolchain/linux_pnacl/bin/pnacl-finalize $(SRCS) -o $(OUTS)",
    message = "Finalizing pexe",
    visibility = ["//visibility:public"],
)

genrule(
    name = "mosh_client_manifest",
    srcs = [
        ":mosh_client_x86_64",
        ":mosh_client_i686",
        ":mosh_client_armv7",
    ],
    outs = ["all_platforms/mosh_client.nmf"],
    tools = ["@nacl_sdk//:create_nmf"],
    cmd = "$(location @nacl_sdk//:create_nmf) -o $(OUTS) $(SRCS)",
    message = "Building NaCl manifest",
    visibility = ["//visibility:public"],
)

# This is a convenience rule which only builds the x86_64 translation, saving
# build time while testing.
genrule(
    name = "mosh_client_manifest_x86_64",
    srcs = [
        ":mosh_client_x86_64",
    ],
    outs = ["x86_64/mosh_client.nmf"],
    tools = ["@nacl_sdk//:create_nmf"],
    cmd = "$(location @nacl_sdk//:create_nmf) -o $(OUTS) $(SRCS)",
    message = "Building x86_64-only NaCl manifest",
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "mosh_client_bc",
    srcs = ["mosh_nacl.cc"],
    deps = [
        ":mosh_nacl_hdr",
        ":pepper_posix_tcp_lib",
        ":ssh_login_lib",
        "@mosh//:mosh_client_lib",
        "@nacl_sdk//:pepper_lib",
    ],
    copts = ["-Wno-unused-parameter"], # TODO: Fix this.
)

cc_library(
    name = "mosh_nacl_hdr",
    hdrs = ["mosh_nacl.h"],
    deps = [
        ":pepper_wrapper_lib",
    ],
)

cc_library(
    name = "pepper_wrapper_lib",
    srcs = ["pepper_wrapper.cc"],
    hdrs = ["pepper_wrapper.h"],
    deps = [
        ":pepper_posix_lib",
    ],
    copts = ["-Wno-unused-parameter"],
    defines = ["USE_NEWLIB"],
)

cc_library(
    name = "pepper_posix_hdr",
    hdrs = ["pepper_posix.h"],
    deps = [
        ":pepper_posix_selector_lib",
        "@nacl_sdk//:pepper_lib",
    ],
)

cc_library(
    name = "pepper_posix_lib",
    srcs = ["pepper_posix.cc"],
    deps = [
        ":pepper_posix_hdr",
        ":pepper_posix_native_udp_lib",
        ":pepper_posix_native_tcp_lib",
    ],
    copts = ["-Wno-unused-parameter"], # TODO: Fix this.
)

cc_library(
    name = "pepper_posix_selector_lib",
    srcs = ["pepper_posix_selector.cc"],
    hdrs = ["pepper_posix_selector.h"],
    deps = [
        ":make_unique_lib",
        ":pthread_locks_lib",
    ],
)

cc_library(
    name = "pthread_locks_lib",
    hdrs = ["pthread_locks.h"],
)

cc_library(
    name = "pepper_posix_udp_lib",
    srcs = ["pepper_posix_udp.cc"],
    hdrs = ["pepper_posix_udp.h"],
    deps = [
        ":pepper_posix_hdr",
        "@glibc_compat//:glibc_compat",
    ],
    copts = ["-Wno-unused-parameter"], # TODO: Fix this.
)

cc_library(
    name = "pepper_posix_tcp_lib",
    srcs = ["pepper_posix_tcp.cc"],
    hdrs = ["pepper_posix_tcp.h"],
    deps = [
        ":pepper_posix_hdr",
        "@glibc_compat//:glibc_compat",
    ],
    copts = ["-Wno-unused-parameter"], # TODO: Fix this.
)

cc_library(
    name = "pepper_posix_native_udp_lib",
    srcs = ["pepper_posix_native_udp.cc"],
    hdrs = ["pepper_posix_native_udp.h"],
    deps = [
        ":pepper_posix_udp_lib",
    ],
    copts = ["-Wno-unused-parameter"], # TODO: Fix this.
)

cc_library(
    name = "pepper_posix_native_tcp_lib",
    srcs = ["pepper_posix_native_tcp.cc"],
    hdrs = ["pepper_posix_native_tcp.h"],
    deps = [
        ":pepper_posix_tcp_lib",
    ],
    copts = ["-Wno-unused-parameter"], # TODO: Fix this.
)

cc_library(
    name = "ssh_lib",
    srcs = ["ssh.cc"],
    hdrs = ["ssh.h"],
    deps = [
        ":make_unique_lib",
        "@libssh//:libssh",
    ],
)

cc_library(
    name = "ssh_login_lib",
    srcs = ["ssh_login.cc"],
    hdrs = ["ssh_login.h"],
    deps = [
        ":mosh_nacl_hdr",
        ":ssh_lib",
        "@nacl_sdk//:pepper_lib",
    ],
)

cc_library(
    name = "make_unique_lib",
    hdrs = ["make_unique.h"],
)
