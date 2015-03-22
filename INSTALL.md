Install Notes
=============

The bulk of these notes pertain to building dependencies of mosh-chrome, not
mosh-chrome itself.

Ubuntu (12.04, 14.04), 64-bit
-----------------------------

Install the following packages:

    git subversion zip build-essential cmake autoconf pkg-config libc6:i386
    libstdc++6:i386 protobuf-compiler

Debian (jessie/sid), 64-bit
---------------------------

Install the following packages:

    git subversion zip build-essential cmake autoconf pkg-config libc6-i386
    lib32stdc++6 protobuf-compiler

**Errors while building zlib as part of pepper**: "Signal 6 from trusted code", see
https://code.google.com/p/naclports/issues/detail?id=90#c6

General Notes
-------------

**Errors relating to pod2man while building openssl**: pod2man in Perl 5.18 is
too strict for the openssl pod files. There is a patch here,
https://rt.openssl.org/Ticket/Display.html?id=3057&user=guest&pass=guest; a
cheap workaround is to blank out all the pod files (the manpages they generate
are not relevant to mosh-chrome):

    $ echo "=pod" > ./../blank.pod
    $ find . -name '*.pod' -exec 'cp' './../blank.pod' '{}' \;
