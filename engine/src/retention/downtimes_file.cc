/**
 * Copyright 2026 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include "com/centreon/engine/retention/downtimes_file.hh"
#include "bbdo/neb.pb.h"
#include "com/centreon/engine/globals.hh"
#include "common/downtimes/downtime_manager.hh"
#include "common/proto_file/proto_file.hh"

namespace downtimes = com::centreon::common::downtimes;
using com::centreon::broker::Downtime;
using com::centreon::broker::EngineDowntimes;

namespace com::centreon::engine::retention::downtimes_file {

namespace {
/**
 * @brief Tell whether a downtime is one of the downtimes BAM puts on the
 * virtual services of its BAs. Broker already sends them again, so they are
 * not saved, as in retention.dat.
 *
 * @param dt The downtime.
 *
 * @return true if the downtime comes from BAM.
 */
bool is_bam_downtime(const downtimes::downtime& dt) {
  if (dt.service_id() == 0)
    return false;
  auto names = downtimes::downtime_manager::instance()
                   .callbacks()
                   .get_host_and_service_names(dt.host_id(), dt.service_id());
  return names.first.compare(0, 12, "_Module_BAM_") == 0 &&
         names.second.compare(0, 3, "ba_") == 0;
}
}  // namespace

/**
 * @brief Save the downtimes Engine owns in @a path.
 *
 * The file is a serialized EngineDowntimes message. It is first written next
 * to @a path and then renamed, so that a crash while writing never leaves a
 * truncated file. In notification_mode=broker, Engine owns no downtime and the
 * file is written empty.
 *
 * @param path The downtimes file, <proto_conf>/downtimes.prot.
 *
 * @return true on success, false otherwise.
 */
bool save(const std::filesystem::path& path) {
  EngineDowntimes msg;
  for (const auto& [_, dt] :
       downtimes::downtime_manager::instance().get_scheduled_downtimes()) {
    if (is_bam_downtime(*dt))
      continue;
    Downtime* d = msg.add_downtimes();
    d->set_id(dt->get_downtime_id());
    d->set_host_id(dt->host_id());
    d->set_service_id(dt->service_id());
    d->set_type(dt->service_id() == 0 ? Downtime::HOST : Downtime::SERVICE);
    d->set_author(dt->get_author());
    d->set_comment_data(dt->get_comment());
    d->set_entry_time(dt->get_entry_time());
    d->set_start_time(dt->get_start_time());
    d->set_end_time(dt->get_end_time());
    d->set_fixed(dt->is_fixed());
    d->set_triggered_by(dt->get_triggered_by());
    d->set_duration(dt->get_duration());
  }

  std::error_code ec;
  com::centreon::common::write_proto_file(path, msg, false, ec);
  if (ec) {
    runtime_logger->error("Cannot write the downtimes file '{}': {}",
                          path.string(), ec.message());
    return false;
  }
  runtime_logger->debug("{} downtime(s) saved in '{}'", msg.downtimes_size(),
                        path.string());
  return true;
}

/**
 * @brief Recreate the downtimes saved in @a path.
 *
 * Called at startup, once the configuration is applied, so that the hosts and
 * services exist, and before the downtime data is initialized, as the
 * retention.dat downtimes are. A downtime whose resource no longer exists is
 * skipped, and so is one already known: a retention.dat read at the switch
 * from the legacy configuration may have brought it already. The depth of the
 * resources is not saved: it is rebuilt when the recreated downtimes start.
 *
 * @param path The downtimes file, <proto_conf>/downtimes.prot.
 *
 * @return The number of downtimes recreated.
 */
size_t restore(const std::filesystem::path& path) {
  EngineDowntimes msg;
  std::error_code ec;
  com::centreon::common::read_proto_file(path, msg, ec);
  if (ec == std::errc::no_such_file_or_directory)
    return 0;
  if (ec) {
    config_logger->error("Cannot read the downtimes file '{}': {}",
                         path.string(), ec.message());
    return 0;
  }

  auto& manager = downtimes::downtime_manager::instance();
  size_t restored = 0;
  for (const Downtime& d : msg.downtimes()) {
    if (manager.find_downtime(downtimes::downtime::any_downtime, d.id())) {
      downtimes_logger->debug("Downtime {} already known, not restored again",
                              d.id());
      continue;
    }
    if (!manager.callbacks().resource_exists(d.host_id(), d.service_id())) {
      downtimes_logger->warn(
          "Downtime {} not restored: the resource ({}, {}) no longer exists",
          d.id(), d.host_id(), d.service_id());
      continue;
    }
    auto dt = std::make_shared<downtimes::downtime>(
        d.host_id(), d.service_id(), d.entry_time(), d.author(),
        d.comment_data(), d.start_time(), d.end_time(), d.fixed(),
        d.triggered_by(), d.duration(), d.id(), downtimes_logger);
    manager.add_downtime(dt);
    dt->notify_broker_load();
    dt->subscribe();
    ++restored;
  }
  config_logger->info("Startup: {} downtime(s) restored from '{}'", restored,
                      path.string());
  return restored;
}

}  // namespace com::centreon::engine::retention::downtimes_file
