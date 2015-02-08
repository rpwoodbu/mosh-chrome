// make_unique.h - Convienience template function for making unique_ptr.

// Copyright 2015 Richard Woodbury
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

#ifndef MAKE_UNIQUE_H
#define MAKE_UNIQUE_H

#include <memory>

template <typename T>
std::unique_ptr<T> make_unique(T* t) {
  return std::unique_ptr<T>(t);
}

#endif // MAKE_UNIQUE_H
