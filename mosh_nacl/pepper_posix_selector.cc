// pepper_posix_selector.cc - Selector for Pepper POSIX adapters.

// Copyright 2013, 2014, 2015 Richard Woodbury
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

#include "mosh_nacl/pepper_posix_selector.h"

#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <algorithm>
#include <memory>

#include "mosh_nacl/make_unique.h"

namespace PepperPOSIX {

using std::unique_ptr;
using std::vector;
using util::make_unique;

Selector::Selector() {}

Selector::~Selector() {
  // It is a logical error to delete Selector before all Targets have been
  // deleted.
  assert(targets_.size() == 0);
}

unique_ptr<Target> Selector::NewTarget(int id) {
  auto t = make_unique<Target>(*this, id);
  targets_.push_back(t.get());
  return t;
}

void Selector::Deregister(const Target& target) {
  for (auto iter = targets_.begin(); iter != targets_.end(); ++iter) {
    if (**iter == target) {
      targets_.erase(iter);
      return;
    }
  }
  // It is a logical error to deregister a target that is not registered.
  assert(false);
}

void Selector::Notify() {
  pthread::MutexLock m(notify_mutex_);
  notify_cv_.Signal();
}

vector<Target*> Selector::Select(const vector<Target*>& read_targets,
                                 const vector<Target*>& write_targets,
                                 const struct timespec* timeout) {
  struct timespec abstime;
  if (timeout != nullptr) {
    // Calculate absolute time for timeout. This should be done ASAP to reduce
    // the chances of this method not returning by the timeout specified. There
    // are no guarantees, of course.
    clock_gettime(CLOCK_REALTIME, &abstime);
    abstime.tv_sec += timeout->tv_sec;
    abstime.tv_nsec += timeout->tv_nsec;
  }

  pthread::MutexLock m(notify_mutex_);

  // Check if any data is available.
  auto result = HasData(read_targets, write_targets);
  if (result.size() > 0) {
    // Data available now; return immediately.
    return result;
  }

  // Wait for a target to have data. Simple no-timeout case.
  if (timeout == nullptr) {
    notify_cv_.Wait(&notify_mutex_);
    return HasData(read_targets, write_targets);
  }

  // Timeout case.
  for (;;) {
    int wait_errno = 0;
    if (!notify_cv_.TimedWait(&notify_mutex_, abstime)) {
      wait_errno = notify_cv_.GetLastError();
    }

    result = HasData(read_targets, write_targets);
    if (result.size() > 0) {
      // We have data... no need to check anything.
      return result;
    }

    if (wait_errno == ETIMEDOUT) {
      struct timespec now;
      clock_gettime(CLOCK_REALTIME, &now);
      if (now.tv_sec < abstime.tv_sec) {
        // Premature timeout. Retry.
        // TODO(rpwoodbu): Remove this hack once the NaCl bug that causes this
        // is fixed.
        wait_errno = 0;
        usleep(100000);
      } else {
        // We have a proper timeout. Return the empty result.
        return result;
      }
    } else {
      // Something went wrong. Avoid looping forever, though, and just return
      // the empty result.
      return result;
    }
  }
}

vector<Target*> Selector::HasData(const vector<Target*>& read_targets,
                                  const vector<Target*>& write_targets) const {
  vector<Target*> result;
  for (auto* target : read_targets) {
    if (target->has_read_data()) {
      result.push_back(target);
    }
  }
  for (auto* target : write_targets) {
    if (target->has_write_data()) {
      result.push_back(target);
    }
  }
  return result;
}

Target::~Target() { selector_.Deregister(*this); }

void Target::UpdateRead(bool has_data) {
  if (has_data == has_read_data_) {
    // No state change; do nothing.
    return;
  }

  has_read_data_ = has_data;
  if (has_data == true) {
    // Only notify if we now have data.
    selector_.Notify();
  }
}

void Target::UpdateWrite(bool has_data) {
  if (has_data == has_write_data_) {
    // No state change; do nothing.
    return;
  }

  has_write_data_ = has_data;
  if (has_data == true) {
    // Only notify if we now have data.
    selector_.Notify();
  }
}

}  // namespace PepperPosix
