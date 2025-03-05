/**
 * Copyright 2011-2020 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include <gtest/gtest.h>
#include <stdio.h>
#include <fstream>
#include "backend_test.hh"
#include "com/centreon/handle_listener.hh"
#include "com/centreon/handle_manager.hh"
#include "com/centreon/io/file_stream.hh"
#include "com/centreon/logging/engine.hh"
#include "com/centreon/logging/file.hh"
#include "com/centreon/logging/temp_logger.hh"

using namespace com::centreon;
using namespace com::centreon::logging;

class listener : public handle_listener {
 public:
  listener() {}
  ~listener() throw() {}
  void error(handle& h) { (void)h; }
};

static bool null_handle() {
  try {
    handle_manager hm;
    listener l;
    hm.add(NULL, &l);
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
    hm.add(&fs, NULL);
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
    listener l;
    hm.add(&fs, &l);
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
    listener l;
    hm.add(&fs, &l);
    try {
      hm.add(&fs, &l);
    } catch (std::exception const& e) {
      (void)e;
      return (true);
    }
  } catch (std::exception const& e) {
    (void)e;
  }
  return (false);
}

static bool is_same(backend const& b1, backend const& b2) {
  return (b1.enable_sync() == b2.enable_sync() &&
          b1.show_pid() == b2.show_pid() &&
          b1.show_timestamp() == b2.show_timestamp() &&
          b1.show_thread_id() == b2.show_thread_id());
}

static bool check_pid(std::string const& data, char const* msg) {
  if (data[0] != '[' || data.size() < 4)
    return (false);
  unsigned int pid_size(
      static_cast<unsigned int>(data.size() - strlen(msg) - 1 - 3));
  for (unsigned int i(1); i < pid_size; ++i)
    if (!isdigit(data[i]))
      return (false);
  if (data.compare(3 + pid_size, strlen(msg), msg))
    return (false);
  return (true);
}

static bool check_thread_id(std::string const& data, char const* msg) {
  void* ptr(NULL);
  char message[1024];

  int ret(sscanf(data.c_str(), "[%p] %s\n", &ptr, message));
  return (ret == 2 && !strncmp(msg, message, strlen(msg)));
}

/**
 *  Check time.
 *
 *  @return True on success, otherwise false.
 */
static bool check_time(std::string const& data, char const* msg) {
  if (data[0] != '[' || data.size() < 4)
    return (false);
  unsigned int time_size(
      static_cast<unsigned int>(data.size() - strlen(msg) - 1 - 3));
  for (unsigned int i(1); i < time_size; ++i)
    if (!isdigit(data[i]))
      return (false);
  if (data.compare(3 + time_size, strlen(msg), msg))
    return (false);
  return (true);
}

static bool null_pointer() {
  try {
    engine& e(engine::instance());
    e.add(NULL, 0, 0);
  } catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

static bool check_log_message(std::string const& path, std::string const& msg) {
  std::ifstream stream(path.c_str());
  char buffer[1024];
  stream.get(buffer, sizeof(buffer));
  return (buffer == msg);
}

static bool check_log_message2(std::string const& path,
                               std::string const& msg) {
  std::ifstream stream(path.c_str());
  char buffer[32 * 1024];
  memset(buffer, 0, sizeof(buffer));
  stream.read(buffer, sizeof(buffer));
  return buffer == msg;
}

TEST(ClibLogging, HandleManagerAdd) {
  ASSERT_TRUE(null_handle());
  ASSERT_TRUE(null_listener());
  ASSERT_TRUE(basic_add());
  ASSERT_TRUE(double_add());
}

TEST(ClibLogging, BackendCopy) {
  backend_test ref(false, true, none, false);

  backend_test c1(ref);
  ASSERT_TRUE(is_same(ref, c1));
  backend_test c2 = ref;
  ASSERT_TRUE(is_same(ref, c2));
}

TEST(ClibLogging, BackendWithPid) {
  static char msg[] = "Centreon Clib test";
  engine& e(engine::instance());

  std::unique_ptr<backend_test> obj(new backend_test(false, true, none, false));
  auto id = e.add(obj.get(), 1, 0);

  e.log(1, 0, msg, sizeof(msg));

  ASSERT_TRUE(check_pid(obj->data(), msg));
  e.remove(id);
}

TEST(ClibLogging, BackendWithThreadId) {
  static char msg[] = "Centreon_Clib_test";
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test(false, false, none, true));
  auto id = e.add(obj.get(), 1, 0);
  e.log(1, 0, msg, sizeof(msg));
  ASSERT_TRUE(check_thread_id(obj->data(), msg));
  e.remove(id);
}

