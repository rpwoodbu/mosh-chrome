// pepper_wrapper.h - Main include file for Pepper wrapping.
//
// Implement the functions below to interface with Pepper wrapping.

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

#ifndef PEPPER_WRAPPER
#define PEPPER_WRAPPER

#include "pepper_posix.h"

// Implement this to return an appropriate instance of PepperPOSIX::POSIX.
// You may want to return a particular one based on the calling thread to
// support multiple Pepper Instances.
PepperPOSIX::POSIX *GetPOSIX();

#endif // PEPPER_WRAPPER
