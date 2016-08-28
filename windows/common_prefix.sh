#!/bin/bash -e

# Emits the common textual prefix of all items on stdin.

sed -e 'N;s/^\(.*\).*\n\1.*$/\1\n\1/;D'
