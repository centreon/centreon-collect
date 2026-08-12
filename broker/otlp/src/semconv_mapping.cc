/**
 * Copyright 2026 Centreon
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

#include "com/centreon/broker/otlp/semconv_mapping.hh"

using namespace com::centreon::broker::otlp;
using com::centreon::common::perfdata;

namespace {

/* Scale factors from a Centreon unit to the UCUM unit the convention wants. */
constexpr double k_percent_to_ratio = 0.01;
constexpr double k_milli_to_unit = 0.001;
constexpr double k_bits_to_bytes = 0.125;

struct table_row {
  const char* otel_name;
  const char* otel_unit;
  instrument instr;
  double scale;
  const char* attr_key;    // nullptr when the row needs no static attribute
  const char* attr_value;
  instance_attribute inst_attr;
};

/**
 * @brief Centreon perfdata label -> OpenTelemetry semantic convention.
 *
 * Keyed on the metric part of a structured label (everything after '#'), so a
 * single row covers every mountpoint or interface.
 *
 * Names, units and instrument kinds follow
 * https://opentelemetry.io/docs/specs/semconv/system/system-metrics/ — note
 * that the CPU state attribute is `cpu.mode` and dropped packets are
 * `system.network.packet.dropped`; both were renamed and older material has
 * the previous spellings.
 *
 * Rows naming a `centreon.*` metric are deliberate: they are cases where no
 * convention exists, and inventing a `system.*` name whose meaning differs
 * from the Centreon metric would produce dashboards that look correct and are
 * not. `interface.traffic.*` is the clearest instance — Centreon reports a
 * bits/second rate while `system.network.io` is a cumulative byte counter, so
 * mapping one onto the other would make every rate() query wrong.
 */
