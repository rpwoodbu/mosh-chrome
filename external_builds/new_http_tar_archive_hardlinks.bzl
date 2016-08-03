# The new_http_archive() function (and probably http_archive() as well) cannot
# handle tarballs that generate hardlinks. All but one of a set of hardlinked
# files will be of zero size. new_http_tar_archive_hardlinks() works around that
# issue by shelling out to "tar". This is obviously not portable.

def _impl(repository_ctx):
    tarball_name = "tarball.tar.bz2"

    repository_ctx.download(
        repository_ctx.attr.url,
        tarball_name,
        repository_ctx.attr.sha256,
    )

    tar_result = repository_ctx.execute([
        "tar",
        "-xjf", tarball_name,
        "--strip-components", str(repository_ctx.attr.strip_components),
    ])
    if tar_result.return_code != 0:
        fail("tar exited {}: {}".format(
            tar_result.return_code, tar_result.stderr))

    rm_result = repository_ctx.execute(["rm", tarball_name])
    if rm_result.return_code != 0:
        fail("rm exited {}: {}".format(rm_result.return_code, rm_result.stderr))

    repository_ctx.symlink(repository_ctx.attr.build_file, "BUILD")


new_http_tar_archive_hardlinks = repository_rule(
    implementation = _impl,
    attrs = {
        "url": attr.string(mandatory=True),
        "sha256": attr.string(mandatory=True),
        "strip_components": attr.int(mandatory=True),
        "build_file": attr.label(mandatory=True),
    },
)
