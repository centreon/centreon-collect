/**
 * Copyright 2020 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */
#include "com/centreon/handle_manager.hh"
#include <gtest/gtest.h>
#include <stdio.h>
#include "com/centreon/handle_listener.hh"
#include "com/centreon/io/file_stream.hh"
#include "com/centreon/task_manager.hh"

using namespace com::centreon;

class listener : public handle_listener {
 public:
  listener() = delete;
  listener(handle& ref_h, bool want_read, bool want_write)
      : _is_call(false),
        _ref_h(ref_h),
        _want_read(want_read),
        _want_write(want_write) {}
  ~listener() noexcept {}
  void error(handle& h) {
    if (&_ref_h == &h)
      _is_call = true;
  }
  bool is_call() const throw() { return (_is_call); }
  void read(handle& h) {
    if (&_ref_h == &h)
      _is_call = true;
  }
  bool want_read(handle& h) {
    (void)h;
    return (_want_read);
  }
  bool want_write(handle& h) {
    (void)h;
    return (_want_write);
  }
  void write(handle& h) {
    if (&_ref_h == &h)
      _is_call = true;
  }

 private:
  bool _is_call;
  handle& _ref_h;
  bool _want_read;
  bool _want_write;
};

static bool null_task_manager() {
  try {
    handle_manager hm;
    hm.multiplex();
  } catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

static bool basic_multiplex_write() {
  task_manager tm;
  handle_manager hm(&tm);
  io::file_stream fs(stdout);
  listener l(fs, false, true);
  hm.add(&fs, &l);
  hm.multiplex();
  int toReturn(l.is_call());
  hm.remove(&fs);
  return toReturn;
}

static bool empty_handle_manager() {
  try {
    task_manager tm;
    handle_manager hm(&tm);
    hm.multiplex();
  } catch (std::exception const& e) {
    (void)e;
    return (false);
  }
  return (true);
}

static bool basic_multiplex_close() {
  task_manager tm;
  handle_manager hm(&tm);
  io::file_stream fs(stdin);
  listener l(fs, true, true);
  hm.add(&fs, &l);
  hm.multiplex();
  int toReturn(l.is_call());
  hm.remove(&fs);
  return toReturn;
}

static bool null_handle() {
  try {
    handle_manager hm;
    io::file_stream fs(stdin);
    listener l(fs, true, true);
    hm.add(nullptr, &l);
  } catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

static bool null_listener() {
  try {
    handle_manager hm;
    io::file_stream fs;
    hm.add(&fs, nullptr);
  } catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

static bool basic_add() {
  try {
    handle_manager hm;

    io::file_stream fs(stdin);
    listener l(fs, false, false);
    hm.add(&fs, &l);
    hm.remove(&fs);
  } catch (std::exception const& e) {
    (void)e;
    return (false);
  }
  return (true);
}

static bool double_add() {
  try {
    handle_manager hm;

    io::file_stream fs(stdin);
    listener l(fs, false, false);
    hm.add(&fs, &l);
    try {
      hm.add(&fs, &l);
    } catch (std::exception const& e) {
      (void)e;
      hm.remove(&fs);
      return (true);
    }
  } catch (std::exception const& e) {
    (void)e;
  }
  return (false);
}

TEST(ClibHandleManager, Add) {
  ASSERT_TRUE(null_handle());
  ASSERT_TRUE(null_listener());
  ASSERT_TRUE(basic_add());
  ASSERT_TRUE(double_add());
}

TEST(ClibHandleManager, Ctor) {
  ASSERT_NO_THROW({ handle_manager hm(nullptr); }

                  {
                    task_manager tm;
                    handle_manager hm(&tm);
                  });
}

TEST(ClibHandleManager, Multiplex) {
  ASSERT_TRUE(null_task_manager());
  ASSERT_TRUE(empty_handle_manager());
  ASSERT_TRUE(basic_multiplex_write());
  ASSERT_TRUE(basic_multiplex_close());
}

TEST(ClibHandleManager, RemoveByHandle) {
  handle_manager hm;
  ASSERT_FALSE(hm.remove(static_cast<handle*>(nullptr)));

  io::file_stream fs(stdin);
  ASSERT_FALSE(hm.remove(&fs));

  listener l(fs, false, false);
  hm.add(&fs, &l);
  ASSERT_TRUE(hm.remove(&fs));
}

TEST(ClibHandleManager, RemoveByHandleListener) {
  handle_manager hm;
  ASSERT_FALSE(hm.remove(static_cast<handle_listener*>(nullptr)));

  io::file_stream fs(stdout);
  listener l(fs, false, false);
  ASSERT_FALSE(hm.remove(&l));

  io::file_stream fs1(stdin);
  hm.add(&fs1, &l);
  ASSERT_EQ(hm.remove(&l), 1u);

  hm.add(&fs1, &l);
  io::file_stream fs2(stdout);
  hm.add(&fs2, &l);
  io::file_stream fs3(stderr);
  hm.add(&fs3, &l);

  ASSERT_EQ(hm.remove(&l), 3u);
}
