"""NaCl build rules."""

def nacl_manifest(name, srcs, visibility=None):
  """Generate a NaCl manifest .nmf file from PEXEs or NEXEs.

  Args:
    name: (str) Name for this target.
    srcs: (list(str)) Labels of the source PEXEs or NEXEs.
    visibility: (list(str)) Visibilities.
  """
  native.genrule(
    name = name,
    srcs = srcs,
    outs = [name + ".nmf"],
    tools = ["@nacl_sdk//:create_nmf"],
    cmd = "$(location @nacl_sdk//:create_nmf) -o $(OUTS) $(SRCS)",
    message = "Building NaCl manifest",
    visibility = visibility,
  )

def nacl_manifest_js(name, src, out, visibility=None):
  """Generate a JS file with details on the manifest file.

  To run the NaCl binary, you need to know the name of the manifest file. It is
  also important to know whether or not the manifest contains a NEXE or a PEXE,
  as they use different MIME types. This rule creates a file you can include
  into your JS to construct the embed tag appropriately.

  Args:
    name: (str) Name for this target.
    src: (str) Label of a manifest .nmf file.
    out: (str) Filename of .js file to generate.
    visibility: (list(str)) Visibilities.
  """
  native.genrule(
    name = name,
    srcs = [src],
    outs = [out],
    cmd = """
      if grep portable "$(SRCS)" > /dev/null; then
        MIME_TYPE="application/x-pnacl"
      else
        MIME_TYPE="application/x-nacl"
      fi

      cat > $(OUTS) << EOF
var nacl_nmf_file = "$$(basename $(SRCS))";
var nacl_mime_type = "$${MIME_TYPE}";
EOF
    """,
    message = "Generating manifest JS file",
    visibility = visibility,
  )
