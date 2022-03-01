package(default_visibility = ["//visibility:public"])

filegroup(
    name = "mosh_js",
    srcs = [
        ":mosh_version",
        "background.js",
        "mosh_client.js",
        "mosh_window.js",
        "nassh_workarounds.js",
        "ssh_key.js",
        "@libapps//:nassh_preferences_editor",
    ],
)

filegroup(
    name = "mosh_html",
    srcs = [
        "mosh_client.html",
        "mosh_prefs.html",
        "mosh_window.html",
        "ssh_key.html",
    ],
)

alias(
    name = "mosh_css",
    actual = "@libapps//:nassh_preferences_editor_css",
)

filegroup(
    name = "mosh_assets",
    srcs = [
        "laptop_terminal.png",
    ],
)

genrule(
    name = "mosh_version",
    srcs = ["@mosh//:version"],
    outs = ["mosh_version.js"],
    cmd = """
        echo "var moshVersion = \\"$$(cat $(location @mosh//:version))\\";" > $(OUTS)
    """,
    message = "Generating mosh_version.js",
)

py_binary(
    name = "subst",
    srcs = ["subst.py"],
)

py_test(
    name = "subst_test",
    srcs = ["subst_test.py"],
    deps = [":subst"],
)

genrule(
    name = "manifest",
    srcs = [
        ":manifest.json.template",
        "//:version.txt",
    ],
    outs = ["manifest.json"],
    cmd = """
        $(location subst) \
            NAME="Mosh" \
            DESCRIPTION="Mosh (mobile shell) for Chrome" \
            VERSION="$$(cat $(location //:version.txt))" \
            KEY="" \
            < $(location :manifest.json.template) > $(OUTS)
    """,
    tools = [":subst"],
    message = "Building Chrome App manifest",
)

genrule(
    name = "manifest_dev",
    srcs = [
        ":manifest.json.template",
        "//:version.txt",
    ],
    outs = ["dev/manifest.json"],
    cmd = """
        $(location subst) \
            NAME="Mosh (dev)" \
            DESCRIPTION="Mosh (mobile shell) for Chrome (dev track)" \
            VERSION="$$(cat $(location //:version.txt))" \
            KEY="" \
            < $(location :manifest.json.template) > $(OUTS)
    """,
    tools = [":subst"],
    message = "Building dev Chrome App manifest",
)

# For the Windows version, we need a "key" in the manifest. On Windows, with
# NW.js, the app is effectively sideloaded into Chromium on each invocation.
# This causes the app to get a new identifier each time, which in turn causes
# it to be unable to retrieve stored local data from previous invocations such
# as SSH fingerprints, last user/host, etc.
#
# We don't want to specify this key for versions distributed via the Chrome Web
# Store. The store manages the key and the app identifier for us. Letting the
# store to do this allows us to sideload for testing without interfering with
# the version installed from the store.
genrule(
    name = "manifest_windows",
    srcs = [
        ":manifest.json.template",
        "//:version.txt",
    ],
    outs = ["windows/manifest.json"],
    cmd = """
        $(location subst) \
            NAME="Mosh" \
            DESCRIPTION="Mosh (mobile shell) for Windows" \
            VERSION="$$(cat $(location //:version.txt))" \
            KEY='"key": "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEApx0tft2uFwdaR1KCwnkkFx/ac43SSJKbKALKaafwiOSDitY7jPkV46xbu4nq6LHOtLQK3ybMjjWHhUvDuyy0te2eum4GzHy2E10Feb1XBiQbdTyeUwF2nu1tBRrzi/F4AeizLuVNNMT+tXMKaQ7bNSYsPAsbyCrxoQDiaU9rdAmOp+TNM2IsZkPwsBg9oQfEOsXZ2C/c/dsyo4QkdAfGTR6xszd9RYyIoJOrQqjzDv0oFrQMSnK9ym3x4BOXZwb1G9PVC/GTvLQWBKUaHI5IPGQdmJQUgMmVWJeHrlS7ngR3GBmqcmiLVATY6PUKW42Smgc2rgj45KHm/cp1gF1OxQIDAQAB",' \
            < $(location :manifest.json.template) > $(OUTS)
    """,
    tools = [":subst"],
    message = "Building Windows Chrome App manifest",
)
