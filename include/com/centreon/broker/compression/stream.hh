/*
** Copyright 2011-2013,2015,2017 Centreon
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

#ifndef CCB_COMPRESSION_STREAM_HH
#define CCB_COMPRESSION_STREAM_HH

#include <vector>

#include "com/centreon/broker/compression/stack_array.hh"
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace compression {
/**
 *  @class stream stream.hh "com/centreon/broker/compression/stream.hh"
 *  @brief Compression stream.
 *
 *  Compress and uncompress data.
 */
class stream : public io::stream {
  const int32_t _level;
  const size_t _size;
  stack_array _rbuffer;
  bool _shutdown;
  std::vector<char> _wbuffer;

  void _flush();
  void _get_data(int size, time_t timeout);

 public:
  static int const max_data_size;

  stream(int32_t level = -1, size_t size = 0);
  ~stream();
  stream(stream const&) = delete;
  stream& operator=(stream const&) = delete;
  int flush();
  bool read(std::shared_ptr<io::data>& d, time_t deadline = (time_t)-1);
  void statistics(json11::Json::object& tree) const;
  int write(std::shared_ptr<io::data> const& d);
};
}  // namespace compression

CCB_END()

#endif  // !CCB_COMPRESSION_STREAM_HH
