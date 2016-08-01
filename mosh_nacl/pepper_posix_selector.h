// pepper_posix_selector.h - Selector for Pepper POSIX adapters.
//
// Pepper POSIX is a set of adapters to enable POSIX-like APIs to work with the
// callback-based APIs of Pepper (and transitively, JavaScript).

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

#ifndef MOSH_NACL_PEPPER_POSIX_SELECTOR_H_
#define MOSH_NACL_PEPPER_POSIX_SELECTOR_H_

#include <memory>
#include <vector>

#include "mosh_nacl/pthread_locks.h"

namespace PepperPOSIX {

class Target;  // This is declared fully below.

// Selector implements select()-style functionality for callback-style I/O.
// In your I/O implementation, get and retain a Target instance by calling
// NewTarget(). Call Target's UpdateRead() or UpdateWrite() methods whenever
// availability of data changes. Other threads can then call Selector's
// Select() or SelectAll() to block until data is available.
class Selector {
 public:
  Selector();
  ~Selector();
  friend class Target;  // Allow Target to access private members of Selector.

  // NewTarget creates a new Target instance for use by an I/O class. The
  // returned Target is owned by the caller, but Selector maintains a pointer
  // to it for the life of the Target. It is an error to delete Selector until
  // all Targets are deleted. id is an opaque identifier for the user to
  // distinguish one Target from another.
  std::unique_ptr<Target> NewTarget(int id);

  // Select returns a subset of targets for which data is available, or
  // waits until the timeout period has passed. It calls
  // pthread_cond_timedwait() if there are no targets with data available
  // when the method is called.
  //
  // Vectors of pointers were chosen for simplicity, even though ownership
  // looks ambiguous. The alternative would have been
  // std::vector<std::reference_wrapper<Target>>, which is messy at best.
  // Select() does not take or convey ownership.
  std::vector<Target*> Select(const std::vector<Target*>& read_targets,
                              const std::vector<Target*>& write_targets,
                              const struct timespec* timeout);

  // SelectAll is similar to Select, but waits for all registered targets.
  std::vector<Target*> SelectAll(const struct timespec* timeout) {
    return Select(targets_, targets_, timeout);
  }

 private:
  // Notify is to be called only from class Target to indicate when
  // there is data available. Internally, Notify uses
  // pthread_cond_signal() to indicate that there is data. It does not
  // need to procure a lock, as it modifies no internal state.
  void Notify();

  // Deregister is to be called only from the class Target when it is
  // being destroyed and must deregister with Selector.
  void Deregister(const Target& target);

  // HasData returns a vector of Targets that have data ready to be read.
  std::vector<Target*> HasData(const std::vector<Target*>& read_targets,
                               const std::vector<Target*>& write_targets) const;

  std::vector<Target*> targets_;  // Does not own Targets!
  pthread::Mutex notify_mutex_;
  pthread::Conditional notify_cv_;

  // Disable copy and assignment.
  Selector(const Selector&) = delete;
  Selector& operator=(const Selector&) = delete;
};

// Target is used by an I/O "target" to communicate with a Selector instance
// for emulating POSIX select()-style functionality. The I/O target should call
// UpdateRead() or UpdateWrite() whenever data availability changes.
class Target {
 public:
  // We default has_write_data_ to true, as many targets never block on writes.
  Target(class Selector& s, int id) : selector_(s), id_(id) {}
  ~Target();

  // UpdateRead updates Target whether there is pending data available in the
  // I/O target. If the state has changed, Target notifies Selector of the
  // change.  You can call UpdateRead even if the state has not changed since
  // the last call to UpdateRead, as UpdateRead will detect that and not send
  // superfluous notifications to Selector.
  void UpdateRead(bool has_data);

  // UpdateWrite updates Target whether new data can be accepted by the I/O
  // target. If the state has changed, Target notifies Selector of the change.
  // You can call UpdateWrite even if the state has not changed since the last
  // call to UpdateWrite, as UpdateWrite will detect that and not send
  // superfluous notifications to Selector.
  void UpdateWrite(bool has_data);

  bool has_read_data() const { return has_read_data_; }
  bool has_write_data() const { return has_write_data_; }
  int id() const { return id_; }
  bool operator==(const Target& rh) { return id() == rh.id(); }

 private:
  class Selector& selector_;
  int id_ = -1;
  bool has_read_data_ = false;
  bool has_write_data_ = true;

  // Disable copy and assignment.
  Target(const Target&) = delete;
  Target& operator=(const Target&) = delete;
};

}  // namespace PepperPosix

#endif  // MOSH_NACL_PEPPER_POSIX_SELECTOR_H_
