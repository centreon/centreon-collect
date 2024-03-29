/**
* Copyright 2012-2013,2015 Centreon
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

#include "com/centreon/broker/misc/misc.hh"

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <random>
#include <stdexcept>

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;

/**
 *  Get a temporary file path.
 *
 *  @return Temporary file path.
 */
std::string misc::temp_path() {
  char path[] = "/tmp/brokerXXXXXX";
  int retval(mkstemp(path));
  if (retval < 0) {
    char const* err_msg(strerror(errno));
    throw msg_fmt("cannot create temporary file: {} ", err_msg);
  }
  ::close(retval);
  ::remove(path);
  return (path);
}

static const uint16_t crc_tbl[16] = {
    0x0000, 0x1081, 0x2102, 0x3183, 0x4204, 0x5285, 0x6306, 0x7387,
    0x8408, 0x9489, 0xa50a, 0xb58b, 0xc60c, 0xd68d, 0xe70e, 0xf78f};

/**
 *
 * Return a crc16 checksum of the given string
 * @param data The string to create the checksum from.
 * @param data_len The length of data to consider.
 *
 * @return The checksum
 */
uint16_t misc::crc16_ccitt(char const* data, uint32_t data_len) {
  uint16_t crc = 0xffff;
  const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
  while (data_len--) {
    uint8_t c = *p++;
    crc = ((crc >> 4) & 0x0fff) ^ crc_tbl[((crc ^ c) & 15)];
    c >>= 4;
    crc = ((crc >> 4) & 0x0fff) ^ crc_tbl[((crc ^ c) & 15)];
  }
  return ~crc & 0xffff;
}

std::string misc::exec(std::string const& cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"),
                                                pclose);
  if (!pipe)
    throw std::runtime_error("popen() failed!");

  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

int32_t misc::exec_process(char const** argv, bool wait_for_completion) {
  int status;
  pid_t my_pid{fork()};
  if (my_pid == 0) {
    int res = execve(argv[0], const_cast<char**>(argv), nullptr);
    if (res == -1) {
      perror("child process failed [%m]");
      return -1;
    }
  }

  if (wait_for_completion) {
    int timeout = 20;
    while (waitpid(my_pid, &status, WNOHANG)) {
      if (--timeout < 0) {
        perror("timeout reached during execution");
        return -1;
      }
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (WIFEXITED(status) != 1 || WEXITSTATUS(status) != 0) {
      perror("%s failed");
      return -1;
    }
  }
  return 0;
}

/**
 *  Build a vector<char> from a string. The string contains numbers in
 *  hexadecimal format. For example: "0A1286EF"
 *
 * @param str The string to convert.
 *
 * @return A vector<char>
 */
std::vector<char> misc::from_hex(std::string const& str) {
  std::vector<char> retval;
  size_t len{str.size()};
  if (len & 1)
    throw msg_fmt("from_hex: '{}' length should be even", str);
  retval.reserve(len >> 1);
  bool valid{false};
  uint8_t value;
  for (char c : str) {
    if (!valid)
      value = 0;
    else
      value <<= 4;
    if (c >= '0' && c <= '9')
      value |= c - '0';
    else if (c >= 'A' && c <= 'F')
      value |= c + 10 - 'A';
    else if (c >= 'a' && c <= 'f')
      value |= c + 10 - 'a';
    else
      throw msg_fmt(
          "from_hex: '{}' should be a string containing some "
          "hexadecimal digits",
          str);
    if (valid) {
      retval.push_back(value);
      valid = false;
    } else
      valid = true;
  }
  return retval;
}

/**
 *  Dump all the filters in a string.
 *
 *  @param[in] filters  The filters.
 *
 *  @return             A string containing all the filters.
 */
std::string misc::dump_filters(const multiplexing::muxer_filter& filters) {
  io::events::events_container all_event_container =
      io::events::instance().get_events_by_category_name("all");
  std::map<uint32_t, std::string> name_by_id;

  for (io::events::events_container::const_iterator
           it = all_event_container.begin(),
           end = all_event_container.end();
       it != end; ++it) {
    if (filters.allows(it->first))
      name_by_id[it->first] = it->second.get_name();
  }

  if (all_event_container.size() == name_by_id.size())
    return "all";

  std::string ret;
  for (auto it = name_by_id.cbegin(), end = name_by_id.cend(); it != end; ++it)
    if (ret.empty())
      ret.append(it->second);
    else
      ret.append(", ").append(it->second);
  return ret;
}

#if DEBUG_ROBOT
void misc::debug(const std::string& content) {
  int p = getpid();
  std::string filename{fmt::format("failed/{}.log", p)};
  FILE* f = fopen(filename.c_str(), "a");
  if (!f) {
    if (mkdir("failed", 0755) != 0)
      return;
    f = fopen(filename.c_str(), "a");
  }
  if (f) {
    fprintf(f, "%s\n", content.c_str());
    fclose(f);
  }
}
#endif