const absl::flat_hash_map<std::string_view, table_row>& mapping_table() {
  static const auto* table = new absl::flat_hash_map<std::string_view,
                                                     table_row>{
      // ---- CPU ------------------------------------------------------------
      {"cpu.utilization.percentage",
       {"system.cpu.utilization", "1", instrument::gauge, k_percent_to_ratio,
        nullptr, nullptr, instance_attribute::none}},
      {"cpu.user.percentage",
       {"system.cpu.utilization", "1", instrument::gauge, k_percent_to_ratio,
        "cpu.mode", "user", instance_attribute::none}},
      {"cpu.system.percentage",
       {"system.cpu.utilization", "1", instrument::gauge, k_percent_to_ratio,
        "cpu.mode", "system", instance_attribute::none}},
      {"cpu.idle.percentage",
       {"system.cpu.utilization", "1", instrument::gauge, k_percent_to_ratio,
        "cpu.mode", "idle", instance_attribute::none}},
      {"cpu.iowait.percentage",
       {"system.cpu.utilization", "1", instrument::gauge, k_percent_to_ratio,
        "cpu.mode", "iowait", instance_attribute::none}},
      {"cpu.steal.percentage",
       {"system.cpu.utilization", "1", instrument::gauge, k_percent_to_ratio,
        "cpu.mode", "steal", instance_attribute::none}},
      {"cpu.nice.percentage",
       {"system.cpu.utilization", "1", instrument::gauge, k_percent_to_ratio,
        "cpu.mode", "nice", instance_attribute::none}},

      // ---- Memory ---------------------------------------------------------
      {"memory.usage.bytes",
       {"system.memory.usage", "By", instrument::sum_non_monotonic, 1.0,
        "system.memory.state", "used", instance_attribute::none}},
      {"memory.free.bytes",
       {"system.memory.usage", "By", instrument::sum_non_monotonic, 1.0,
        "system.memory.state", "free", instance_attribute::none}},
      {"memory.cached.bytes",
       {"system.memory.usage", "By", instrument::sum_non_monotonic, 1.0,
        "system.memory.state", "cached", instance_attribute::none}},
      {"memory.buffer.bytes",
       {"system.memory.usage", "By", instrument::sum_non_monotonic, 1.0,
        "system.memory.state", "buffered", instance_attribute::none}},
      {"memory.usage.percentage",
       {"system.memory.utilization", "1", instrument::gauge,
        k_percent_to_ratio, "system.memory.state", "used",
        instance_attribute::none}},
      {"memory.total.bytes",
       {"system.memory.limit", "By", instrument::sum_non_monotonic, 1.0,
        nullptr, nullptr, instance_attribute::none}},

      // ---- Swap / paging --------------------------------------------------
      {"swap.usage.bytes",
       {"system.paging.usage", "By", instrument::sum_non_monotonic, 1.0,
        "system.paging.state", "used", instance_attribute::none}},
      {"swap.free.bytes",
       {"system.paging.usage", "By", instrument::sum_non_monotonic, 1.0,
        "system.paging.state", "free", instance_attribute::none}},
      {"swap.usage.percentage",
       {"system.paging.utilization", "1", instrument::gauge,
        k_percent_to_ratio, "system.paging.state", "used",
        instance_attribute::none}},

      // ---- Filesystem -----------------------------------------------------
      {"disk.space.usage.bytes",
       {"system.filesystem.usage", "By", instrument::sum_non_monotonic, 1.0,
        "system.filesystem.state", "used",
        instance_attribute::filesystem_mountpoint}},
      {"disk.space.free.bytes",
       {"system.filesystem.usage", "By", instrument::sum_non_monotonic, 1.0,
        "system.filesystem.state", "free",
        instance_attribute::filesystem_mountpoint}},
      {"disk.space.usage.percentage",
       {"system.filesystem.utilization", "1", instrument::gauge,
        k_percent_to_ratio, nullptr, nullptr,
        instance_attribute::filesystem_mountpoint}},
      {"disk.space.total.bytes",
       {"system.filesystem.limit", "By", instrument::sum_non_monotonic, 1.0,
        nullptr, nullptr, instance_attribute::filesystem_mountpoint}},
      {"disk.inodes.usage.percentage",
       {"centreon.filesystem.inode.utilization", "1", instrument::gauge,
        k_percent_to_ratio, nullptr, nullptr,
        instance_attribute::filesystem_mountpoint}},

      // ---- Disk I/O -------------------------------------------------------
      {"disk.io.read.bytes",
       {"system.disk.io", "By", instrument::sum_monotonic, 1.0,
        "disk.io.direction", "read", instance_attribute::system_device}},
      {"disk.io.write.bytes",
       {"system.disk.io", "By", instrument::sum_monotonic, 1.0,
        "disk.io.direction", "write", instance_attribute::system_device}},
      /* Centreon reports a rate here, the convention counts bytes: keep the
       * Centreon namespace rather than corrupt system.disk.io. */
      {"disk.io.read.bytespersecond",
       {"centreon.disk.io.rate", "By/s", instrument::gauge, 1.0,
        "disk.io.direction", "read", instance_attribute::system_device}},
      {"disk.io.write.bytespersecond",
       {"centreon.disk.io.rate", "By/s", instrument::gauge, 1.0,
        "disk.io.direction", "write", instance_attribute::system_device}},

      // ---- Network --------------------------------------------------------
      /* bits/second rate, not a cumulative byte counter: see the class note. */
      {"interface.traffic.in.bitspersecond",
       {"centreon.network.throughput", "By/s", instrument::gauge,
        k_bits_to_bytes, "network.io.direction", "receive",
        instance_attribute::network_interface}},
      {"interface.traffic.out.bitspersecond",
       {"centreon.network.throughput", "By/s", instrument::gauge,
        k_bits_to_bytes, "network.io.direction", "transmit",
        instance_attribute::network_interface}},
      {"interface.packets.in.count",
       {"system.network.packet.count", "{packet}", instrument::sum_monotonic,
        1.0, "network.io.direction", "receive",
        instance_attribute::network_interface}},
      {"interface.packets.out.count",
       {"system.network.packet.count", "{packet}", instrument::sum_monotonic,
        1.0, "network.io.direction", "transmit",
        instance_attribute::network_interface}},
      {"interface.packets.error.in.count",
       {"system.network.errors", "{error}", instrument::sum_monotonic, 1.0,
        "network.io.direction", "receive",
        instance_attribute::network_interface}},
      {"interface.packets.error.out.count",
       {"system.network.errors", "{error}", instrument::sum_monotonic, 1.0,
        "network.io.direction", "transmit",
        instance_attribute::network_interface}},
      {"interface.packets.discard.in.count",
       {"system.network.packet.dropped", "{packet}",
        instrument::sum_monotonic, 1.0, "network.io.direction", "receive",
        instance_attribute::network_interface}},
      {"interface.packets.discard.out.count",
       {"system.network.packet.dropped", "{packet}",
        instrument::sum_monotonic, 1.0, "network.io.direction", "transmit",
        instance_attribute::network_interface}},

      // ---- Load -----------------------------------------------------------
      {"load.1m.count",
       {"system.linux.cpu.load_1m", "{run_queue_item}", instrument::gauge, 1.0,
        nullptr, nullptr, instance_attribute::none}},
      {"load.5m.count",
       {"system.linux.cpu.load_5m", "{run_queue_item}", instrument::gauge, 1.0,
        nullptr, nullptr, instance_attribute::none}},
      {"load.15m.count",
       {"system.linux.cpu.load_15m", "{run_queue_item}", instrument::gauge,
        1.0, nullptr, nullptr, instance_attribute::none}},
      {"load1",
       {"system.linux.cpu.load_1m", "{run_queue_item}", instrument::gauge, 1.0,
        nullptr, nullptr, instance_attribute::none}},
      {"load5",
       {"system.linux.cpu.load_5m", "{run_queue_item}", instrument::gauge, 1.0,
        nullptr, nullptr, instance_attribute::none}},
      {"load15",
       {"system.linux.cpu.load_15m", "{run_queue_item}", instrument::gauge,
        1.0, nullptr, nullptr, instance_attribute::none}},

      // ---- Processes / uptime ---------------------------------------------
      {"processes.count",
       {"system.process.count", "{process}", instrument::sum_non_monotonic,
        1.0, nullptr, nullptr, instance_attribute::none}},
      {"nbproc",
       {"system.process.count", "{process}", instrument::sum_non_monotonic,
        1.0, nullptr, nullptr, instance_attribute::none}},
      {"system.uptime.seconds",
       {"system.uptime", "s", instrument::gauge, 1.0, nullptr, nullptr,
        instance_attribute::none}},
      {"uptime",
       {"system.uptime", "s", instrument::gauge, 1.0, nullptr, nullptr,
        instance_attribute::none}},

      // ---- ICMP / latency (no semantic convention exists) ------------------
      {"rta",
       {"centreon.icmp.rtt", "s", instrument::gauge, k_milli_to_unit, nullptr,
        nullptr, instance_attribute::none}},
      {"rtmin",
       {"centreon.icmp.rtt.min", "s", instrument::gauge, k_milli_to_unit,
        nullptr, nullptr, instance_attribute::none}},
      {"rtmax",
       {"centreon.icmp.rtt.max", "s", instrument::gauge, k_milli_to_unit,
        nullptr, nullptr, instance_attribute::none}},
      {"pl",
       {"centreon.icmp.packet_loss", "1", instrument::gauge,
        k_percent_to_ratio, nullptr, nullptr, instance_attribute::none}},
      {"time",
       {"centreon.check.duration", "s", instrument::gauge, 1.0, nullptr,
        nullptr, instance_attribute::none}},
  };
  return *table;
}

