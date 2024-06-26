/**
 * Copyright 2011-2012,2017, 2021 Centreon
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

#include "com/centreon/broker/file/opener.hh"

#include "com/centreon/broker/file/splitter.hh"
#include "com/centreon/broker/file/stream.hh"
#include "com/centreon/broker/multiplexing/muxer_filter.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::file;

/**
 *  Constructor.
 */
opener::opener()
    : io::endpoint(
          false,
          {},
          multiplexing::muxer_filter(multiplexing::muxer_filter::zero_init())),
      _auto_delete(true),
      _max_size(100000000) {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
opener::opener(const opener& other)
    : io::endpoint(other),
      _auto_delete(other._auto_delete),
      _filename(other._filename),
      _max_size(other._max_size) {}

/**
 *  Destructor.
 */
opener::~opener() {}

/**
 *  Open a new stream.
 *
 *  @return Opened stream.
 */
std::shared_ptr<io::stream> opener::open() {
  // Open splitted file.
  return std::make_shared<stream>(_filename, nullptr, _max_size, _auto_delete);
}

/**
 *  Enable or disable auto-delete mode.
 *
 *  @param[in] auto_delete  True to automatically delete file parts.
 */
void opener::set_auto_delete(bool auto_delete) {
  _auto_delete = auto_delete;
}

/**
 *  Set the filename.
 *
 *  @param[in] filename Filename.
 */
void opener::set_filename(std::string const& filename) {
  _filename = filename;
}

/**
 *  Set the maximum allowed file size.
 *
 *  @param[in] max Maximum file size in bytes.
 */
void opener::set_max_size(unsigned long long max) {
  _max_size = max;
}
