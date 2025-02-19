/**
 * Copyright 2015,2017, 2020-2021 Centreon
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

#include "com/centreon/broker/persistent_file.hh"

#include "broker/core/bbdo/stream.hh"
#include "com/centreon/broker/compression/stream.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 *  Constructor.
 *
 *  @param[in] path  Path of the persistent file.
 *  @param[in] muxer_name The muxer this file is associated to. This name is
 * used to access the statistics in the stats center.
 */
persistent_file::persistent_file(const std::string& path, QueueFileStats* stats)
    : io::stream("persistent_file") {
  // On-disk file.
  constexpr uint32_t max_size{100000000u};
  _splitter = std::make_shared<file::stream>(path, stats, max_size, true);

  // Compression layer.
  auto cs{std::make_shared<compression::stream>()};
  cs->set_substream(_splitter);

  // BBDO layer.
  auto bs{std::make_shared<bbdo::stream>(true)};
  bs->set_coarse(true);
  bs->set_negotiate(false);
  bs->set_substream(cs);

  // Set stream.
  io::stream::set_substream(bs);
  if (stats)
    config::applier::state::instance().center()->execute(
        [path, stats, max_file_size = _splitter->max_file_size()] {
          stats->set_name(path);
          stats->set_max_file_size(max_file_size);
        });
}

/**
 *  Read data from file.
 *
 *  @param[out] d         Output data.
 *  @param[in]  deadline  Timeout.
 *
 *  @return Always return true, as file never times out.
 */
bool persistent_file::read(std::shared_ptr<io::data>& d, time_t deadline) {
  return _substream->read(d, deadline);
}

/**
 *  Generate statistics of persistent file.
 *
 *  @param[out] tree  Statistics tree.
 */
void persistent_file::statistics(nlohmann::json& tree) const {
  _substream->statistics(tree);
}

/**
 *  Write data to file.
 *
 *  @param[in] d  Input data.
 */
int32_t persistent_file::write(std::shared_ptr<io::data> const& d) {
  return _substream->write(d);
}

/**
 * @brief Flush the stream and stop it.
 *
 * @return the number of acknowledged events.
 */
int32_t persistent_file::stop() {
  int32_t retval = _substream->stop();
  log_v2::instance()
      .get(log_v2::CORE)
      ->info("persistent file stopped with {} acknowledged events", retval);
  return retval;
}

/**
 *  Remove persistent file.
 */
void persistent_file::remove_all_files() {
  _splitter->remove_all_files();
}
