package(default_visibility = ["//visibility:private"])

ARCHITECTURES = ("x64", "ia32")

[genrule(
    name = "mosh_windows_" + arch,
    srcs = [
        "mosh.ico",
        "mosh.wxs",
        ":nwjs_file_list_{arch}".format(arch=arch),
        ":nwjs_wxs_fragment_{arch}".format(arch=arch),
        "//:mosh_chrome_windows",
        "//:version.txt",
        "@nwjs_win_{arch}//:nwjs_distrib".format(arch=arch),
    ],
    outs = ["mosh_for_windows_{arch}.msi".format(arch=arch)],
    tools = [
        ":common_prefix",
        "@msitools//:wixl"
    ],
    cmd = """
        $(location @msitools//:wixl) \
            $(location :nwjs_wxs_fragment_{arch}) \
            $(location mosh.wxs) \
            -D SourceDir=$$( \
                $(location :common_prefix) \
                < $(location :nwjs_file_list_{arch})) \
            -D MoshChrome=$(location //:mosh_chrome_windows) \
            -D MoshIcon=$(location mosh.ico) \
            -D Version=$$(cat $(location //:version.txt)) \
            --arch {wixl_arch} \
            -o $(OUTS)
    """.format(
        arch=arch,
        wixl_arch = "x86" if arch == "ia32" else arch,
    ),
    message = "Building Mosh for Windows installer",
    visibility = ["//visibility:public"],
) for arch in ARCHITECTURES]

[genrule(
    name = "nwjs_wxs_fragment_{arch}".format(arch=arch),
    srcs = [":nwjs_file_list_{arch}".format(arch=arch)],
    outs = ["nwjs_{arch}.wxs_fragment".format(arch=arch)],
    tools = [
        ":common_prefix",
        ":emit_with_newlines",
        "@msitools//:wixl_heat"
    ],
    cmd = """
        $(location @msitools//:wixl_heat) \
            -p $$($(location :common_prefix) \
                < $(location :nwjs_file_list_{arch})) \
            --component-group NW.js \
            --var var.SourceDir \
            --directory-ref=INSTALLDIR \
            < $(location :nwjs_file_list_{arch}) \
            > $(OUTS)
    """.format(arch=arch),
    message = "Generating WXS fragment",
) for arch in ARCHITECTURES]

[genrule(
    name = "nwjs_file_list_{arch}".format(arch=arch),
    srcs = ["@nwjs_win_{arch}//:nwjs_distrib".format(arch=arch)],
    outs = ["nwjs_file_list_{arch}.txt".format(arch=arch)],
    tools = [":emit_with_newlines"],
    cmd = """
        $(location :emit_with_newlines) \
            $(locations @nwjs_win_{arch}//:nwjs_distrib) \
            > $(OUTS)
    """.format(arch=arch),
) for arch in ARCHITECTURES]

sh_binary(
    name = "common_prefix",
    srcs = ["common_prefix.sh"],
)

sh_binary(
    name = "emit_with_newlines",
    srcs = ["emit_with_newlines.sh"],
)
