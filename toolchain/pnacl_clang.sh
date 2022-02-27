#!/bin/bash
set -euo pipefail
external/nacl_sdk/toolchain/linux_pnacl/bin/pnacl-clang "$@"
