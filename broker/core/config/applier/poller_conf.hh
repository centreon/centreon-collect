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
#include <optional>
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
  /* A version the poller could not apply, and why. */
  struct rejection {
    std::string version;
    std::string error;
  };

  /* The delivery automaton: what the poller runs compared to what Broker
   * prepared for it. See doc/conf-delivery-simplification-fr.md, section 1.2,
   * for its transitions, whose numbers the comments refer to. */
  enum class delivery_state : uint8_t {
    /* Broker does not know the configuration the poller runs, and asked the
     * poller for it. */
    unknown,
    /* Nothing to send. */
    up_to_date,
    /* A configuration is prepared and waits to be sent. */
    owed,
    /* A configuration was sent, the acknowledgement is awaited. */
    in_flight,
    /* The poller could not apply the configuration prepared for it. */
    rejected,
  };

  /* The database automaton (section 1.3). */
  enum class db_state : uint8_t {
    /* The database holds what <ID>.prot describes. */
    up_to_date,
    /* An acknowledged diff-<ID>.prot waits for the round to close. */
    pending,
  };

 private:
  uint64_t _poller_id;
  std::shared_ptr<spdlog::logger> _logger;
  delivery_state _delivery = delivery_state::up_to_date;
  db_state _db = db_state::up_to_date;
  /* The latest configuration version prepared for this poller. */
  std::string _available_conf;
  /* The configuration version this poller runs, as it told us. */
  std::string _engine_conf;
  /* In in_flight, the version handed over to the poller. Engine applies the
   * DiffStates it receives in order, so a second one sent meanwhile would be
   * applied on top of a configuration it was not computed from. It may be
   * older than _available_conf: a newer export arrived after it left
   * (transition 11), and is sent once the poller answers. */
  std::string _conf_in_flight;
  /* The last version this poller could not apply, and why; none in the usual
   * case. Kept after the poller leaves rejected, so that GetPollers still
   * tells it, until a configuration is acknowledged. */
  std::optional<rejection> _rejection;
  /* Whether the poller is still owed the runtime snapshot of its resources:
   * true by construction, a poller that was just registered got nothing yet,
   * and again once Broker rebuilt and restored a configuration it had lost;
   * false once the stream serving the poller sent the snapshot, with a
   * configuration diff or alone. It belongs to neither automaton. */
  bool _runtime_state_owed = true;

  void _settle();

 public:
  poller_conf(uint64_t poller_id,
              const std::shared_ptr<spdlog::logger>& logger,
              std::string engine_conf = {});

  uint64_t poller_id() const noexcept { return _poller_id; }
  delivery_state delivery() const noexcept { return _delivery; }
  db_state db() const noexcept { return _db; }
  const std::string& available_conf() const noexcept { return _available_conf; }
  const std::string& engine_conf() const noexcept { return _engine_conf; }
  const std::string& conf_in_flight() const noexcept { return _conf_in_flight; }
  const std::optional<rejection>& last_rejection() const noexcept {
    return _rejection;
  }
  bool broker_knows_conf() const noexcept {
    return _delivery != delivery_state::unknown;
  }
  bool runtime_state_owed() const noexcept { return _runtime_state_owed; }
  void set_runtime_state_owed(bool owed) noexcept {
    _runtime_state_owed = owed;
  }

  bool owes_conf() const noexcept;
  bool needs_update() const noexcept;
  bool awaits_acknowledgement() const noexcept;

  void on_connected(std::string engine_conf);
  void on_disconnected();
  void on_relay_request(const std::string& engine_conf);
  void on_conf_unknown();
  void on_conf_recovered();
  void prepare(const std::string& version);
  void on_announced(const std::string& version);
  bool on_sent(std::string_view version);
  bool on_acknowledged(const std::string& version);
  bool on_rejected(const std::string& running_version,
                   const std::string& failed_version,
                   const std::string& error);
  void leave_round() noexcept;
};
}  // namespace com::centreon::broker::config::applier

#endif /* !CCB_CONFIG_APPLIER_POLLER_CONF_HH */
