/**
 * Copyright 2011-2013,2015,2017 Centreon
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

#include "com/centreon/broker/rrd/lib.hh"

#include <fcntl.h>
#include <rrd.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>

#include "bbdo/storage/metric.hh"
#include "com/centreon/broker/rrd/exceptions/open.hh"
#include "com/centreon/broker/rrd/exceptions/update.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::rrd;

/**
 *  Constructor.
 *
 *  @param[in] tmpl_path  The template path.
 *  @param[in] cache_size The maximum number of cache element.
 */
lib::lib(std::string const& tmpl_path, uint32_t cache_size)
    : _creator(tmpl_path, cache_size) {}

/**
 *  @brief Initiates the bulk load of multiple commands.
 *
 *  With the librrd backend, this method does nothing.
 */
void lib::begin() {}

/**
 *  Clean the template cache.
 */
void lib::clean() {
  _creator.clear();
}

/**
 *  Close the RRD file.
 */
void lib::close() {
  _filename.clear();
}

/**
 *  @brief Commit transaction started with begin().
 *
 *  With the librrd backend, the method does nothing.
 */
void lib::commit() {}

/**
 *  Open a RRD file which already exists.
 *
 *  @param[in] filename Path to the RRD file.
 */
void lib::open(std::string const& filename) {
  // Close previous file.
  this->close();

  // Check that the file exists.
  if (access(filename.c_str(), F_OK))
    throw exceptions::open("RRD: file '{}' does not exist", filename);

  // Remember information for further operations.
  _filename = filename;
}

/**
 *  Open a RRD file and create it if it does not exists.
 *
 *  @param[in] filename   Path to the RRD file.
 *  @param[in] length     Duration in seconds that the RRD file should
 *                        retain.
 *  @param[in] from       Timestamp of the first record.
 *  @param[in] step       Time interval between each record.
 *  @param[in] value_type Type of the metric.
 */
void lib::open(std::string const& filename,
               uint32_t length,
               time_t from,
               uint32_t step,
               short value_type,
               bool without_cache) {
  // Close previous file.
  this->close();

  // Remember informations for further operations.
  _filename = filename;
  _creator.create(filename, length, from, step, value_type, without_cache);
}

/**
 *  Remove the RRD file.
 *
 *  @param[in] filename Path to the RRD file.
 */
void lib::remove(std::string const& filename) {
  if (::remove(filename.c_str())) {
    char const* msg(strerror(errno));
    _logger->error("RRD: could not remove file '{}': {}", filename, msg);
  } else
    SPDLOG_LOGGER_INFO(_logger, "remove file {}", filename);
}

/**
 *  Update the RRD file with new value.
 *
 *  @param[in] t     Timestamp of value.
 *  @param[in] value Associated value.
 */
void lib::update(time_t t, std::string const& value) {
  // Build argument string.
  if (value == "") {
    _logger->error("RRD: ignored update non-float value '{}' in file '{}'",
                   value, _filename);
    return;
  }

  std::string arg(fmt::format("{}:{}", t, value));

  // Set argument table.
  char const* argv[2];
  argv[0] = arg.c_str();
  argv[1] = nullptr;

  // Debug message.
  _logger->debug("RRD: updating file '{}' ({})", _filename, argv[0]);

  // Update RRD file.
  rrd_clear_error();
  if (rrd_update_r(_filename.c_str(), nullptr, sizeof(argv) / sizeof(*argv) - 1,
                   argv)) {
    char const* msg(rrd_get_error());
    if (!strstr(msg, "illegal attempt to update using time"))
      _logger->error("RRD: failed to update value in file '{}': {}", _filename,
                     msg);

    else
      _logger->error("RRD: ignored update error in file '{}': {}", _filename,
                     msg);
  }
}

void lib::update(const std::deque<std::string>& pts) {
  const char* argv[pts.size() + 1];
  argv[pts.size()] = nullptr;
  auto it = pts.begin();
  for (uint32_t i = 0; i < pts.size(); i++) {
    _logger->trace("insertion of {} in rrd file", *it);
    argv[i] = it->data();
    ++it;
  }
  rrd_clear_error();
  if (rrd_update_r(_filename.c_str(), nullptr, sizeof(argv) / sizeof(*argv) - 1,
                   argv)) {
    char const* msg(rrd_get_error());
    if (!strstr(msg, "illegal attempt to update using time"))
      _logger->error("RRD: failed to update value in file '{}': {}", _filename,
                     msg);

    else
      _logger->error("RRD: ignored update error in file '{}': {}", _filename,
                     msg);
  }
}
