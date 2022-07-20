/*
** Copyright 2011-2012, 2021-2022 Centreon
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

#include "com/centreon/broker/compression/opener.hh"
#include "com/centreon/broker/compression/stream.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::compression;

/**
 * @brief Constructor
 *
 * @param size Size of the compression buffer. Default value is 0.
 * @param level Level of the compression function (in the range [-1, 9]). -1 is
 * the default compression.
 */
opener::opener(int32_t level, size_t size)
    : io::endpoint(false), _level(level), _size(size) {}

/**
 *  Open a compression stream.
 *
 *  @return New compression object.
 */
std::unique_ptr<io::stream> opener::open() {
  std::unique_ptr<io::stream> retval;
  if (_from)
    retval = _open(_from->open());
  return retval;
}

/**
 *  Open a compression stream.
 *
 *  @return New compression object.
 */
std::unique_ptr<io::stream> opener::_open(std::shared_ptr<io::stream> base) {
  std::unique_ptr<io::stream> retval;
  if (base) {
    retval = std::make_unique<stream>(_level, _size);
    retval->set_substream(base);
  }
  return retval;
}
