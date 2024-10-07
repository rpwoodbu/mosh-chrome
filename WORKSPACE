workspace(name = "mosh_chrome")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "rbe_default",
    # Change the sha256 digest to the value of the `configs_tarball_digest` in the manifest you
    # got when you ran the curl command above.
    sha256 = "9f81180099ebc84da906c53cf56c1e9835d7dbc4bbc8ac5c7e075f050c450c3a",
    urls = ["https://storage.googleapis.com/rbe-toolchain/bazel-configs/bazel_5.0.0/rbe-ubuntu1604/latest/rbe_default.tar"],
)

# The manifest of NaCl SDK versions can be found here:
# https://storage.googleapis.com/nativeclient-mirror/nacl/nacl_sdk/naclsdk_manifest2.json

# There are hardlinks in the stock NaCl SDK archive, and Bazel can't handle
# them. It just creates files of zero size. To work around this, I've hacked
# together a custom repository rule that just shells out to "tar". This isn't
# portable, but neither is the SDK.
#
# In the event that Bazel fixes this bug, here is the native rule:
#
# http_archive(
#     name = "nacl_sdk",
#     url = "https://storage.googleapis.com/nativeclient-mirror/nacl/nacl_sdk/49.0.2623.87/naclsdk_linux.tar.bz2",
#     sha256 = "c53c14e5eaf6858e5b4a4e964c84d7774f03490be7986ab07c6792e807e05f14",
#     strip_prefix = "pepper_49",
#     build_file = "BUILD.nacl_sdk",
# )

load(
    "//:external_builds/new_http_tar_archive_hardlinks.bzl",
    "new_http_tar_archive_hardlinks",
)
new_http_tar_archive_hardlinks(
    name = "nacl_sdk",
    url = "https://storage.googleapis.com/nativeclient-mirror/nacl/nacl_sdk/49.0.2623.87/naclsdk_linux.tar.bz2",
    sha256 = "c53c14e5eaf6858e5b4a4e964c84d7774f03490be7986ab07c6792e807e05f14",
    strip_components = 1,
    build_file = "//:external/BUILD.nacl_sdk",
)

http_archive(
    name = "jsoncpp",
    url = "https://gsdview.appspot.com/webports/builds/pepper_49/trunk-785-g807a23e/packages/jsoncpp_1.6.1_pnacl.tar.bz2",
    sha256 = "eb6067b5fc463b3a3777365c9a0cb3da69acd1cffbd22f8e9b1a9eb630c0d1c8",
    strip_prefix = "payload",
    build_file = "BUILD.jsoncpp",
)

http_archive(
    name = "ncurses",
    url = "https://gsdview.appspot.com/webports/builds/pepper_49/trunk-785-g807a23e/packages/ncurses_5.9_pnacl.tar.bz2",
    sha256 = "5d654df56d146d593bdc855231d61f745460b182be83c4f081e824362042c6d7",
    strip_prefix = "payload",
    build_file = "BUILD.ncurses",
)

http_archive(
    name = "openssl",
    url = "https://gsdview.appspot.com/webports/builds/pepper_49/trunk-785-g807a23e/packages/openssl_1.0.2e_pnacl.tar.bz2",
    sha256 = "8ff02228adbd45c22a36611ffd0d47115d62e08bdfb5750977195cf19cb2b912",
    strip_prefix = "payload",
    build_file = "BUILD.openssl",
)

http_archive(
    name = "protobuf",
    url = "https://gsdview.appspot.com/webports/builds/pepper_49/trunk-785-g807a23e/packages/protobuf_3.0.0-beta-2_pnacl.tar.bz2",
    sha256 = "76780b202c2b692bbeef4b8ed5706563b9d5f02acb98d7bd6250fa00e2d6e1f9",
    strip_prefix = "payload",
    build_file = "BUILD.protobuf",
)

http_archive(
    name = "pnacl_zlib",
    url = "https://gsdview.appspot.com/webports/builds/pepper_49/trunk-785-g807a23e/packages/zlib_1.2.8_pnacl.tar.bz2",
    sha256 = "99203f49edad39570a3362e72373ea1d63a97b9b1828b75e45d3e856d2832033",
    strip_prefix = "payload",
    build_file = "BUILD.zlib",
)

# Needed by `com_google_protobuf`.
bind(
    name = "zlib",
    actual = "@pnacl_zlib//:zlib",
)

http_archive(
    name = "glibc_compat",
    url = "https://gsdview.appspot.com/webports/builds/pepper_49/trunk-785-g807a23e/packages/glibc-compat_0.1_pnacl.tar.bz2",
    sha256 = "3153933c99f161c41eb9205d7b30459fd8b367a67dcf942829182b2c15f541fd",
    strip_prefix = "payload",
    build_file = "BUILD.glibc_compat",
)

LIBSSH_COMMIT = "cdd4f3e3efb8758a53ebf34ac7b34d74d217158d"  # branch = "mosh-chrome-patches"
LIBSSH_SHA256 = "98b9076b95f3cdd710475fad7a72c0a88ab54fc840c84678c0cdd5836a8c4008"
http_archive(
    name = "libssh",
    urls = ["https://github.com/rpwoodbu/libssh/archive/{}.tar.gz".format(LIBSSH_COMMIT)],
    sha256 = LIBSSH_SHA256,
    strip_prefix = "libssh-{}".format(LIBSSH_COMMIT),
    build_file = "BUILD.libssh",
)

