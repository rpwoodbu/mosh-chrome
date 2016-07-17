"""PNaCl build rules."""

def pnacl_finalized(name, src, visibility=None):
  """Finalizes a library to portable bitcode for distribution.

  Args:
    name: (str) Name for this target.
    src: (str) Label of the source library.
    visibility: (list(str)) Visibilities.
  """
  native.genrule(
    name = name,
    srcs = [src],
    outs = [name + ".pexe"],
    tools = ["@nacl_sdk//:pnacl_toolchain"],
    cmd = "external/nacl_sdk/toolchain/linux_pnacl/bin/pnacl-finalize $(SRCS) -o $(OUTS)",
    message = "Finalizing portable binary",
    visibility = visibility,
  )


def pnacl_translated(name, src, arch, visibility=None):
  """Translates finalized portable bitcode to native executable.

  Args:
    name: (str) Name for this target.
    src: (str) Label of the PEXE to translate.
    arch: (str) Mnemonic of one of the supported PNaCl target architectures.
    visibility: (list(str)) Visibilities.
  """
  threads_flag = ""
  if arch == "armv7":
    # Work around a bug in pnacl-translate which creates unreliable and
    # nondeterministic NEXEs for armv7 when running multithreaded.
    threads_flag = "-threads=seq"

  native.genrule(
    name = name,
    srcs = [src],
    outs = [name + ".nexe"],
    tools = ["@nacl_sdk//:pnacl_toolchain"],
    message = "Translating portable bitcode to native code",
    cmd = """
      external/nacl_sdk/toolchain/linux_pnacl/bin/pnacl-translate \
        -arch {arch} {threads_flag} $(SRCS) -o $(OUTS)
    """.format(
      arch=arch,
      threads_flag=threads_flag,
    ),
    visibility = visibility,
  )
