from subst import substitute

template = 'A ${noun} ${verb} into a ${bar}.'
args = ['noun=neutron', 'verb=walks', 'bar=reactor']
expected = 'A neutron walks into a reactor.'
assert(substitute(template, args) == expected)