const char* instance_attribute_key(instance_attribute a) {
  switch (a) {
    case instance_attribute::filesystem_mountpoint:
      return "system.filesystem.mountpoint";
    case instance_attribute::network_interface:
      return "network.interface.name";
    case instance_attribute::system_device:
      return "system.device";
    case instance_attribute::cpu_logical_number:
      return "cpu.logical_number";
    default:
      return nullptr;
  }
}

/**
 * @brief UCUM unit for a Centreon unit of measure, for fallback metrics.
 *
 * Fallback keeps the value unscaled, so the unit reported must be the one the
 * value is actually in.
 */
std::string ucum_unit(std::string_view centreon_unit) {
  if (centreon_unit.empty())
    return "1";
  if (centreon_unit == "%")
    return "%";
  if (centreon_unit == "B" || centreon_unit == "b")
    return "By";
  if (centreon_unit == "s")
    return "s";
  if (centreon_unit == "ms")
    return "ms";
  if (centreon_unit == "c")
    return "1";
  return std::string(centreon_unit);
}

}  // namespace

namespace com::centreon::broker::otlp {

decomposed_name decompose(std::string_view perfdata_name) {
  decomposed_name res;
  const std::size_t sharp = perfdata_name.find('#');
  if (sharp == std::string_view::npos) {
    res.metric = perfdata_name;
    return res;
  }

  res.metric = perfdata_name.substr(sharp + 1);
  const std::string_view full_instance = perfdata_name.substr(0, sharp);
  const std::size_t tilde = full_instance.find('~');
  if (tilde == std::string_view::npos) {
    res.instance = full_instance;
    return res;
  }

  res.instance = full_instance.substr(0, tilde);
  for (std::string_view sub :
       absl::StrSplit(full_instance.substr(tilde + 1), '~'))
    res.subinstances.push_back(sub);
  return res;
}

std::string sanitize(std::string_view raw) {
  std::string out;
  out.reserve(raw.size());
  bool last_underscore = false;
  for (char c : raw) {
    if (absl::ascii_isalnum(static_cast<unsigned char>(c))) {
      out.push_back(absl::ascii_tolower(static_cast<unsigned char>(c)));
      last_underscore = false;
    } else if (c == '.') {
      /* Dots already separate namespaces in the Centreon convention; keep
       * them so disk.space.usage stays hierarchical. */
      if (!out.empty() && out.back() != '.') {
        out.push_back('.');
        last_underscore = false;
      }
    } else if (!last_underscore && !out.empty()) {
      out.push_back('_');
      last_underscore = true;
    }
  }
  while (!out.empty() && (out.back() == '_' || out.back() == '.'))
    out.pop_back();
  return out.empty() ? std::string("unnamed") : out;
}

mapping map_metric(std::string_view perfdata_name,
                   std::string_view unit,
                   perfdata::data_type value_type) {
  const decomposed_name parts = decompose(perfdata_name);

  const auto& table = mapping_table();
  const auto found = table.find(parts.metric);
  if (found != table.end()) {
    const table_row& row = found->second;
    const char* inst_key = instance_attribute_key(row.inst_attr);

    /* A convention that identifies its series by mountpoint/interface is
     * meaningless without it: every filesystem would collapse into one
     * series. Degrade to the Centreon namespace instead. */
    if (inst_key == nullptr || !parts.instance.empty()) {
      mapping m;
      m.name = row.otel_name;
      m.unit = row.otel_unit;
      m.instr = row.instr;
      m.scale = row.scale;
      if (row.attr_key)
        m.attributes.emplace_back(row.attr_key, row.attr_value);
      if (inst_key)
        m.attributes.emplace_back(inst_key, std::string(parts.instance));
      m.description = metric_description(m.name);
      return m;
    }
  }

  mapping m;
  m.name = absl::StrCat("centreon.", sanitize(parts.metric));
  m.unit = ucum_unit(unit);
  m.instr = (value_type == perfdata::counter || value_type == perfdata::derive)
                ? instrument::sum_monotonic
                : instrument::gauge;
  m.scale = 1.0;
  m.is_fallback = true;
  m.description =
      "Centreon perfdata metric with no OpenTelemetry semantic convention "
      "equivalent";
  /* The instance is still identifying information even without a convention
   * to name it. */
  if (!parts.instance.empty())
    m.attributes.emplace_back("centreon.metric.instance",
                              std::string(parts.instance));
  return m;
}

std::string_view metric_description(std::string_view emitted_name) {
  /* Keyed by emitted name, so the 48 mapping rows collapse to the 26 metrics
   * they produce. Plain factual one-liners: these are not verbatim copies of
   * the semantic convention texts. */
  static const auto* table =
      new absl::flat_hash_map<std::string_view, std::string_view>{
          // ---- semantic conventions -----------------------------------------
          {"system.cpu.utilization",
           "Fraction of CPU time spent, per mode when the mode is known"},
          {"system.memory.usage", "Memory used, by state"},
          {"system.memory.utilization", "Fraction of memory used"},
          {"system.memory.limit", "Total physical memory"},
          {"system.paging.usage", "Swap used, by state"},
          {"system.paging.utilization", "Fraction of swap used"},
          {"system.filesystem.usage", "Filesystem space used, by state"},
          {"system.filesystem.utilization",
           "Fraction of filesystem space used"},
          {"system.filesystem.limit", "Total filesystem space"},
          {"system.disk.io", "Bytes transferred to and from a disk"},
          {"system.network.errors", "Network errors, by direction"},
          {"system.network.packet.count", "Network packets, by direction"},
          {"system.network.packet.dropped",
           "Network packets dropped, by direction"},
          {"system.process.count", "Number of processes"},
          {"system.uptime", "Time since the host booted"},
          {"system.linux.cpu.load_1m", "One minute load average"},
          {"system.linux.cpu.load_5m", "Five minute load average"},
          {"system.linux.cpu.load_15m", "Fifteen minute load average"},
          // ---- Centreon namespace -------------------------------------------
          {"centreon.filesystem.inode.utilization",
           "Fraction of filesystem inodes used"},
          {"centreon.disk.io.rate",
           "Disk throughput as reported by the check, in bytes per second"},
          {"centreon.network.throughput",
           "Network throughput as reported by the check, in bytes per second"},
          {"centreon.icmp.rtt", "ICMP round trip time"},
          {"centreon.icmp.rtt.min", "Minimum ICMP round trip time"},
          {"centreon.icmp.rtt.max", "Maximum ICMP round trip time"},
          {"centreon.icmp.packet_loss", "Fraction of ICMP packets lost"},
          {"centreon.check.duration", "Time the check took to run"},
      };
  auto found = table->find(emitted_name);
  return found == table->end() ? std::string_view{} : found->second;
}

std::string threshold_metric_name(std::string_view emitted_name) {
  if (absl::StartsWith(emitted_name, "centreon."))
    return absl::StrCat(emitted_name, ".threshold");
  return absl::StrCat("centreon.", emitted_name, ".threshold");
}

std::string bound_metric_name(std::string_view emitted_name) {
  if (absl::StartsWith(emitted_name, "centreon."))
    return absl::StrCat(emitted_name, ".bound");
  return absl::StrCat("centreon.", emitted_name, ".bound");
}

}  // namespace com::centreon::broker::otlp
