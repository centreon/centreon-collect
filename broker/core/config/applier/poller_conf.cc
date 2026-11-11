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

/**
 * @brief Whether the version prepared for a poller is one to send: there is
 * one, the poller does not run it, and it did not fail to apply it -- it would
 * fail the same way.
 *
 * @param available_version The version prepared.
 * @param engine_version The version the poller runs.
 * @param last_rejection The last version it could not apply, if any.
 */
bool owes(
    const std::string& available_version,
    const std::string& engine_version,
    const std::optional<poller_conf::rejection>& last_rejection) noexcept {
  return !available_version.empty() && available_version != engine_version &&
         !(last_rejection && available_version == last_rejection->version);
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
      _engine_conf{std::move(engine_conf)} {}

/**
 * @brief Whether the configuration prepared for this poller is the one in
 * flight, its acknowledgement awaited.
 *
 * In in_flight, the version sent may be an older one, overtaken by a newer
 * export (transition 11): the poller is then still owed the newer one.
 */
bool poller_conf::awaits_acknowledgement() const noexcept {
  return _delivery == delivery_state::in_flight && !_available_conf.empty() &&
         _conf_in_flight == _available_conf;
}

/**
 * @brief Work out the delivery state from the versions, after a transition
 * changed them.
 *
 * Something in flight wins: nothing else is sent before the poller answers.
 * Then a version to send. unknown only lasts while there is nothing of the
 * kind: an export prepared while Broker does not know what the poller runs is
 * sent at once (transition 4, which step 4 of the plan makes wait).
 */
void poller_conf::_settle() {
  if (!_conf_in_flight.empty())
    _delivery = delivery_state::in_flight;
  else if (owes(_available_conf, _engine_conf, _rejection))
    _delivery = delivery_state::owed;
  else if (_delivery == delivery_state::unknown)
    return;
  else if (!_available_conf.empty() && _rejection &&
           _available_conf == _rejection->version &&
           _available_conf != _engine_conf)
    _delivery = delivery_state::rejected;
  else
    _delivery = delivery_state::up_to_date;
}

/**
 * @brief The poller just connected: everything Broker knew of its
 * configuration is forgotten, except the version it announces.
 *
 * This is what a new poller_conf was when its lifetime was the connection's,
 * and it is kept as such for now: what was owed, in flight or rejected is
 * worked out again from the files (see broker_state::add_peer()). A rejected
 * version, in particular, is tried again (transition 19), and a version in
 * flight is acknowledged by the announcement or owed again (13, 15).
 *
 * @param engine_conf The configuration version the poller runs, if known.
 */
void poller_conf::on_connected(std::string engine_conf) {
  *this = poller_conf{_poller_id, _logger, std::move(engine_conf)};
  _registration = registration::connected;
}

/**
 * @brief The poller is gone: it owes nothing and takes no part in the
 * configuration round any more, as if its poller_conf did not exist.
 *
 * The plan keeps in_flight across a disconnection (transition 12); for now the
 * reconnection decides from the files, which gives the same outcome.
 */
void poller_conf::on_disconnected() {
  *this = poller_conf{_poller_id, _logger};
}

/**
 * @brief topology.cache says this poller was last reached through a relay: the
 * registry holds an entry for it, expected, not connected.
 *
 * Only a poller seen behind a relay can be expected: topology.cache records the
 * relay of each poller that came through one, and nothing else. Whether it
 * comes back through the same relay is only known when it does (see
 * peer_registry::register_poller_via_relay()).
 *
 * Nothing else changes: at startup the poller is in its initial state anyway,
 * and an entry already there is left as it is (see
 * peer_registry::restore_pollers_from_cache()) -- a connected poller stays
 * connected.
 */
void poller_conf::on_expected() noexcept {
  if (_registration == registration::none)
    _registration = registration::expected;
}

/**
 * @brief A relay asks for the configuration of this poller, telling the
 * version it knows.
 *
 * The request opens a new exchange: a version handed over before will not be
 * acknowledged through it, so what was in flight is dropped -- unless it is
 * the version prepared for the poller, which the answer to the request sends
 * again, and whose acknowledgement still counts for the round.
 *
 * Reaching Broker through a relay is a connection: a poller expected from
 * topology.cache is connected from now on.
 *
 * @param engine_conf The version the relay knows, empty if none.
 */
void poller_conf::on_relay_request(const std::string& engine_conf) {
  _registration = registration::connected;
  if (!engine_conf.empty())
    _engine_conf = engine_conf;
  if (!awaits_acknowledgement())
    _conf_in_flight.clear();
  _settle();
}

/**
 * @brief Broker has no configuration stored for this poller and no
 * announcement of one: it asks the poller for its own (transition 2).
 */
void poller_conf::on_conf_unknown() {
  _delivery = delivery_state::unknown;
  _settle();
}

/**
 * @brief Broker holds the configuration of the poller again: the poller sent
 * it back, or another one is stored or announced (transitions 5 and 6).
 */
void poller_conf::on_conf_recovered() {
  if (_delivery == delivery_state::unknown)
    _delivery = delivery_state::up_to_date;
  _settle();
}

/**
 * @brief A configuration was prepared for the poller (transitions 4, 7, 8, 11
 * and 18).
 *
 * Preparing again the version already prepared changes nothing: two
 * announcements for one export, which PHP is free to make, would otherwise
 * orphan the acknowledgement of the version in flight, and the round would
 * never close.
 *
 * A new version takes the poller out of the database automaton's pending
 * state, as it took it out of the round until now: its acknowledged diff has
 * been carried to the database just before (see
 * broker_state::_integrate_acknowledged_diffs()).
 *
 * @param version The version prepared.
 */
void poller_conf::prepare(const std::string& version) {
  if (_available_conf == version)
    return;
  _available_conf = version;
  _db = db_state::up_to_date;
  _settle();
}

/**
 * @brief The poller announced at its connection the version prepared for it:
 * it applied it, and its acknowledgement never reached Broker. The
 * announcement is that acknowledgement (transitions 10 and 13), without the
 * sending that never took place in this session.
 *
 * @param version The version prepared, which the poller runs.
 */
void poller_conf::on_announced(const std::string& version) {
  _available_conf = version;
  _conf_in_flight.clear();
  _rejection.reset();
  _db = db_state::pending;
  _settle();
}

/**
 * @brief Record that a configuration has been handed over to this poller
 * (transition 9).
 *
 * What is sent is decided without the lock of the poller configurations (the
 * diff file is read, or the relay is found up to date, before this call). A
 * newer configuration can therefore be prepared in between: the poller is then
 * in flight with an older version, and the newer one stays owed, sent once the
 * poller answers.
 *
 * With no configuration prepared at all, the version still left: nothing else
 * is sent until the poller says which version it runs.
 *
 * @param version The version of the configuration actually sent.
 *
 * @return true if it is the version prepared for the poller, whose
 * acknowledgement the round now waits for.
 */
bool poller_conf::on_sent(std::string_view version) {
  _conf_in_flight = version;
  _settle();
  if (_available_conf.empty()) {
    _logger->debug(
        "Configuration '{}' sent to poller {}, which has none prepared: "
        "nothing to mark",
        version, _poller_id);
    return false;
  }
  if (_available_conf != version) {
    _logger->info(
        "Configuration '{}' sent to poller {} is no longer the latest one "
        "('{}'): the latest stays owed",
        version, _poller_id, _available_conf);
    return false;
  }
  _logger->debug("New configuration sent to poller {}", _poller_id);
  return true;
}

/**
 * @brief Record what the poller acknowledged, and tell whether it is the
 * configuration prepared for it (transitions 13 and 14).
 *
 * Whatever the version, the poller runs it now, and it no longer has anything
 * in flight: the next delivery can be made.
 *
 * Only the prepared version counts as this poller's part of the round, though.
 * An older one -- sent before a newer export replaced it -- is not what
 * `new-<ID>.prot` and `diff-<ID>.prot` hold any more: renaming the first or
 * feeding the cache with the second would claim the poller runs a
 * configuration it never received. The newer one stays owed (14), and is sent
 * once this acknowledgement has said which version the poller runs.
 *
 * @param version The version the poller acknowledged.
 *
 * @return true if @p version is the configuration prepared and sent for this
 * poller.
 */
bool poller_conf::on_acknowledged(const std::string& version) {
  _logger->info(
      "Poller {} acknowledged '{}' (it ran '{}', '{}' is the latest prepared)",
      _poller_id, version, _engine_conf, _available_conf);
  const bool awaited = awaits_acknowledgement() && _available_conf == version;
  _engine_conf = version;
  _conf_in_flight.clear();
  if (awaited) {
    _db = db_state::pending;
    _rejection.reset();
  }
  _settle();
  return awaited;
}

/**
 * @brief Record that the poller could not apply a configuration it was sent
 * (transitions 16 and 17).
 *
 * The poller still runs @p running_version, and nothing is in flight any more.
 * When @p failed_version is the configuration prepared for it, the poller
 * becomes rejected and leaves the round, since it will not acknowledge it:
 * sending it again would fail the same way. A new export (18), or a
 * reconnection of the poller (19), tries again. An older version only says
 * what the poller runs, as with on_acknowledged(): the newer one stays owed and
 * is sent now (17).
 *
 * @param running_version The version the poller still runs.
 * @param failed_version The version it could not apply.
 * @param error Why it could not.
 *
 * @return true if @p failed_version is the configuration prepared for it, so
 * that its files are left to remove.
 */
bool poller_conf::on_rejected(const std::string& running_version,
                              const std::string& failed_version,
                              const std::string& error) {
  _logger->error(
      "Poller {} could not apply the configuration '{}', it still runs '{}': "
      "{}",
      _poller_id, failed_version, running_version, error);
  _engine_conf = running_version;
  _conf_in_flight.clear();
  const bool prepared = _available_conf == failed_version;
  if (prepared) {
    _rejection = rejection{failed_version, error};
    _db = db_state::up_to_date;
  }
  _settle();
  return prepared;
}

/**
 * @brief Whether a configuration is prepared for this poller and still owes it
 * a delivery.
 *
 * This is what keeps a configuration round open, whether or not the delivery
 * can be made right now: see broker_state::try_close_conf_round(). A poller in
 * flight with an older version than the one prepared still owes it.
 */
bool poller_conf::owes_conf() const noexcept {
  if (_delivery == delivery_state::owed)
    return true;
  return _delivery == delivery_state::in_flight && !awaits_acknowledgement() &&
         owes(_available_conf, _engine_conf, _rejection);
}

/**
 * @brief Whether the configuration owed to this poller can be handed over now.
 *
 * Not while another one is in flight. Engine applies its DiffStates in order:
 * a configuration sent before the previous one is acknowledged would be a
 * difference computed from the version Broker believed the poller ran, applied
 * on top of the one it actually runs. The acknowledgement says which version
 * that is, and the delivery waits for it. It always comes: Engine acknowledges
 * a configuration it could not apply too (see on_rejected()), and a poller that
 * dies on the way disconnects, which resets it (see on_disconnected()).
 */
bool poller_conf::needs_update() const noexcept {
  return _delivery == delivery_state::owed;
}

/**
 * @brief The acknowledged difference of this poller was carried to the
 * database: the database automaton goes back to up_to_date.
 */
void poller_conf::leave_round() noexcept {
  _db = db_state::up_to_date;
}

}  // namespace com::centreon::broker::config::applier
