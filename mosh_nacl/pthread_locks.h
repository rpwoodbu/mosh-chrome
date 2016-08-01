// pthread_locks.h - C++ wrapper around pthread locking constructs.
//
// This primarily adds RAII semantics to pthread locking facilities.

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

#ifndef MOSH_NACL_PTHREAD_LOCKS_H_
#define MOSH_NACL_PTHREAD_LOCKS_H_

#include <pthread.h>

namespace pthread {

class Mutex {
  friend class Conditional;

 public:
  Mutex() { pthread_mutex_init(&mutex_, nullptr); }
  ~Mutex() { pthread_mutex_destroy(&mutex_); }

  bool Lock() {
    err_ = pthread_mutex_lock(&mutex_);
    return err_ == 0;
  }

  bool Unlock() {
    err_ = pthread_mutex_unlock(&mutex_);
    return err_ == 0;
  }

  int GetLastError() const { return err_; }

 private:
  pthread_mutex_t mutex_;
  int err_ = 0;

  // Disable copy and assignment.
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;
};

// Use this class to aquire a Mutex and release it automatically via scoping.
class MutexLock {
 public:
  explicit MutexLock(Mutex& m) : m_(m) { m_.Lock(); }
  ~MutexLock() { m_.Unlock(); }

 private:
  Mutex& m_;

  // Disable copy and assignment.
  MutexLock(const MutexLock&) = delete;
  MutexLock& operator=(const MutexLock&) = delete;
};

class Conditional {
 public:
  Conditional() { pthread_cond_init(&cv_, nullptr); }
  ~Conditional() { pthread_cond_destroy(&cv_); }

  bool Signal() {
    err_ = pthread_cond_signal(&cv_);
    return err_ == 0;
  }

  bool Broadcast() {
    err_ = pthread_cond_broadcast(&cv_);
    return err_ == 0;
  }

  bool Wait(Mutex* m) {
    err_ = pthread_cond_wait(&cv_, &m->mutex_);
    return err_ == 0;
  }

  bool TimedWait(Mutex* m, const struct timespec& abstime) {
    err_ = pthread_cond_timedwait(&cv_, &m->mutex_, &abstime);
    return err_ == 0;
  }

  int GetLastError() const { return err_; }

 private:
  pthread_cond_t cv_;
  int err_ = 0;

  // Disable copy and assignment.
  Conditional(const Conditional&) = delete;
  Conditional& operator=(const Conditional&) = delete;
};

}  // namespace pthread

#endif  // MOSH_NACL_PTHREAD_LOCKS_H_
