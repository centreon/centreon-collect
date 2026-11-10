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
#ifndef CCB_CONFIG_APPLIER_POLLER_CONF_HH
#define CCB_CONFIG_APPLIER_POLLER_CONF_HH
#include <spdlog/spdlog.h>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace com::centreon::broker::config::applier {
/**
 * @brief The names of the configuration files of one poller, in the pollers
 * configuration directory. Nothing else builds them.
 *
 *  - `<ID>.prot`: the configuration the poller acknowledged, the reference of
 *    the cache and of the database;
 *  - `new-<ID>.prot`: a configuration prepared for it, not acknowledged yet;
 *  - `diff-<ID>.prot`: the difference from `<ID>.prot` to `new-<ID>.prot`,
 *    until the global diff carries it to the database.
 *
 * It only names them: reading and writing go through common/proto_file.
 *
 * It refers to the directory without copying it: the directory must outlive
 * it, hence no temporary is accepted.
 */
class poller_conf_files {
  const std::filesystem::path& _dir;
  uint64_t _poller_id;

 public:
  poller_conf_files(const std::filesystem::path& dir, uint64_t poller_id);
  poller_conf_files(std::filesystem::path&& dir, uint64_t poller_id) = delete;

  uint64_t poller_id() const noexcept { return _poller_id; }
  std::filesystem::path stored_path() const;
  std::filesystem::path new_path() const;
  std::filesystem::path diff_path() const;

  static uint64_t stored_id(const std::filesystem::path& p);
  static uint64_t new_id(const std::filesystem::path& p);
  static uint64_t diff_id(const std::filesystem::path& p);
};

/**
 * @brief What Broker knows of the configuration of one poller: the version it
 * runs, the one prepared for it, the one in flight, the one it rejected, and
 * where it stands in the configuration round.
 *
 * The connection itself is not here, it belongs to peer_registry. A
 * poller_conf lives as long as Broker knows the poller, connected or not: it is
 * created by the first of its files at startup, its connection or an export,
 * and destroyed by RemovePoller. For now, a connection and a disconnection
 * both bring it back to its initial state (see on_connected()).
 */
class poller_conf {
 public:
  /* Where a poller stands in the current configuration round. */
  enum class conf_round : uint8_t {
    /* Not part of the round: nothing was handed over to it, or what it
     * acknowledged has already been carried to the database. */
    out,
    /* Its available_conf has been handed over, the acknowledgement is
     * awaited. */
    sent,
    /* It acknowledged its available_conf, and waits for the round to
     * close. */
    acknowledged,
  };

 private:
  uint64_t _poller_id;
  std::shared_ptr<spdlog::logger> _logger;

 public:
  /* The latest configuration version prepared for this poller. */
  std::string available_conf;
  /* The configuration version this poller runs, as it told us. */
  std::string engine_conf;
  /* Where this poller stands in the current configuration round. */
  conf_round round = conf_round::out;
  /* Whether Broker holds the content of the configuration this poller runs:
   * a <ID>.prot on disk, or a <ID>.lck announcing one. False is not a
   * property of the poller -- which may well tell us in engine_conf the
   * version it runs -- but of Broker, and it is what makes it ask the poller
   * for its configuration back. */
  bool broker_knows_poller_conf = true;
  /* Whether the poller is still owed the runtime snapshot of its resources:
   * true by construction, a poller that was just registered got nothing yet,
   * and again once Broker rebuilt and restored a configuration it had lost;
   * false once the stream serving the poller sent the snapshot, with a
   * configuration diff or alone. */
  bool runtime_state_owed = true;
  /* The version last handed over to this poller, until it acknowledges a
   * configuration -- applied or not; empty when nothing is in flight. Engine
   * applies the DiffStates it receives in order, so a second one sent
   * meanwhile would be applied on top of a configuration it was not computed
   * from. */
  std::string conf_in_flight;
  /* The last version this poller could not apply, and why. That version is
   * not sent again: it would fail the same way. A new export, or a
   * reconnection of the poller, is what tries again. */
  std::string rejected_conf;
  std::string rejection_error;

  poller_conf(uint64_t poller_id,
              const std::shared_ptr<spdlog::logger>& logger,
              std::string engine_conf = {});

  uint64_t poller_id() const noexcept { return _poller_id; }
  void on_connected(std::string engine_conf);
  void on_disconnected();
  bool owes_conf() const noexcept;
  bool needs_update() const noexcept;
  bool mark_sent(std::string_view version);
  bool acknowledge(const std::string& version);
  bool reject(const std::string& running_version,
              const std::string& failed_version,
              const std::string& error);
};
}  // namespace com::centreon::broker::config::applier

#endif /* !CCB_CONFIG_APPLIER_POLLER_CONF_HH */