TEST(ClibLogging, BackendWithTimestamp) {
  static char msg[] = "Centreon Clib test";

  engine& e(engine::instance());

  std::unique_ptr<backend_test> obj(
      new backend_test(false, false, none, false));
  auto id = e.add(obj.get(), 1, 0);
  obj->show_timestamp(second);
  e.log(1, 0, msg, sizeof(msg));
  ASSERT_TRUE(check_time(obj->data(), msg));
  obj->reset();

  obj->show_timestamp(millisecond);
  e.log(1, 0, msg, sizeof(msg));
  ASSERT_TRUE(check_time(obj->data(), msg));

  obj->reset();

  obj->show_timestamp(microsecond);
  e.log(1, 0, msg, sizeof(msg));
  ASSERT_TRUE(check_time(obj->data(), msg));
  obj->reset();
  e.remove(id);
}

TEST(ClibLogging, EngineAdd) {
  engine& e(engine::instance());
  ASSERT_TRUE(null_pointer());

  std::unique_ptr<backend_test> obj(new backend_test);
  unsigned long id(e.add(obj.get(), 0, 0));
  ASSERT_TRUE(id);
  e.remove(id);
}

TEST(ClibLogging, EngineIsLog) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);

  unsigned int limits(sizeof(unsigned int) * CHAR_BIT);
  for (unsigned int i(0); i < 3; ++i) {
    for (unsigned int j(0); j < limits; ++j) {
      unsigned long id(e.add(obj.get(), 1 << j, i));
      for (unsigned int k(0); k < limits; ++k) {
        ASSERT_EQ(e.is_log(1 << k, i), (k == j));

        for (unsigned int k(0); k < 3; ++k) {
          ASSERT_EQ(e.is_log(1 << j, k), (i >= k));
        }
      }
      ASSERT_TRUE(e.remove(id));
    }
  }
}

TEST(ClibLogging, EngineLog) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);

  e.log(1, 0, NULL, 0);

  unsigned int limits(sizeof(unsigned int) * CHAR_BIT);

  for (unsigned int i(0); i < 3; ++i) {
    for (unsigned int j(0); j < limits; ++j) {
      unsigned long id(e.add(obj.get(), 1 << j, i));
      for (unsigned int k(0); k < limits; ++k)
        e.log(1 << k, i, "", 0);
      ASSERT_TRUE(e.remove(id));
    }
  }

  ASSERT_EQ(obj->get_nb_call(), 3 * limits);
}

TEST(ClibLogging, EngineRemoveByBackend) {
  engine& e(engine::instance());
  ASSERT_TRUE(null_pointer());

  std::unique_ptr<backend_test> obj(new backend_test);
  e.add(obj.get(), 1, 0);
  ASSERT_EQ(e.remove(obj.get()), 1u);

  constexpr uint32_t nb_backend{1000};
  for (unsigned int i(1); i < nb_backend; ++i)
    e.add(obj.get(), i, 0);

  ASSERT_EQ(e.remove(obj.get()), nb_backend - 1);
}

TEST(ClibLogging, EngineRemoveById) {
  engine& e(engine::instance());
  ASSERT_FALSE(e.remove(1));
  ASSERT_FALSE(e.remove(42));

  std::unique_ptr<backend_test> obj(new backend_test);
  unsigned long id(e.add(obj.get(), 1, 0));

  ASSERT_TRUE(e.remove(id));
}

