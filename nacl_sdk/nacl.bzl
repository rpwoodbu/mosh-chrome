"""NaCl build rules."""

def nacl_manifest(name, srcs, out=None, visibility=None):
  """Generate a NaCl manifest .nmf file from PEXEs or NEXEs.

  Args:
    name: (str) Name for this target.
    srcs: (list(str)) Labels of the source PEXEs or NEXEs.
    out: (str) Filename of .nmf file to generate.
    visibility: (list(str)) Visibilities.
  """
  if not out:
    out = name + ".nmf"

  native.genrule(
    name = name,
    srcs = srcs,
    outs = [out],
    tools = ["@nacl_sdk//:create_nmf"],
    cmd = "$(location @nacl_sdk//:create_nmf) -o $(OUTS) $(SRCS)",
    message = "Building NaCl manifest",
    visibility = visibility,
  )
