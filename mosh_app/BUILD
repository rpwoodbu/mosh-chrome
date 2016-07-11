package(default_visibility = ["//visibility:public"])

filegroup(
    name = "mosh_js",
    srcs = [
        "background.js",
        "mosh_client.js",
        "mosh_window.js",
        "nassh_preferences_editor.js",
        "ssh_key.js",
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

filegroup(
    name = "mosh_css",
    srcs = [
        "nassh_preferences.css",
    ],
)

filegroup(
    name = "mosh_assets",
    srcs = [
        "laptop_terminal.png",
    ],
)

# TODO: Dependency on /usr/bin/envsubst (from Debian package gettext-base) is
# dubious.

genrule(
    name = "manifest",
    srcs = ["manifest.json.template"],
    outs = ["manifest.json"],
    cmd = """
        export NAME="Mosh"
        export DESCRIPTION="Mosh (mobile shell) for Chrome"
        /usr/bin/envsubst \
            '$$$$NAME $$$$DESCRIPTION' \
            < $(SRCS) > $(OUTS)
    """,
    message = "Building Chrome App manifest",
)

genrule(
    name = "manifest_dev",
    srcs = ["manifest.json.template"],
    outs = ["dev/manifest.json"],
    cmd = """
        export NAME="Mosh (dev)"
        export DESCRIPTION="Mosh (mobile shell) for Chrome (dev track)"
        /usr/bin/envsubst \
            '$$$$NAME $$$$DESCRIPTION' \
            < $(SRCS) > $(OUTS)
    """,
    message = "Building dev Chrome App manifest",
)