TEST(ClibLogging, EngineWithThread) {
  static uint32_t const nb_writter(10);

  engine& e(engine::instance());

  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 1, 0);

  std::vector<std::thread> threads;
  for (uint32_t i = 0; i < nb_writter; ++i)
    threads.push_back(std::thread([]() {
      engine& e(engine::instance());
      for (uint32_t i = 0; i < nb_writter; ++i) {
        std::ostringstream oss;
        oss << std::this_thread::get_id() << ":" << i;
        e.log(1, 0, oss.str().c_str(), oss.str().size());
      }
    }));

  for (auto& t : threads)
    t.join();

  for (uint32_t i(0); i < nb_writter; ++i) {
    for (uint32_t j(0); j < nb_writter; ++j) {
      std::ostringstream oss;
      oss << &threads[i] << ":" << j << "\n";
      ASSERT_TRUE(obj->data().find(oss.str()));
    }
  }
  e.remove(id);
}

TEST(ClibLogging, FileLog) {
  static char msg[] = "Centreon Clib test";

  std::string tmp(com::centreon::io::file_stream::temp_path());
  {
    file f(tmp, false, false, none, false);
    f.log(1, 0, msg, sizeof(msg));
  }
  ASSERT_TRUE(check_log_message(tmp, msg));

  {
    FILE* out(NULL);
    ASSERT_TRUE((out = fopen(tmp.c_str(), "w")));
    file f(out, false, false, none, false);
    f.log(1, 0, msg, sizeof(msg));
  }
  ASSERT_TRUE(check_log_message(tmp, msg));
}

TEST(ClibLogging, FileLogMultiline) {
  static unsigned int const nb_line(1024);

  std::string tmpfile(com::centreon::io::file_stream::temp_path());

  std::ostringstream tmp;
  std::ostringstream tmpref;
  for (unsigned int i(0); i < nb_line; ++i) {
    tmp << i << "\n";
    tmpref << "[" << std::this_thread::get_id() << "] " << i << "\n";
  }
  std::string msg(tmp.str());
  std::string ref(tmpref.str());

  {
    file f(tmpfile, false, false, none, true);
    f.log(1, 0, msg.c_str(), msg.size());
  }
  ASSERT_TRUE(check_log_message2(tmpfile, ref));
}

TEST(ClibLogging, TempLoggerCtor) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 3, 0);

  temp_logger(0, 0) << "Centreon Clib test";
  ASSERT_EQ(obj->get_nb_call(), 0u);

  temp_logger(1, 0) << "Centreon Clib test";
  ASSERT_EQ(obj->get_nb_call(), 1u);
  e.remove(id);
}

TEST(ClibLogging, TempLoggerCopy) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 3, 0);

  temp_logger tmp(1, 0);
  tmp << "Centreon Clib test";
  temp_logger(tmp) << " copy";
  ASSERT_NE(obj->data().find("copy"), std::string::npos);
  e.remove(id);
}

TEST(ClibLogging, TempLoggerDoNothingChar) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 2, 0);

  temp_logger(1, 2) << 'c';
  ASSERT_FALSE(obj->get_nb_call());
  e.remove(id);
}

TEST(ClibLogging, TempLoggerDoNothingDouble) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 2, 0);

  temp_logger(1, 2) << double(42.42);
  ASSERT_FALSE(obj->get_nb_call());
  e.remove(id);
}

TEST(ClibLogging, TempLoggerDoNothingInt) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 2, 0);

  temp_logger(1, 2) << int(42);
  ASSERT_FALSE(obj->get_nb_call());
  e.remove(id);
}

TEST(ClibLogging, TempLoggerDoNothingLong) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 2, 0);

  temp_logger(1, 2) << 42L;
  ASSERT_FALSE(obj->get_nb_call());
  e.remove(id);
}

TEST(ClibLogging, TempLoggerDoNothingLongLong) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 2, 0);

  temp_logger(1, 2) << 42LL;
  ASSERT_FALSE(obj->get_nb_call());
  e.remove(id);
}

