/*
** Copyright 2011-2013 Centreon
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

#ifndef CCB_RRD_LIB_HH
#define CCB_RRD_LIB_HH

#include "com/centreon/broker/rrd/backend.hh"
#include "com/centreon/broker/rrd/creator.hh"

namespace com::centreon::broker {

namespace rrd {
/**
 *  @class lib lib.hh "com/centreon/broker/rrd/lib.hh"
 *  @brief Handle RRD file access through librrd.
 *
 *  Handle creation, deletion, tuning and update of an RRD file with
 *  librrd.
 */
class lib : public backend {
 public:
  lib(std::string const& tmpl_path, uint32_t cache_size);
  lib(lib const& l) = delete;
  ~lib() = default;
  lib& operator=(lib const& l) = delete;
  void begin() override;
  void clean() override;
  void close() override;
  void commit() override;
  void open(std::string const& filename) override;
  void open(std::string const& filename,
            uint32_t length,
            time_t from,
            uint32_t step,
            short value_type = 0,
            bool without_cache = false) override;
  void remove(std::string const& filename) override;
  void update(time_t t, std::string const& value) override;
  void update(const std::deque<std::string>& pts) override;

 private:
  creator _creator;
  std::string _filename;
};
}  // namespace rrd

}

#endif  // !CCB_RRD_LIB_HH
