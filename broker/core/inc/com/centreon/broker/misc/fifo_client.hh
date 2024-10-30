/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
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
#ifndef _CCB_MISC_FIFO_CLIENT_HH
#define _CCB_MISC_FIFO_CLIENT_HH

namespace com::centreon::broker {

namespace misc {
/**
 * @class fifo_client fifo_client.hh "com/centreon/broker/misc/fifo_client.hh"
 * @brief fifo client, its goal is to connect to an existing fifo from its
 * name and write into it. The connection is non blocking.
 */
class fifo_client {
  const std::string _filename;
  enum class step { OPEN, WRITE };
  step _step = step::OPEN;
  int _fd;

 public:
  fifo_client(std::string filename) : _filename{std::move(filename)} {}
  ~fifo_client() noexcept { close(); }
  void close() {
    ::close(_fd);
    _step = step::OPEN;
  }

  /**
   * @brief Write into the fifo the content of buffer. If it is not open, try to
   * open it at first. We have a step machine with two steps:
   * * OPEN
   * * WRITE
   * If open successes, the step goes to WRITE. And if the write fails, the step
   * goes back to OPEN.
   *
   * @param buffer The string to write.
   *
   * @return 0 on success, -1 if open fails, -2 if write fails.
   */
  int write(const std::string& buffer) {
    int retval = 0;
    switch (_step) {
      case step::OPEN:
        _fd = open(_filename.c_str(), O_WRONLY | O_NONBLOCK);
        if (_fd < 0) {
          fprintf(stderr, "%s\n", strerror(errno));

          retval = -1;
          break;
        } else {
          // No break here, we continue with write
          _step = step::WRITE;
        }
        [[fallthrough]];
      case step::WRITE:
        if (::write(_fd, buffer.c_str(), buffer.size()) !=
            static_cast<ssize_t>(buffer.size())) {
          fprintf(stderr, "%s\n", strerror(errno));

          // We go back to step::OPEN
          _step = step::OPEN;
          retval = -2;
        } else
          retval = 0;
    }
    return retval;
  }
};
}  // namespace misc

}  // namespace com::centreon::broker

#endif /* !_CCB_MISC_FIFO_CLIENT */
