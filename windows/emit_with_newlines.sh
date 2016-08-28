#!/bin/bash -e

# Emits all arguments on stdout, each on a new line.

while [[ $# > 0 ]]; do
  echo "${1}"
  shift
done
