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
#include "broker/core/config/applier/poller_conf.hh"

#include <absl/strings/numbers.h>
#include <absl/strings/strip.h>
#include <fmt/format.h>

namespace com::centreon::broker::config::applier {

namespace {
/**
 * @brief The poller ID a file name of the pollers configuration directory
 * carries after @p prefix.
 *
 * @param p The path of the file.
 * @param prefix What comes before the ID: empty, "new-" or "diff-".
 *
 * @return The poller ID, or 0 if @p p is not `<prefix><ID>.prot`.
 */
uint64_t poller_id_after_prefix(const std::filesystem::path& p,
                                std::string_view prefix) {
  if (p.extension() != ".prot")
    return 0;
  const std::string stem = p.stem().string();
  std::string_view id = stem;
  uint64_t retval = 0;
  if (!absl::ConsumePrefix(&id, prefix) || !absl::SimpleAtoi(id, &retval))
    return 0;
  return retval;
}
}  // namespace

/**
 * @brief Name the files of one poller.
 *
 * @param dir The pollers configuration directory.
 * @param poller_id The poller ID.
 */
poller_conf_files::poller_conf_files(const std::filesystem::path& dir,
                                     uint64_t poller_id)
    : _dir{dir}, _poller_id{poller_id} {}

/**
 * @brief The configuration the poller acknowledged: `<ID>.prot`.
 */
std::filesystem::path poller_conf_files::stored_path() const {
  return _dir / fmt::format("{}.prot", _poller_id);
}

/**
 * @brief The configuration prepared for the poller and not acknowledged yet:
 * `new-<ID>.prot`.
 */
std::filesystem::path poller_conf_files::new_path() const {
  return _dir / fmt::format("new-{}.prot", _poller_id);
}

/**
 * @brief The difference from `<ID>.prot` to `new-<ID>.prot`: `diff-<ID>.prot`.
 */
std::filesystem::path poller_conf_files::diff_path() const {
  return _dir / fmt::format("diff-{}.prot", _poller_id);
}

/**
 * @brief The poller ID a stored configuration file name carries.
 *
 * Only `<ID>.prot` is a stored configuration. `new-<ID>.prot` and
 * `diff-<ID>.prot` are work files of the push in progress and must not be read
 * as the configuration of a poller.
 *
 * @param p The path of a file of the pollers configuration directory.
 *
 * @return The poller ID, or 0 for anything that is not `<ID>.prot`.
 */
uint64_t poller_conf_files::stored_id(const std::filesystem::path& p) {
  return poller_id_after_prefix(p, "");
}

/**
 * @brief The poller a configuration prepared and not acknowledged yet belongs
 * to, from its file name.
 *
 * @param p The path of a file of the pollers configuration directory.
 *
 * @return The poller ID if @p p is a `new-<ID>.prot`, 0 otherwise.
 */
uint64_t poller_conf_files::new_id(const std::filesystem::path& p) {
  return poller_id_after_prefix(p, "new-");
}

/**
 * @brief The poller a difference waiting for the database belongs to, from its
 * file name.
 *
 * @param p The path of a file of the pollers configuration directory.
 *
 * @return The poller ID if @p p is a `diff-<ID>.prot`, 0 otherwise.
 */
uint64_t poller_conf_files::diff_id(const std::filesystem::path& p) {
  return poller_id_after_prefix(p, "diff-");
}

/**
 * @brief Create the configuration state of a poller Broker just learned about:
 * nothing prepared, nothing in flight, nothing rejected.
 *
 * @param poller_id The poller ID.
 * @param logger The logger of the configuration.
 * @param engine_conf The configuration version the poller is known to run, if
 * any.
 */
poller_conf::poller_conf(uint64_t poller_id,
                         const std::shared_ptr<spdlog::logger>& logger,
                         std::string engine_conf)
    : _poller_id{poller_id},
      _logger{logger},
      engine_conf{std::move(engine_conf)} {}

/**
 * @brief The poller just connected: everything Broker knew of its
 * configuration is forgotten, except the version it announces.
 *
 * This is what a new poller_conf was when its lifetime was the connection's,
 * and it is kept as such for now: what was owed, in flight or rejected is
 * worked out again from the files (see broker_state::add_peer()). A rejected
 * version, in particular, is tried again.
 *
 * @param engine_conf The configuration version the poller runs, if known.
 */
void poller_conf::on_connected(std::string engine_conf) {
  *this = poller_conf{_poller_id, _logger, std::move(engine_conf)};
}

/**
 * @brief The poller is gone: it owes nothing and takes no part in the
 * configuration round any more, as if its poller_conf did not exist.
 */
void poller_conf::on_disconnected() {
  on_connected({});
}

/**
 * @brief Whether a configuration is prepared for this poller and still owes it
 * a delivery.
 *
 * This is what keeps a configuration round open, whether or not the delivery
 * can be made right now: see peer_registry::try_close_conf_round().
 *
 * A version the poller could not apply is not owed any more: it would fail the
 * same way, and keeping it owed would hold the round open for every other
 * poller.
 */
bool poller_conf::owes_conf() const noexcept {
  return round == conf_round::out && !available_conf.empty() &&
         available_conf != engine_conf && available_conf != rejected_conf;
}

/**
 * @brief Whether the configuration owed to this poller can be handed over now.
 *
 * Not while another one is in flight. Engine applies its DiffStates in order:
 * a configuration sent before the previous one is acknowledged would be a
 * difference computed from the version Broker believed the poller ran, applied
 * on top of the one it actually runs. The acknowledgement says which version
 * that is, and the delivery waits for it. It always comes: Engine acknowledges
 * a configuration it could not apply too (see reject()), and a poller that dies
 * on the way disconnects, which resets it (see on_disconnected()).
 */
bool poller_conf::needs_update() const noexcept {
  return owes_conf() && conf_in_flight.empty();
}

/**
 * @brief Record that the prepared configuration has been handed over to this
 * poller, if the configuration sent is still the one prepared for it. This
 * opens a round for it: the acknowledgement it owed for the previous one no
 * longer counts.
 *
 * What is sent is decided without the registry lock (the diff file is read, or
 * the relay is found up to date, before this call). A newer configuration can
 * therefore be prepared in between: marking it as sent would claim the poller
 * received a version it never saw, and nothing would send it any more. Left
 * unmarked, it is still owed and the next read of the stream sends it.
 *
 * With no configuration prepared at all, there is nothing to mark either.
 *
 * @param version The version of the configuration actually sent.
 *
 * @return true if the poller is now marked as served.
 */
bool poller_conf::mark_sent(std::string_view version) {
  /* Whatever is decided below, this version did leave: nothing else is sent
   * to the poller until it says which version it runs. */
  conf_in_flight = version;
  if (available_conf.empty()) {
    _logger->debug(
        "Configuration '{}' sent to poller {}, which has none prepared: "
        "nothing to mark",
        version, _poller_id);
    return false;
  }
  if (available_conf != version) {
    _logger->info(
        "Configuration '{}' sent to poller {} is no longer the latest one "
        "('{}'): the latest stays owed",
        version, _poller_id, available_conf);
    return false;
  }
  round = conf_round::sent;
  _logger->debug("New configuration sent to poller {}", _poller_id);
  return true;
}

/**
 * @brief Record what the poller acknowledged, and tell whether it is the
 * configuration prepared for it.
 *
 * Whatever the version, the poller runs it now, and it no longer has anything
 * in flight: the next delivery can be made.
 *
 * Only the prepared version counts as this poller's part of the round, though.
 * An older one -- sent before a newer export replaced it -- is not what
 * `new-<ID>.prot` and `diff-<ID>.prot` hold any more: renaming the first or
 * feeding the cache with the second would claim the poller runs a
 * configuration it never received. The newer one stays owed, and is sent once
 * this acknowledgement has said which version the poller runs.
 *
 * @param version The version the poller acknowledged.
 *
 * @return true if @p version is the configuration prepared and sent for this
 * poller.
 */
bool poller_conf::acknowledge(const std::string& version) {
  _logger->info(
      "Poller {} acknowledged '{}' (it ran '{}', '{}' is the latest prepared)",
      _poller_id, version, engine_conf, available_conf);
  engine_conf = version;
  conf_in_flight.clear();
  if (round != conf_round::sent || available_conf != version)
    return false;
  round = conf_round::acknowledged;
  rejected_conf.clear();
  rejection_error.clear();
  return true;
}

/**
 * @brief Record that the poller could not apply a configuration it was sent.
 *
 * The poller still runs @p running_version, and nothing is in flight any more.
 * When @p failed_version is the configuration prepared for it, the poller
 * leaves the round, since it will not acknowledge it, and that version is no
 * longer owed: sending it again would fail the same way. A new export, or a
 * reconnection of the poller -- which resets it -- tries again. An
 * older version only says what the poller runs, as with acknowledge(): the
 * newer one stays owed and is sent now.
 *
 * @param running_version The version the poller still runs.
 * @param failed_version The version it could not apply.
 * @param error Why it could not.
 *
 * @return true if @p failed_version is the configuration prepared for it, so
 * that its files are left to remove.
 */
bool poller_conf::reject(const std::string& running_version,
                         const std::string& failed_version,
                         const std::string& error) {
  _logger->error(
      "Poller {} could not apply the configuration '{}', it still runs '{}': "
      "{}",
      _poller_id, failed_version, running_version, error);
  engine_conf = running_version;
  conf_in_flight.clear();
  if (available_conf != failed_version)
    return false;
  rejected_conf = failed_version;
  rejection_error = error;
  round = conf_round::out;
  return true;
}

}  // namespace com::centreon::broker::config::applier
