/**
 * Copyright 2013 Centreon
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

#ifndef CCB_RRD_CREATOR_HH
#define CCB_RRD_CREATOR_HH

#include <sys/types.h>

namespace com::centreon::broker {

namespace rrd {
/**
 *  @class creator creator.hh "com/centreon/broker/rrd/creator.hh"
 *  @brief RRD creator.
 *
 *  Create RRD objects.
 */
class creator {
  struct tmpl_info {
    bool operator<(tmpl_info const& right) const {
      if (length != right.length)
        return (length < right.length);
      if (step != right.step)
        return (step < right.step);
      if (value_type != right.value_type)
        return (value_type < right.value_type);
      // from must be the last compared value
      return from < right.from;
    }
    bool is_length_step_type_equal(const tmpl_info& right) const {
      return length == right.length && step == right.step &&
             value_type == right.value_type;
    }
    time_t from;
    uint32_t length;
    uint32_t step;
    short value_type;
  };

  struct fd_info {
    int fd;
    off_t size;
    std::string path;
  };

  void _duplicate(std::string const& filename, fd_info const& in_fd);
  void _open(std::string const& filename,
             uint32_t length,
             time_t from,
             uint32_t step,
             short value_type);
  void _read_write(int out_fd,
                   int in_fd,
                   ssize_t size,
                   std::string const& filename);
#ifdef __linux__
  void _sendfile(int out_fd,
                 int in_fd,
                 off_t already_transferred,
                 ssize_t size,
                 std::string const& filename);
#endif  // Linux

  uint32_t _cache_size;
  std::map<tmpl_info, fd_info> _fds;
  std::string _tmpl_path;

 public:
  creator(std::string const& tmpl_path, uint32_t cache_size);
  creator(creator const&) = delete;
  ~creator();
  creator& operator=(creator const&) = delete;
  void clear();
  void create(std::string const& filename,
              uint32_t length,
              time_t from,
              uint32_t step,
              short value_type,
              bool without_cache = false);
};
}  // namespace rrd

}  // namespace com::centreon::broker

#endif  // !CCB_RRD_CREATOR_HH
