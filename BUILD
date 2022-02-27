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
# TODO: Build separate ones for x64 and ia32 to reduce the size.
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
