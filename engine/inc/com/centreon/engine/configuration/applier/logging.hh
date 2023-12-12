/**
 * Copyright 2011-2014 Merethis
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef CCE_CONFIGURATION_APPLIER_LOGGING_HH
#define CCE_CONFIGURATION_APPLIER_LOGGING_HH

#include "com/centreon/engine/configuration/state.hh"
#include "com/centreon/logging/file.hh"
#include "com/centreon/logging/syslogger.hh"
#include "common/configuration/state.pb.h"

namespace com::centreon::engine {

namespace configuration {
namespace applier {
/**
 *  @class logging logging.hh
 *  @brief Simple configuration applier for logging class.
 *
 *  Simple configuration applier for logging class.
 */
class logging {
  com::centreon::logging::file* _debug;
  uint64_t _debug_level;
  unsigned long _debug_max_size;
  unsigned int _debug_verbosity;
  com::centreon::logging::file* _log;
  com::centreon::logging::file* _stderr;
  com::centreon::logging::file* _stdout;
  com::centreon::logging::syslogger* _syslog;

  logging();
  logging(configuration::state& config);
  logging(logging const&);
  ~logging() noexcept;
  logging& operator=(logging const&);

  void _add_stdout();
  void _add_stderr();
  void _add_syslog();
#ifdef LEGACY_CONF
  void _add_log_file(configuration::state const& config);
  void _add_debug(configuration::state const& config);
#else
  void _add_log_file(const configuration::State& config);
  void _add_debug(const configuration::State& config);
#endif
  void _del_syslog();
  void _del_log_file();
  void _del_debug();
  void _del_stdout();
  void _del_stderr();

 public:
#ifdef LEGACY_CONF
  void apply(configuration::state& config);
#else
  void apply(configuration::State& config);
#endif
  static logging& instance();
  void clear();
};
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_LOGGING_HH
