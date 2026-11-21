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

/* Replays the BAM timeperiod fixtures (the .conf files under
 * broker/bam/test/time/cfg, historically exercised by
 * broker/bam/test/time/check_timeperiod.cc) against
 * the shared common::timeperiods library, going through the text to protobuf
 * converter (common/engine_conf/timeperiod_legacy).
 *
 * Each fixture carries a reference instant (ref_time) that is the CORRECT next
 * valid time. broker's own timeperiod evaluates only the weekly schedule, so it
 * fails every exception/exclusion fixture (those are ASSERT_FALSE in the broker
 * test). The shared library implements the full grammar, so here we expect it
 * to match ref_time on EVERY fixture — proving equivalence on the weekly cases
 * and completeness on the exception/exclusion ones. */

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "absl/strings/ascii.h"
#include "common/engine_conf/timeperiod_legacy.hh"
#include "common/timeperiods/timeperiod.hh"

namespace cfg = com::centreon::engine::configuration;
using com::centreon::common::timeperiods::timeperiod;

namespace {

std::string trim(std::string_view s) {
  return std::string{absl::StripAsciiWhitespace(s)};
}

/* IANA zone from a name optionally prefixed by ':' (the TZ-env form used in the
 * fixtures, e.g. ":Europe/Paris"). Empty → the process local zone (the test
 * harness pins TZ=Europe/Paris), unknown → local as a safe fallback. */
absl::TimeZone load_zone(std::string name) {
  if (!name.empty() && name.front() == ':')
    name.erase(0, 1);
  name = trim(name);
  if (name.empty())
    return absl::LocalTimeZone();
  absl::TimeZone tz;
  if (!absl::LoadTimeZone(name, &tz))
    return absl::LocalTimeZone();
  return tz;
}

/* Parse "YYYY-MM-DD HH:MM:SS" with an optional " :Zone" suffix into an absolute
 * instant. Without a suffix the local zone is used (as broker's mktime does).
 */
bool parse_time(const std::string& value, time_t& out) {
  absl::TimeZone tz = absl::LocalTimeZone();
  std::string dt = value;
  size_t zpos = value.find(" :");
  if (zpos != std::string::npos) {
    dt = value.substr(0, zpos);
    tz = load_zone(value.substr(zpos + 1));
  }
  int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
  if (sscanf(trim(dt).c_str(), "%4d-%2d-%2d %2d:%2d:%2d", &y, &mo, &d, &h, &mi,
             &s) != 6)
    return false;
  out = absl::ToTimeT(tz.At(absl::CivilSecond(y, mo, d, h, mi, s)).pre);
  return true;
}

int weekday_index(const std::string& token) {
  static const char* const days[] = {"sunday",    "monday",   "tuesday",
                                     "wednesday", "thursday", "friday",
                                     "saturday"};
  for (int i = 0; i < 7; ++i)
    if (token == days[i])
      return i;
  return -1;
}

/* One timeperiod block of a fixture: the protobuf being filled + its evaluation
 * timezone (the "timezone=" line, empty if none). */
struct block {
  cfg::Timeperiod tp;
  std::string tz;
};

struct fixture {
  std::vector<block> blocks;
  time_t preferred_time = 0;
  time_t current_time = 0;
  time_t ref_time = 0;
};

/* Parse a fixture file, mirroring broker's parse_file but building protobufs
 * through the legacy converter instead of broker's time::timeperiod. */
bool parse_fixture(const std::string& path, fixture& out) {
  std::ifstream in(path);
  if (!in.is_open())
    return false;
  block cur;
  std::string raw;
  while (std::getline(in, raw)) {
    std::string line = trim(raw);
    if (line.empty() || line[0] == '#')
      continue;
    size_t eq = line.find('=');
    if (eq == std::string::npos)
      return false;
    std::string key = trim(line.substr(0, eq));
    std::string value = trim(line.substr(eq + 1));

    if (key == "preferred_time") {
      if (!parse_time(value, out.preferred_time))
        return false;
    } else if (key == "current_time") {
      if (!parse_time(value, out.current_time))
        return false;
    } else if (key == "ref_time") {
      if (!parse_time(value, out.ref_time))
        return false;
    } else if (key == "weekday") {
      size_t sp = value.find_first_of(" \t");
      std::string day = (sp == std::string::npos) ? value : value.substr(0, sp);
      int idx = weekday_index(day);
      if (idx < 0)
        continue;  // broker silently ignores a non-weekday first token.
      std::string range =
          (sp == std::string::npos) ? "" : trim(value.substr(sp));
      if (!cfg::legacy_set_weekday(cur.tp, idx, range))
        return false;
    } else if (key == "speday") {
      size_t sp = value.find_first_of(" \t");
      if (sp == std::string::npos)
        return false;
      if (!cfg::legacy_add_exception(cur.tp, value.substr(0, sp),
                                     trim(value.substr(sp))))
        return false;
    } else if (key == "exclusion") {
      cur.tp.mutable_exclude()->add_data(value);
    } else if (key == "timezone") {
      cur.tz = value;
    } else if (key == "timeperiod") {
      cur.tp.set_timeperiod_name(value);
      cur.tp.set_alias(value);  // common's ctor rejects an empty alias.
      out.blocks.push_back(std::move(cur));
      cur = block{};
    } else
      return false;
  }
  return true;
}

void check_fixture(const std::string& path) {
  fixture fx;
  ASSERT_TRUE(parse_fixture(path, fx)) << "could not parse " << path;
  ASSERT_FALSE(fx.blocks.empty()) << "no timeperiod in " << path;

  // Build every block as a shared-library timeperiod, then resolve so the
  // exclusion name → pointer links are wired across the set.
  timeperiod_map all;
  for (const block& b : fx.blocks) {
    auto tp = std::make_shared<timeperiod>(b.tp);
    all[tp->get_name()] = tp;
  }
  uint32_t w = 0, e = 0;
  for (auto& [name, tp] : all)
    tp->resolve(all, w, e);

  // The "main" timeperiod is the last block; evaluate from max(preferred,
  // current) like broker's checkPeriod. notif=true reproduces broker's "return
  // -1 when no valid time is ever found" convention.
  const block& main = fx.blocks.back();
  auto tp = all[main.tp.timeperiod_name()];
  absl::TimeZone tz = load_zone(main.tz);
  time_t preferred = std::max(fx.preferred_time, fx.current_time);
  time_t valid = tp->get_next_valid_time(preferred, true, tz);

  EXPECT_EQ(valid, fx.ref_time)
      << path << "\n  preferred=" << preferred << " got=" << valid
      << " expected=" << fx.ref_time;
}

}  // namespace

// Every BAM fixture's next valid time must match its (correct) reference
// instant when evaluated by the shared library.
TEST(BamLegacyTimeperiods, AllFixturesMatchReference) {
  namespace fs = std::filesystem;
  fs::path root(BAM_TP_CFG_DIR);
  ASSERT_TRUE(fs::exists(root)) << "missing fixtures directory " << root;

  std::vector<fs::path> files;
  for (const auto& entry : fs::recursive_directory_iterator(root))
    if (entry.is_regular_file() && entry.path().extension() == ".conf")
      files.push_back(entry.path());
  std::sort(files.begin(), files.end());
  ASSERT_FALSE(files.empty()) << "no .conf fixtures under " << root;

  for (const auto& f : files)
    check_fixture(f.string());
}
