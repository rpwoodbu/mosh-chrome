package(default_visibility = ["//visibility:private"])

load("//:nacl_sdk/pnacl.bzl", "pnacl_translated", "pnacl_finalized")
load("//:nacl_sdk/nacl.bzl", "nacl_manifest")

architectures = ("x86_64", "i686", "armv7")

[pnacl_translated(
    name = "mosh_client_" + arch,
    src = ":mosh_client_pexe",
    arch = arch,
    visibility = ["//visibility:public"],
) for arch in architectures]

filegroup(
    name = "mosh_client_all_architectures",
    srcs = [":mosh_client_" + arch for arch in architectures],
    visibility = ["//visibility:public"],
)

pnacl_finalized(
    name = "mosh_client_pexe",
    src = ":mosh_client",
    visibility = ["//visibility:public"],
)

nacl_manifest(
    name = "mosh_client_manifest_all_architectures",
    srcs = [":mosh_client_" + arch for arch in architectures],
    visibility = ["//visibility:public"],
)

nacl_manifest(
    name = "mosh_client_manifest_pexe",
    srcs = [":mosh_client_pexe"],
    visibility = ["//visibility:public"],
)

# This is a convenience rule which only builds the x86_64 translation, saving
# build time while testing.
nacl_manifest(
    name = "mosh_client_manifest_x86_64",
    srcs = [":mosh_client_x86_64"],
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "mosh_client",
    srcs = ["mosh_nacl.cc"],
    deps = [
        ":gpdns_resolver_lib",
        ":mosh_nacl_hdr",
        ":pepper_posix_tcp_lib",
        ":pepper_resolver_lib",
        ":ssh_login_lib",
        "@mosh//:mosh_client_lib",
        "@nacl_sdk//:pepper_lib",
    ],
)

cc_library(
    name = "mosh_nacl_hdr",
    hdrs = ["mosh_nacl.h"],
    deps = [
        ":pepper_wrapper_lib",
        ":resolver_lib",
    ],
)

cc_library(
    name = "pepper_wrapper_lib",
    srcs = ["pepper_wrapper.cc"],
    hdrs = ["pepper_wrapper.h"],
    deps = [
        ":pepper_posix_lib",
    ],
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
)

cc_library(
    name = "pepper_posix_tcp_lib",
    srcs = ["pepper_posix_tcp.cc"],
    hdrs = ["pepper_posix_tcp.h"],
    deps = [
        ":pepper_posix_hdr",
        "@glibc_compat//:glibc_compat",
    ],
)

cc_library(
    name = "pepper_posix_native_udp_lib",
    srcs = ["pepper_posix_native_udp.cc"],
    hdrs = ["pepper_posix_native_udp.h"],
    deps = [
        ":pepper_posix_udp_lib",
    ],
)

cc_library(
    name = "pepper_posix_native_tcp_lib",
    srcs = ["pepper_posix_native_tcp.cc"],
    hdrs = ["pepper_posix_native_tcp.h"],
    deps = [
        ":pepper_posix_tcp_lib",
    ],
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
    name = "sshfp_record_lib",
    srcs = ["sshfp_record.cc"],
    hdrs = ["sshfp_record.h"],
    deps = [
        ":ssh_lib",
    ],
)

cc_test(
    name = "sshfp_record_test",
    srcs = ["sshfp_record_test.cc"],
    deps = [":sshfp_record_lib"],
    # Undefine NDEBUG, as this test uses assert(), which is rightly disabled in
    # opt builds.
    copts = ["-UNDEBUG"],
)

cc_library(
    name = "ssh_login_lib",
    srcs = ["ssh_login.cc"],
    hdrs = ["ssh_login.h"],
    deps = [
        ":mosh_nacl_hdr",
        ":ssh_lib",
        ":sshfp_record_lib",
        "@nacl_sdk//:pepper_lib",
    ],
)

cc_library(
    name = "resolver_lib",
    hdrs = ["resolver.h"],
)

cc_library(
    name = "pepper_resolver_lib",
    srcs = ["pepper_resolver.cc"],
    hdrs = ["pepper_resolver.h"],
    deps = [
        ":resolver_lib",
        "@nacl_sdk//:pepper_lib",
    ],
)

cc_library(
    name = "gpdns_resolver_lib",
    srcs = ["gpdns_resolver.cc"],
    hdrs = ["gpdns_resolver.h"],
    deps = [
        ":resolver_lib",
        "@glibc_compat//:glibc_compat",
        "@jsoncpp//:jsoncpp",
        "@nacl_sdk//:pepper_lib",
    ],
)

cc_library(
    name = "make_unique_lib",
    hdrs = ["make_unique.h"],
)
