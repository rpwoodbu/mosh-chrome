"""Very rudamentary replacement for envsubst.

Example:
  echo "A ${noun} ${verb} into a ${bar}." | subst noun=neutron verb=walks bar=reactor

Will output:
  A neutron walks into a reactor.
"""

import sys


def substitute(template, args):
  for arg in args:
    k, v = arg.split('=')
    template = template.replace('${%s}' % k, v)
  return template


if __name__ == '__main__':
  sys.stdout.write(substitute(sys.stdin.read(), sys.argv[1:]))
