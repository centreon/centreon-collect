/**
 * Copyright 2011-2012,2015,2017-2024 Centreon
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

#include "com/centreon/broker/io/stream.hh"

#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::io;
using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 * @brief Constructor. The name is chosen by the developer. This name is
 * constant.
 *
 * @param name a string representing the stream.
 */
stream::stream(const std::string& name) : _name(name) {}

/**
 *  Flush data.
 *
 *  @return Number of events acknowledged. This is 0 by default.
 */
int stream::flush() {
  return 0;
}

/**
 *  Get peer name.
 *
 *  @return Peer name.
 */
std::string stream::peer() const {
  return !_substream ? "(unknown)" : _substream->peer();
}

/**
 *  Set sub-stream.
 *
 *  @param[in,out] substream  Stream on which this stream will read and
 *                            write.
 */
void stream::set_substream(std::shared_ptr<stream> substream) {
  _substream = substream;
}

std::shared_ptr<stream> stream::get_substream() {
  return _substream;
}

/**
 *  Generate statistics about the stream.
 *
 *  @param[out] tree Output tree.
 */
void stream::statistics(nlohmann::json& tree) const {
  (void)tree;
}

/**
 *  Configuration update.
 */
void stream::update() {}

/**
 *  Validate an event.
 *
 *  @param[in] d      The event.
 *  @param[in] error  The prefix of the error message.
 *
 *  @return           True if event is valid.
 */
bool stream::validate(std::shared_ptr<io::data> const& d,
                      std::string const& error) {
  if (!d) {
    log_v2::instance()
        .get(log_v2::CORE)
        ->error(
            "{}: received a null event. This should never happen. "
            "This is likely a software bug that you should report "
            "to Centreon Broker developers.",
            error);
    return false;
  }
  return true;
}

/**
 * @brief if it has a substream, it waits until the substream has sent all data
 * on the wire
 *
 * @param ms_timeout
 * @return true all data sent
 * @return false timeout expires
 */
bool stream::wait_for_all_events_written(unsigned ms_timeout) {
  if (_substream) {
    return _substream->wait_for_all_events_written(ms_timeout);
  }
  return true;
}