TEST(ClibLogging, TempLoggerDoNothingPVoid) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 2, 0);

  temp_logger(1, 2) << obj.get();
  ASSERT_FALSE(obj->get_nb_call());
  e.remove(id);
}

TEST(ClibLogging, TempLoggerDoNothingStdString) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 2, 0);

  temp_logger(1, 2) << std::string("Centreon Clib test");
  ASSERT_FALSE(obj->get_nb_call());
  e.remove(id);
}

TEST(ClibLogging, TempLoggerDoNothingString) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 2, 0);

  temp_logger(1, 2) << "Centreon Clib test";
  ASSERT_FALSE(obj->get_nb_call());
  e.remove(id);
}

TEST(ClibLogging, TempLoggerDoNothingUint) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 2, 0);

  unsigned int ui(42);
  temp_logger(1, 2) << ui;
  ASSERT_FALSE(obj->get_nb_call());
  e.remove(id);
}

TEST(ClibLogging, TempLoggerDoNothingULong) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 2, 0);

  unsigned long ul(42);
  temp_logger(1, 2) << ul;
  ASSERT_FALSE(obj->get_nb_call());
  e.remove(id);
}

TEST(ClibLogging, TempLoggerDoNothingULongLong) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 2, 0);

  unsigned long long ull(42);
  temp_logger(1, 2) << ull;
  ASSERT_FALSE(obj->get_nb_call());
  e.remove(id);
}

TEST(ClibLogging, TempLoggerLogChar) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 3, 0);

  temp_logger(1, 0) << 'c';
  ASSERT_EQ(obj->get_nb_call(), 1u);
  e.remove(id);
}

TEST(ClibLogging, TempLoggerLogDouble) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 3, 0);

  temp_logger(1, 0) << double(42.42);
  ASSERT_EQ(obj->get_nb_call(), 1u);
  e.remove(id);
}

TEST(ClibLogging, TempLoggerLogInt) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 3, 0);

  temp_logger(1, 0) << int(42);
  ASSERT_EQ(obj->get_nb_call(), 1u);
  e.remove(id);
}

TEST(ClibLogging, TempLoggerLogLong) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 3, 0);

  temp_logger(1, 0) << 42L;
  ASSERT_EQ(obj->get_nb_call(), 1u);
  e.remove(id);
}

TEST(ClibLogging, TempLoggerLogLongLong) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 3, 0);

  temp_logger(1, 0) << 42LL;
  ASSERT_EQ(obj->get_nb_call(), 1u);
  e.remove(id);
}

TEST(ClibLogging, TempLoggerLogPVoid) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 3, 0);

  temp_logger(1, 0) << obj.get();
  ASSERT_EQ(obj->get_nb_call(), 1u);
  e.remove(id);
}

TEST(ClibLogging, TempLoggerLogStdString) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 3, 0);

  temp_logger(1, 0) << std::string("Centreon Clib test");
  ASSERT_EQ(obj->get_nb_call(), 1u);
  e.remove(id);
}

TEST(ClibLogging, TempLoggerLogString) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 3, 0);

  temp_logger(1, 0) << "Centreon Clib test";
  ASSERT_EQ(obj->get_nb_call(), 1u);
  e.remove(id);
}

TEST(ClibLogging, TempLoggerLogUint) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 3, 0);

  unsigned int ui(42);
  temp_logger(1, 0) << ui;
  ASSERT_EQ(obj->get_nb_call(), 1u);
  e.remove(id);
}

TEST(ClibLogging, TempLoggerLogULong) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 3, 0);

  unsigned long ul(42);
  temp_logger(1, 0) << ul;
  ASSERT_EQ(obj->get_nb_call(), 1u);
  e.remove(id);
}

TEST(ClibLogging, TempLoggerLogULongLong) {
  engine& e(engine::instance());
  std::unique_ptr<backend_test> obj(new backend_test);
  auto id = e.add(obj.get(), 3, 0);

  unsigned long long ull(42);
  temp_logger(1, 0) << ull;
  ASSERT_EQ(obj->get_nb_call(), 1u);
  e.remove(id);
}
