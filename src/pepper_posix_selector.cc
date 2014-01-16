// pepper_posix_selector.cc - Selector for Pepper POSIX adapters.

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

#include "pepper_posix_selector.h"

#include <algorithm>
#include <assert.h>

namespace PepperPOSIX {

Selector::Selector() {
  pthread_mutex_init(&notify_mutex_, NULL);
  pthread_cond_init(&notify_cv_, NULL);
}

Selector::~Selector() {
  // It is a logical error to delete Selector before all Targets have been
  // deleted.
  assert(targets_.size() == 0);
  pthread_cond_destroy(&notify_cv_);
  pthread_mutex_destroy(&notify_mutex_);
}

Target *Selector::NewTarget(int id) {
  Target *t = new Target(this, id);
  targets_.push_back(t);
  return t;
}

void Selector::Deregister(const Target *target) {
  vector<Target*>::iterator t = std::find(
      targets_.begin(), targets_.end(), target);
  // It is a logical error to deregister a target that is not registered.
  assert(*t == target);
  targets_.erase(t);
}

void Selector::Notify() {
  pthread_cond_signal(&notify_cv_);
}

vector<Target*> Selector::Select(
    const vector<Target*> &read_targets, const vector<Target*> &write_targets,
    const struct timespec *timeout) {
  // Calculate absolute time for timeout. This should be done ASAP to reduce
  // the chances of this method not returning by the timeout specified. There
  // are no guarantees, of course.
  struct timespec abstime;
  clock_gettime(CLOCK_REALTIME, &abstime);
  if (timeout != NULL) {
    abstime.tv_sec += timeout->tv_sec;
    abstime.tv_nsec += timeout->tv_nsec;
  }

  // Check if any data is available.
  vector<Target*> result = HasData(read_targets, write_targets);
  if (result.size() > 0) {
    // Data available now; return immediately.
    return result;
  }

  // Wait for a target to have data.
  pthread_mutex_lock(&notify_mutex_);
  if (timeout == NULL) {
    pthread_cond_wait(&notify_cv_, &notify_mutex_);
  } else {
    pthread_cond_timedwait(&notify_cv_, &notify_mutex_, &abstime);
  }
  pthread_mutex_unlock(&notify_mutex_);

  // Must check again to see who has data.
  return HasData(read_targets, write_targets);
}

const vector<Target*> Selector::HasData(
    const vector<Target*> &read_targets, const vector<Target*> &write_targets) {
  vector<Target*> result;
  for (vector<Target*>::const_iterator t = read_targets.begin();
      t != read_targets.end(); ++t) {
    if ((*t)->has_read_data()) {
      result.push_back(*t);
    }
  }
  for (vector<Target*>::const_iterator t = write_targets.begin();
      t != write_targets.end(); ++t) {
    if ((*t)->has_write_data()) {
      result.push_back(*t);
    }
  }
  return result;
}

Target::~Target() {
  selector_->Deregister(this);
}

void Target::UpdateRead(bool has_data) {
  if (has_data == has_read_data_) {
    // No state change; do nothing.
    return;
  }

  has_read_data_ = has_data;
  if (has_data == true) {
    // Only notify if we now have data.
    selector_->Notify();
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
    selector_->Notify();
  }
}

} // namespace PepperPosix