MOSH_COMMIT = "cf73e1f8799b01ad1ed9731c6b3d239b68509222"  # tag = "mosh-1.3.2"
MOSH_SHA256 = "253cb72e58a4a011f7133abdf68b0035f9d91ed5947b27a0574b7563370c9473"
http_archive(
    name = "mosh",
    urls = ["https://github.com/rpwoodbu/mosh/archive/{}.tar.gz".format(MOSH_COMMIT)],
    sha256 = MOSH_SHA256,
    strip_prefix = "mosh-{}".format(MOSH_COMMIT),
    build_file = "BUILD.mosh",
)

LIBAPPS_COMMIT = "2056832f0b287924110849cecbf1e5019c1a51c3"  # tag = "hterm-1.80"
LIBAPPS_SHA256 = "4e4f44006a6197fcbbcf159cfccdd6d1ced8dc8d7d033520fc8b52c8543e2177"
http_archive(
    name = "libapps",
    urls = ["https://github.com/libapps/libapps-mirror/archive/{}.tar.gz".format(LIBAPPS_COMMIT)],
    sha256 = LIBAPPS_SHA256,
    strip_prefix = "libapps-mirror-{}".format(LIBAPPS_COMMIT),
    build_file = "BUILD.libapps",
)

PROTOBUF_COMMIT = "582743bf40c5d3639a70f98f183914a2c0cd0680"  # tag = "v3.7.0"
PROTOBUF_SHA256 = "cf9e2fb1d2cd30ec9d51ff1749045208bd641f290f64b85046485934b0e03783"
http_archive(
    name = "com_google_protobuf",
    urls = ["https://github.com/google/protobuf/archive/{}.tar.gz".format(PROTOBUF_COMMIT)],
    sha256 = PROTOBUF_SHA256,
    strip_prefix = "protobuf-{}".format(PROTOBUF_COMMIT),
)

# Needed by `com_google_protobuf`.
http_archive(
    name = "bazel_skylib",
    sha256 = "bbccf674aa441c266df9894182d80de104cabd19be98be002f6d478aaa31574d",
    strip_prefix = "bazel-skylib-2169ae1c374aab4a09aa90e65efe1a3aad4e279b",
    urls = ["https://github.com/bazelbuild/bazel-skylib/archive/2169ae1c374aab4a09aa90e65efe1a3aad4e279b.tar.gz"],
)

# Nothing special about this commit; this project simply doesn't have a recent
# release which includes Bazel support. Chose the same commit as found here:
# https://github.com/bazelbuild/bazel/blob/d3d86440e90a8cef8483dec83f06c8a6f644603e/WORKSPACE#L163
GOOGLETEST_COMMIT = "dfa853b63d17c787914b663b50c2095a0c5b706e"
GOOGLETEST_SHA256 = "313a16fba8f0be8ee20ba9883e044556044cbd1ae6cea532473d163a843ef991"
http_archive(
    name = "com_google_googletest",
    urls = ["https://github.com/google/googletest/archive/{}.tar.gz".format(GOOGLETEST_COMMIT)],
    sha256 = GOOGLETEST_SHA256,
    strip_prefix = "googletest-{}".format(GOOGLETEST_COMMIT),
)

STYLEGUIDE_COMMIT = "6d3a7d8a229e189f7a5bb7c3923363356625ece5"  # From branch "gh-pages"
STYLEGUIDE_SHA256 = "1c0367ed9caf3906d3f3d94bc78a20a2b8288e1800beaee1fae26c4010afb6c0"
http_archive(
    name = "styleguide",
    urls = ["https://github.com/google/styleguide/archive/{}.tar.gz".format(STYLEGUIDE_COMMIT)],
    sha256 = STYLEGUIDE_SHA256,
    strip_prefix = "styleguide-{}".format(STYLEGUIDE_COMMIT),
    build_file = "BUILD.styleguide",
)

#
# Stuff to build a "native" Windows app.
#

http_archive(
    name = "msitools",
    url = "http://ftp.gnome.org/pub/GNOME/sources/msitools/0.95/msitools-0.95.tar.xz",
    sha256 = "977ea3744cf091a19a8d06eae149fa9ab0d5cd078cc224e8da92bc05dcba66da",
    strip_prefix = "msitools-0.95",
    build_file = "BUILD.msitools",
)

http_archive(
    name = "libgcab",
    url = "http://ftp.gnome.org/pub/GNOME/sources/gcab/0.7/gcab-0.7.tar.xz",
    sha256 = "a16e5ef88f1c547c6c8c05962f684ec127e078d302549f3dfd2291e167d4adef",
    strip_prefix = "gcab-0.7",
    build_file = "BUILD.libgcab",
)

http_archive(
    name = "nwjs_win_x64",
    url = "https://dl.nwjs.io/v0.16.1/nwjs-sdk-v0.16.1-win-x64.zip",
    sha256 = "ceaab5e221bb1503626cfee05de3564c406a350b43ee16f97130a3f25b8fd6ce",
    strip_prefix = "nwjs-sdk-v0.16.1-win-x64",
    build_file = "BUILD.nwjs_win",
)

http_archive(
    name = "nwjs_win_ia32",
    url = "https://dl.nwjs.io/v0.16.1/nwjs-sdk-v0.16.1-win-ia32.zip",
    sha256 = "0e43f5e699e83cf0ab51ebfd6ba62b7424a71ec4151949e60fbdc126745cc139",
    strip_prefix = "nwjs-sdk-v0.16.1-win-ia32",
    build_file = "BUILD.nwjs_win",
)
