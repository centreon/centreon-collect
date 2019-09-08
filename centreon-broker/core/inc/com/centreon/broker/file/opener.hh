/*
** Copyright 2011-2012,2017 Centreon
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

#ifndef CCB_FILE_OPENER_HH
#define CCB_FILE_OPENER_HH

#include <string>
#include "com/centreon/broker/io/endpoint.hh"
#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace file {
/**
 *  @class opener opener.hh "com/centreon/broker/file/opener.hh"
 *  @brief Open a file stream.
 *
 *  Open a file stream.
 */
class opener : public io::endpoint {
 public:
  opener();
  opener(opener const& other);
  ~opener();
  opener& operator=(opener const& other);
  std::shared_ptr<io::stream> open();
  void set_auto_delete(bool auto_delete);
  void set_filename(std::string const& filename);
  void set_max_size(unsigned long long max);

 private:
  bool _auto_delete;
  std::string _filename;
  unsigned long long _max_size;
};
}  // namespace file

CCB_END()

#endif  // !CCB_FILE_OPENER_HH
