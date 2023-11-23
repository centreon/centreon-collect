/*
** Copyright 2019 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_MISC_SHARED_MUTEX_HH
#define CCB_MISC_SHARED_MUTEX_HH

#include <pthread.h>

namespace com::centreon::broker {

namespace misc {
class shared_mutex {
  pthread_rwlock_t _rwlock;

 public:
  shared_mutex() : _rwlock(PTHREAD_RWLOCK_INITIALIZER) {}

  ~shared_mutex() { pthread_rwlock_destroy(&_rwlock); }

  // Exclusive ownership

  void lock() { pthread_rwlock_wrlock(&_rwlock); }

  bool try_lock() { return pthread_rwlock_trywrlock(&_rwlock) == 0; }

  bool try_lock_for(int ms) {
    struct timespec timeout {
      .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000
    };
    return pthread_rwlock_timedwrlock(&_rwlock, &timeout) == 0;
  }

  void unlock() { pthread_rwlock_unlock(&_rwlock); }

  // Shared ownership

  void lock_shared() { pthread_rwlock_rdlock(&_rwlock); }

  bool try_lock_shared() { return pthread_rwlock_trywrlock(&_rwlock) == 0; }

  bool try_lock_shared_for(int ms) {
    struct timespec timeout {
      .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000
    };
    return pthread_rwlock_timedrdlock(&_rwlock, &timeout) == 0;
  }
};

class read_lock {
 private:
  shared_mutex& _m;
  bool _locked;

 public:
  explicit read_lock(shared_mutex& m) : _m(m), _locked{true} {
    _m.lock_shared();
  }
  ~read_lock() noexcept {
    if (_locked)
      _m.unlock();
  }

  void unlock() {
    _m.unlock();
    _locked = false;
  }
};

}  // namespace misc

}  // namespace com::centreon::broker

#endif /* CCB_MISC_SHARED_MUTEX_HH */
