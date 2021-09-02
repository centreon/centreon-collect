/*
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
#include "com/centreon/process.hh"
#include <gtest/gtest.h>
#include <cstdlib>
#include <future>
#include <iostream>
#include "com/centreon/clib.hh"
#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon;

TEST(ClibProcess, ProcessEnv) {
  process p;
  char* env[] = {(char*)"key1=value1", (char*)"key2=value2",
                 (char*)"key3=value3", NULL};
  p.exec(
      "./test/bin_test_process_output check_env "
      "key1=value1 key2=value2 key3=value3",
      env);
  p.wait();
  ASSERT_EQ(p.exit_code(), EXIT_SUCCESS);
}

TEST(ClibProcess, ProcessKill) {
  process p;
  p.exec("./test/bin_test_process_output check_sleep 1");
  p.kill();
  timestamp start(timestamp::now());
  p.wait();
  timestamp end(timestamp::now());
  ASSERT_EQ((end - start).to_seconds(), 0);
}

TEST(ClibProcess, ProcessOutput) {
  const char* argv[2] = {"err", "out"};

  std::string cmd("./test/bin_test_process_output check_output ");
  cmd += argv[1];

  process p;
  p.exec(cmd);
  char buffer_write[16 * 1024];
  std::string buffer_read;
  for (unsigned int i(0); i < sizeof(buffer_write); ++i)
    buffer_write[i] = static_cast<char>(i);
  unsigned int total_read(0);
  unsigned int total_write(0);
  do {
    if (total_write < sizeof(buffer_write))
      total_write += p.write(buffer_write, sizeof(buffer_write) - total_write);
    if (!strcmp(argv[1], "out"))
      p.read(buffer_read);
    else
      p.read_err(buffer_read);
    total_read += buffer_read.size();
  } while (total_read < sizeof(buffer_write));
  p.update_ending_process(0);
  p.wait();
  ASSERT_EQ(p.exit_code(), EXIT_SUCCESS);
  ASSERT_EQ(total_write, sizeof(buffer_write));
  ASSERT_EQ(total_write, total_read);
}

TEST(ClibProcess, ProcessOutputThWrite) {
  const char* argv[2]{"err", "out"};

  std::string cmd("./test/bin_test_process_output check_output ");
  cmd += argv[1];

  static constexpr size_t size = 10 * 1024;
  static constexpr size_t count = 30;

  process p;
  std::promise<bool> prm;
  std::future<bool> running = prm.get_future();
  std::thread th_exec([&p, &cmd, &prm] {
    p.exec(cmd);
    prm.set_value(true);
  });

  running.get();

  std::thread th_write([&p] {
    char buffer_write[size + 1];
    buffer_write[size] = 0;
    for (unsigned int i = 0; i < size; ++i)
      buffer_write[i] = static_cast<char>('A' + i % 26);

    for (int i = 0; i < count; i++) {
      unsigned int total_write = 0;
      do {
        if (total_write < size)
          total_write +=
              p.write(buffer_write + total_write, size - total_write);
      } while (total_write < size);
    }
    std::cout << "W ";
    for (int i = 0; i < count; i++)
      std::cout << buffer_write;
    std::cout << std::endl;
  });

  std::string buffer_read;
  do {
    std::string data;
    if (!strcmp(argv[1], "out"))
      p.read(data);
    else
      p.read_err(data);
    buffer_read += data;
  } while (buffer_read.size() < size * count);

  th_exec.join();
  th_write.join();
  p.update_ending_process(0);
  p.wait();

  std::cout << "R " << buffer_read << std::endl;
  ASSERT_EQ(buffer_read.size(), size * count);
  for (int i = 0, j = 0; i < size * count; i++) {
    ASSERT_EQ(static_cast<char>('A' + j % 26), buffer_read[i]);
    if (++j == size)
      j = 0;
  }

  ASSERT_EQ(p.exit_code(), EXIT_SUCCESS);
}

TEST(ClibProcess, ProcessOutputThRead) {
  const char* argv[2]{"err", "out"};

  std::string cmd("./test/bin_test_process_output check_output ");
  cmd += argv[1];

  /* out or err? */
  bool out = !strcmp(argv[1], "out");

  static constexpr size_t size = 10 * 1024;
  static constexpr size_t count = 30;

  process p;
  std::promise<bool> prm;
  std::future<bool> running = prm.get_future();
  std::thread th_exec([&p, &cmd, &prm] {
    p.exec(cmd);
    prm.set_value(true);
  });

  running.get();

  char buffer_write[size + 1];
  buffer_write[size] = 0;
  for (unsigned int i = 0; i < size; ++i)
    buffer_write[i] = static_cast<char>('A' + i % 26);

  for (int i = 0; i < count; i++) {
    unsigned int total_write = 0;
    do {
      if (total_write < size)
        total_write += p.write(buffer_write + total_write, size - total_write);
    } while (total_write < size);
  }

  std::cout << "W ";
  for (int i = 0; i < count; i++)
    std::cout << buffer_write;
  std::cout << std::endl;

  std::thread th_read([&p, &out] {
    std::string buffer_read;
    do {
      std::string data;
      if (out)
        p.read(data);
      else
        p.read_err(data);
      buffer_read += data;
    } while (buffer_read.size() < size * count);

    std::cout << "R " << buffer_read << std::endl;
    for (int i = 0, j = 0; i < size * count; i++) {
      ASSERT_EQ(static_cast<char>('A' + j % 26), buffer_read[i]);
      if (++j == size)
        j = 0;
    }
  });

  th_exec.join();
  th_read.join();
  p.update_ending_process(0);
  p.wait();

  ASSERT_EQ(p.exit_code(), EXIT_SUCCESS);
}

TEST(ClibProcess, ProcessReturn) {
  process p;
  p.exec("./test/bin_test_process_output check_return 42");
  p.wait();
  ASSERT_EQ(p.exit_code(), 42);
}

TEST(ClibProcess, ProcessTerminate) {
  process p;
  p.exec("./test/bin_test_process_output check_sleep 1");
  p.terminate();
  timestamp start(timestamp::now());
  p.wait();
  timestamp end(timestamp::now());
  ASSERT_EQ((end - start).to_seconds(), 0);
}

TEST(ClibProcess, ProcessTimeout) {
  process p;
  p.exec("./test/bin_test_process_output check_sleep 5", NULL, 1);
  p.wait();
  timestamp exectime(p.end_time() - p.start_time());
  ASSERT_LT(exectime.to_seconds(), 2);
}

TEST(ClibProcess, ProcessWaitTimeout) {
  process p;
  p.exec("./test/bin_test_process_output check_sleep 1");
  ASSERT_FALSE(p.wait(500) == true);
  ASSERT_FALSE(p.wait(1500) == false);
}
