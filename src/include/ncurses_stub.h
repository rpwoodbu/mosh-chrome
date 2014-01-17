// ncurses_stub.h - Stub functions for a basic ncurses link to succeed.
//
// This file is just to fake out Mosh's configure script so that it believes
// ncurses works (which it does, but it doesn't believe so under newlib).

// Copyright 2013 Richard Woodbury
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef __NCURSES_STUB_H__
#define __NCURSES_STUB_H__

#include <signal.h>
#include <termios.h>
#include <unistd.h>

int sigaction(int signum, const struct sigaction *act,
    struct sigaction *oldact) {
  return 0;
}

int tcgetattr(int fd, struct termios *termios_p) {
  return 0;
}

int tcsetattr(int fd, int optional_actions,
    const struct termios *termios_p) {
  return 0;
}

int tcflush(int fd, int queue_selector) {
  return 0;
}

#endif // __NCURSES_STUB_H__
