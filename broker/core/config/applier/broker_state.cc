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

#include "broker/core/config/applier/broker_state.hh"

#include <algorithm>
#include <future>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/post.hpp>

#include "bbdo/bbdo.pb.h"
#include "bbdo/neb.pb.h"
#include "com/centreon/broker/broker_downtime_callbacks.hh"
#include "com/centreon/broker/broker_notification_callbacks.hh"
#include "com/centreon/broker/multiplexing/engine.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/common/file.hh"
#include "com/centreon/common/pool.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/downtimes/downtime_manager.hh"
#include "common/engine_conf/indexed_state.hh"
#include "common/engine_conf/parser.hh"
#include "common/notifications/notification_manager.hh"

using com::centreon::common::log_v2::log_v2;

namespace com::centreon::broker::config::applier {

/**
 * @brief Destructor of the state class.
 */
broker_state::~broker_state() {
  /* Unregister the notification sink so the engine stops referencing the
   * dispatcher before it is destroyed. In the normal shutdown order the engine
   * is already unloaded (deinit() unloads it before state::unload()), so this
   * is defensive; instance_ptr() is null then. */
  if (auto engine = multiplexing::engine::instance_ptr())
    engine->set_notification_sink(nullptr);
  if (_safety_timer) {
    /* Order matters: the flag first, so a handler that is already running stops
     * re-arming, then the cancels, then the drain. The pending wait has to be
     * cancelled too -- it holds a handler that would otherwise be called long
     * after this object is gone. */
    _watch_engine_conf_stopped.store(true);
    _safety_timer->cancel();
    if (_debounce_timer)
      _debounce_timer->cancel();
    if (_cache_config_dir_watcher)
      _cache_config_dir_watcher->cancel();
    /* Drain the watcher: post a barrier on the strand and wait for it. Because
     * the strand serializes every watcher handler, when the barrier runs no
     * handler is in flight or queued, so the resources used by the handler
     * (e.g. _cache_config_dir_watcher) can be destroyed below without a race.
     * This is deadlock-free here: the pool is still running at shutdown (it is
     * stopped only after deinit()), this destructor runs on the main thread
     * (not a pool thread), and it holds no lock the handler could wait on. */
    auto drain = [](auto& strand) {
      if (!strand)
        return;
      std::promise<void> drained;
      auto fut = drained.get_future();
      boost::asio::post(*strand, [&drained] { drained.set_value(); });
      fut.wait();
    };
    drain(_watch_strand);
    /* The watching first, so nothing can post any more configuration work;
     * then the work itself. Its barrier runs behind whatever cycle is in
     * flight, so a configuration being read is finished rather than cut short
     * -- which would leave a half-written .prot behind. */
    drain(_config_strand);
  }
  save_topology_cache();
  /* Hand the started downtimes over to the global cache so they are persisted
   * with it and can be re-injected on the next start. This must happen BEFORE
   * unload() (which destroys the manager) and while the cache is still alive
   * (the base state destructor, which owns it, runs after this one). */
  if (com::centreon::common::downtimes::downtime_manager::is_loaded()) {
    std::vector<Downtime> active;
    for (const auto& [_, dt] :
         com::centreon::common::downtimes::downtime_manager::instance()
             .get_scheduled_downtimes()) {
      if (!dt->is_in_effect())
        continue;
      Downtime d;
      d.set_id(dt->get_downtime_id());
      d.set_host_id(dt->host_id());
      d.set_service_id(dt->service_id());
      d.set_author(dt->get_author());
      d.set_comment_data(dt->get_comment());
      d.set_entry_time(dt->get_entry_time());
      d.set_start_time(dt->get_start_time());
      d.set_end_time(dt->get_end_time());
      d.set_fixed(dt->is_fixed());
      d.set_triggered_by(dt->get_triggered_by());
      d.set_duration(dt->get_duration());
      d.set_started(true);
      d.set_comment_id(dt->get_comment_id());
      d.set_type(dt->service_id() == 0 ? Downtime_DowntimeType_HOST
                                       : Downtime_DowntimeType_SERVICE);
      active.push_back(std::move(d));
    }
    config::applier::state::instance().cache().set_active_downtimes(
        std::move(active));
  }
  /* Hand the notification runtime states over to the global cache so the
   * notification chain (number, timings, notified contacts) is persisted and
   * can be re-injected on the next start. Same ordering constraint as
   * downtimes: BEFORE unload() and while the cache is still alive. */
  if (com::centreon::common::notifications::notification_manager::is_loaded()) {
    std::vector<BrokerCache::NotificationState> states;
    for (const auto& snap :
         com::centreon::common::notifications::notification_manager::instance()
             .snapshot_states()) {
      BrokerCache::NotificationState ns;
      ns.set_host_id(snap.host_id);
      ns.set_service_id(snap.service_id);
      ns.set_number(snap.number);
      ns.set_current_id(snap.current_id);
      ns.set_last(snap.last);
      ns.set_next(snap.next);
      ns.set_initial(snap.initial);
      for (size_t i = 0; i < snap.events.size(); i++) {
        if (!snap.events[i])
          continue;
        auto* e = ns.add_events();
        e->set_category(static_cast<uint32_t>(i));
        e->set_reason_type(static_cast<uint32_t>(snap.events[i]->type));
        e->set_interval(
            static_cast<uint32_t>(snap.events[i]->interval.count()));
        for (const auto& c : snap.events[i]->notified_contacts)
          e->add_notified_contacts(c);
      }
      states.push_back(std::move(ns));
    }
    config::applier::state::instance().cache().set_notification_states(
        std::move(states));
  }
  com::centreon::common::downtimes::downtime_manager::unload();
  com::centreon::common::notifications::notification_manager::unload();
}

/**
 *  Apply a configuration state.
 *
 *  @param[in] s       State to apply.
 *  @param[in] run_mux Set to true if multiplexing must be run.
 */
void broker_state::apply(const com::centreon::broker::config::state& s,
                         bool run_mux) {
  auto logger = log_v2::instance().get(log_v2::CORE);
  /* Load the downtime_manager BEFORE state::apply(): the latter initializes
   * (and, in legacy mode, loads from disk) the global cache, and the cache
   * load re-injects the persisted active downtimes into the manager — so the
   * manager must already exist at that point. */
  {
    auto it = s.params().find("notification_mode");
    _notification_mode = (it != s.params().end() && it->second == "broker")
                             ? notification_mode_broker
                             : notification_mode_engine;
  }
  if (_notification_mode == notification_mode_broker) {
    com::centreon::common::downtimes::downtime_manager::load(
        std::make_unique<broker_downtime_callbacks>(
            com::centreon::common::pool::instance().io_context()));
    /* This message is the signal that Broker now owns downtime management and
     * that the gRPC ScheduleDowntime/DeleteDowntime endpoints are usable. It
     * goes to the CORE logger (enabled at info by default) rather than the
     * CONFIG logger (error by default) so it is reliably observable. */
    logger->info(
        "notification_mode=broker: downtime management enabled, downtime "
        "manager loaded");

    /* Broker owns the notification decision: inject the Broker backend into the
     * notification library. The execution is dispatched to the pollers via
     * pb_notification_execute. In engine mode the manager is never loaded here.
     */
    com::centreon::common::notifications::notification_manager::load(
        std::make_unique<broker_notification_callbacks>());
    logger->info(
        "notification_mode=broker: notification decision enabled, "
        "notification manager loaded");

    /* Register the notification trigger as an event_sink on the multiplexing
     * engine: it drives the notification_manager on each host/service status
     * batch. */
    _notification_dispatcher =
        std::make_unique<broker_notification_dispatcher>();
    multiplexing::engine::instance_ptr()->set_notification_sink(
        _notification_dispatcher.get());
  }

  state::apply(s, run_mux);

  /* The persisted active downtimes are re-injected from
   * _maybe_release_barrier() once the startup readiness barrier releases (i.e.
   * after every output stream has emitted its startup definitions and the
   * engine has flushed them). Doing it here, before the barrier, would let a
   * stale BA service definition clobber the re-injected inherited-downtime
   * depth. In centralized mode the resources are not known yet and the
   * re-injection is a no-op anyway (done later from _process_engine_state after
   * merge). */

  if (s.get_bbdo_version().major_v >= 3) {
    /* The cache directory is set first, so that the watcher is started and the
     * topology cache can be loaded. */
    if (!s.cache_config_dir().empty() && _pollers_config_dir.empty()) {
      set_pollers_config_dir(std::filesystem::path(cache_dir()) /
                             "pollers-configuration/");
      load_topology_cache();
    } else
      set_pollers_config_dir(s.pollers_config_dir());

    // Configuration cache directory (for broker, from php).
    set_cache_config_dir(s.cache_config_dir());
  }
}

/**
 * @brief Invoked by the base startup readiness barrier right after the
 * multiplexing engine is started. Re-inject the persisted active downtimes so
 * they are ordered after the startup definitions the engine just flushed (e.g.
 * the BA virtual service definitions). No-op in centralized mode (resources not
 * known yet; re-injected later from _process_engine_state after merge).
 */
void broker_state::_on_barrier_released() {
  if (_notification_mode == notification_mode_broker) {
    cache().reinject_pending_downtimes();
    cache().reinject_pending_notification_states();
  }
}

/**
 * @brief Get the pollers configurations directory.
 *
 * @return The pollers configurations directory.
 */
const std::filesystem::path& broker_state::pollers_config_dir() const noexcept {
  return _pollers_config_dir;
}

/**
 * @brief Set the pollers configurations directory.
 *
 * @param pollers_config_dir The pollers configurations directory.
 */
void broker_state::set_pollers_config_dir(
    const std::filesystem::path& pollers_config_dir) {
  _pollers_config_dir = pollers_config_dir;
}

/**
 * @brief Set the configuration cache directory.
 *
 * @param engine_conf_dir The configuration cache directory.
 */
void broker_state::set_cache_config_dir(
    const std::filesystem::path& cache_config_dir) {
  _cache_config_dir = cache_config_dir;
  if (!_cache_config_dir.empty()) {
    _logger->info("Watching for changes in '{}'", _cache_config_dir.string());
    /* IN_CLOSE_WRITE and IN_MOVED_TO are the two events that mean "this file is
     * complete": the first when whoever wrote it closed it, the second when it
     * was renamed into place. A <ID>.lck can do without them -- only its name
     * is read -- but pollers.lck has contents, and IN_CREATE fires while the
     * file is still empty.
     *
     * IN_DELETE_SELF and IN_MOVE_SELF are about the watched directory itself,
     * not its content: they are what tells the watcher its watch died and has
     * to be established again. They have to be asked for -- unlike IN_IGNORED,
     * which the kernel delivers on its own -- and without them a cache
     * directory that is renamed rather than deleted would silently stop being
     * watched. */
    _cache_config_dir_watcher = std::make_unique<file::directory_watcher>(
        _cache_config_dir,
        IN_CREATE | IN_MODIFY | IN_ATTRIB | IN_CLOSE_WRITE | IN_MOVED_TO |
            IN_DELETE_SELF | IN_MOVE_SELF,
        true);
    if (!_safety_timer) {
      _logger->debug("Starting engine configuration watcher");
      auto& io_ctx = com::centreon::common::pool::instance().io_context();
      _watch_strand = std::make_unique<
          boost::asio::strand<boost::asio::io_context::executor_type>>(
          io_ctx.get_executor());
      _safety_timer = std::make_unique<boost::asio::steady_timer>(io_ctx);
      _debounce_timer = std::make_unique<boost::asio::steady_timer>(io_ctx);
      _config_strand = std::make_unique<
          boost::asio::strand<boost::asio::io_context::executor_type>>(
          io_ctx.get_executor());
      _start_watching();
    } else {
      /* The machinery was already started by a peer that connected before this
       * configuration was applied, at a time when there was no directory to
       * watch. Two things were missed then and neither comes back on its own:
       * the wait could not be armed, and whatever happened before this point is
       * unknown to us -- that peer's own .lck was not even looked for, since
       * _lck_file_for_poller() gives up without a watcher. So arm the wait,
       * and scan, or a poller that connected first would wait for the safety
       * timer to be given its configuration. */
      boost::asio::post(*_watch_strand, [this] { _arm_inotify_wait(); });
      _post_config_work(true);
    }
  } else if (_cache_config_dir_watcher) {
    _logger->info("Stop watching for changes in '{}'",
                  _cache_config_dir.string());
    _cache_config_dir_watcher.reset();
  }
}

/**
 * @brief Write the topology cache to disk. Called on clean shutdown. Persists
 * (poller_id, relay_id) pairs for all engine peers reachable via a relay so
 * that the central can pre-populate routing hints on restart.
 */
void broker_state::save_topology_cache() const {
  if (_pollers_config_dir.empty())
    return;
  TopologyCache cache;
  {
    absl::ReaderMutexLock lck(&_connected_peers_m);
    for (const auto& [poller_id, relay_id] : _last_known_topology) {
      auto* e = cache.add_entries();
      e->set_poller_id(poller_id);
      e->set_relay_id(relay_id);
    }
  }
  const auto path = _pollers_config_dir / "topology.cache";
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    _logger->warn("Cannot write topology cache: '{}' not accessible",
                  path.string());
    return;
  }
  if (!cache.SerializeToOstream(&f))
    _logger->error("Failed to write topology cache to '{}'", path.string());
  else
    _logger->info("Topology cache written: {} entries", cache.entries_size());
}

/**
 * @brief Load the topology cache from disk. Called once at startup, after
 * _pollers_config_dir is set. Populates _engine_peers with via_remote hints
 * so that PHP diffs pushed during the outage are routed correctly before
 * the relays reconnect.
 */
void broker_state::load_topology_cache() {
  if (_pollers_config_dir.empty())
    return;
  const auto path = _pollers_config_dir / "topology.cache";
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return;
  TopologyCache cache;
  if (!cache.ParseFromIstream(&f)) {
    _logger->warn("Failed to parse topology cache from '{}'", path.string());
    return;
  }
  absl::WriterMutexLock lck(&_connected_peers_m);
  for (const auto& e : cache.entries()) {
    _last_known_topology[e.poller_id()] = e.relay_id();
    if (!_engine_peers.count(e.poller_id())) {
      _engine_peers[e.poller_id()] =
          engine_peer{e.poller_id(), "",   0,     false,       "", "",
                      false,         true, false, e.relay_id()};
    }
  }
  _logger->info("Topology cache loaded: {} hints", cache.entries_size());
}

namespace {
/* How long a burst of events is coalesced before it is handled. Long enough
 * that a deploy-all lands as one batch -- and so reads the configuration store
 * once -- short enough that a push is delivered without a perceptible wait. */
constexpr std::chrono::milliseconds debounce_delay{500};
/* A ceiling on that coalescing, so a stream of events that never stops cannot
 * postpone the work indefinitely. */
constexpr std::chrono::seconds debounce_max_delay{5};
/* How often the safety net fires when no event does. Long on purpose: it only
 * has to catch what inotify structurally cannot report -- a watch that could
 * not be re-established, a filesystem inotify does not serve -- not to detect
 * ordinary configuration pushes. */
constexpr std::chrono::minutes safety_period{5};
/* How soon the net comes back when the watch is down. A lost watch reports
 * nothing at all, so no event can ever wake us up to retry: the usual slow pace
 * would leave Broker blind to configuration changes for minutes, when what
 * normally caused it -- a cache directory being recreated -- is over in
 * seconds. */
constexpr std::chrono::seconds watch_retry_period{5};

/* The file PHP touches last to announce a whole export: it names every poller
 * of the batch, so Broker no longer has to guess where the batch ends. See
 * doc/php-evolutions. The individual <id>.lck files remain supported. */
constexpr std::string_view poller_batch_file{"pollers.lck"};

/**
 * @brief The poller id a stored configuration file name carries.
 *
 * Only `<id>.prot` is a stored configuration. `new-<id>.prot` and
 * `diff-<id>.prot` are work files of the push in progress and must not be read
 * as the configuration of a poller.
 *
 * @param p The path of a file of the pollers configuration directory.
 *
 * @return The poller id, or 0 for anything that is not a stored configuration.
 */
uint64_t stored_poller_config_id(const std::filesystem::path& p) {
  if (p.extension() != ".prot")
    return 0;
  uint64_t retval = 0;
  return absl::SimpleAtoi(p.stem().string(), &retval) ? retval : 0;
}
}  // namespace

/**
 * @brief Read every poller configuration Broker stores, and index their hosts
 * and services by poller.
 *
 * This is what gives a per-poller validation a global view: an object missing
 * from the configuration under validation can then be reported as living on
 * another poller instead of as undefined.
 *
 * The result is deliberately not tied to one poller, so that validating several
 * pollers in a row parses the store once instead of once per poller. Which
 * poller a given validation is about is told through `foreign_objects::self`,
 * and that is what keeps its own objects out of the answers.
 *
 * The index mirrors the intra-poller one built by `state_helper::resolve`,
 * hence services only: anomaly detections are not part of the by-name service
 * index there either.
 *
 * @return The states read, and the index borrowing their strings. Both must
 * outlive the validation that uses the index.
 */
broker_state::foreign_states broker_state::load_foreign_objects() const {
  foreign_states retval;
  if (pollers_config_dir().empty() ||
      !std::filesystem::exists(pollers_config_dir()))
    return retval;

  std::error_code ec;
  for (const auto& entry :
       std::filesystem::directory_iterator(pollers_config_dir(), ec)) {
    uint64_t id = stored_poller_config_id(entry.path());
    if (id == 0)
      continue;

    auto state = std::make_unique<engine::configuration::State>();
    std::ifstream f(entry.path(), std::ios::binary);
    if (!f || !state->ParseFromIstream(&f)) {
      /* A configuration we cannot read only costs us the precision of the
       * diagnostics about that poller, so it is not worth failing the whole
       * validation. */
      _logger->warn(
          "Cannot read the stored configuration '{}' of poller {}: the "
          "validation of the other pollers will not be able to report objects "
          "living on it",
          entry.path().string(), id);
      continue;
    }
    for (const auto& h : state->hosts())
      retval.objects.hosts.emplace(h.host_name(), id);
    for (const auto& s : state->services())
      retval.objects.services.emplace(
          std::pair<std::string_view, std::string_view>(
              s.host_name(), s.service_description()),
          id);
    /* Moving the unique_ptr does not move the message, so every view inserted
     * above stays valid. */
    retval.states.push_back(std::move(state));
  }
  if (ec)
    _logger->warn("Cannot browse the pollers configuration directory '{}': {}",
                  pollers_config_dir().string(), ec.message());
  _logger->debug(
      "Loaded the {} stored poller configurations for the cross-poller "
      "validation: {} hosts and {} services indexed",
      retval.states.size(), retval.objects.hosts.size(),
      retval.objects.services.size());
  return retval;
}

/**
 * @brief Create the <ID>.prot file for a poller with the given configuration.
 * This file will be used by broker to fill the cache and prepare the storage
 * database. The configuration is sent by the poller when Broker lost it.
 *
 * Before writing, we check whether a newer configuration is already in place
 * or being processed (a .lck file from PHP, a new-<ID>.prot being prepared, or
 * a <ID>.prot already installed by the normal flow). In those cases we skip the
 * write so as not to overwrite a more recent configuration.
 *
 * @param conf The configuration of the poller to create the <ID>.prot file for.
 */
void broker_state::create_prot_file(
    const com::centreon::engine::configuration::State& conf) {
  assert(conf.poller_id());
  const uint32_t poller_id = conf.poller_id();

  if (!pollers_config_dir_usable()) {
    _logger->error(
        "No pollers configuration directory: refusing to store the "
        "configuration of poller {} at a relative path",
        poller_id);
    return;
  }

  // Logs the skip reason, clears the unknown flag, and signals to the caller
  // that creation should be skipped.
  auto skip = [&](std::string_view reason) {
    _logger->info("Skipping prot file creation for poller {}: {}", poller_id,
                  reason);
    set_poller_engine_conf_unknown(poller_id, false);
  };

  // If PHP has already sent a new configuration for this poller (signalled by
  // a .lck file), let the normal configuration flow handle it rather than
  // overwriting with the engine's current (possibly older) state.
  if (!_cache_config_dir.empty()) {
    std::filesystem::path lck_file =
        _cache_config_dir / fmt::format("{}.lck", poller_id);
    if (std::filesystem::is_regular_file(lck_file)) {
      skip(fmt::format(
          "'{}' exists, the normal configuration flow will handle it",
          lck_file.string()));
      return;
    }
  }

  // If the normal flow is already preparing a new-<ID>.prot or has already
  // installed a <ID>.prot, do not overwrite it.
  std::filesystem::path prot_file =
      pollers_config_dir() / fmt::format("{}.prot", poller_id);
  std::filesystem::path new_prot_file =
      pollers_config_dir() / fmt::format("new-{}.prot", poller_id);
  if (std::filesystem::is_regular_file(new_prot_file)) {
    skip(fmt::format(
        "'{}' already exists, the normal configuration flow will handle it",
        new_prot_file.string()));
    return;
  }
  if (std::filesystem::is_regular_file(prot_file)) {
    skip(
        fmt::format("'{}' already exists, the normal configuration flow has "
                    "already handled it",
                    prot_file.string()));
    return;
  }

  std::ofstream f(prot_file);
  if (f) {
    conf.SerializeToOstream(&f);
    f.close();
    _logger->debug("Created prot file '{}' for poller id {}",
                   prot_file.string(), poller_id);
    set_poller_engine_conf_unknown(poller_id, false);
    _feed_cache_and_wake_up_resources(poller_id);
  } else {
    _logger->error("Unable to create '{}'", prot_file.string());
  }
}

/**
 * @brief Add a poller to the list of connected pollers.
 *
 * @param poller_id The id of the poller (an id by host)
 * @param broker_name The name of the poller
 */
void broker_state::add_peer(uint64_t poller_id,
                            const std::string& poller_name,
                            const std::string& broker_name,
                            common::PeerType peer_type,
                            bool extended_negotiation,
                            const std::string& engine_conf,
                            const std::string& timezone) {
  assert(poller_id && !broker_name.empty());
  {
    absl::WriterMutexLock lck(&_connected_peers_m);
    const peer_key key{poller_id, poller_name, broker_name};

    auto found_engine = _engine_peers.find(poller_id);

    bool already_present = (found_engine != _engine_peers.end() &&
                            found_engine->second.poller_name == poller_name) ||
                           _broker_peers.count(key) ||
                           _unknown_peers.count(key);
    if (already_present) {
      _logger->warn(
          "Poller '{}' with id {} already known as connected. Replacing it.",
          broker_name, poller_id);
    } else {
      _logger->info("Poller '{}' with id {} connected", broker_name, poller_id);
    }

    /* For ENGINE reconnections, preserve the known engine_conf if the caller
     * did not supply a new one. */
    std::string effective_engine_conf = engine_conf;
    if (effective_engine_conf.empty() && peer_type == common::ENGINE) {
      if (found_engine != _engine_peers.end())
        effective_engine_conf = found_engine->second.engine_conf;
    }

    /* Remove from all maps in case the peer type changed.
     * For BROKER peers, do NOT erase _engine_peers: a relay-registered engine
     * peer (via_remote) and a broker peer may legitimately share the same
     * poller_id (e.g. rrd and Engine both on poller 1) and must not interfere.
     * The engine_peer entry was created by register_engine_peer_via_relay and
     * must survive until _prepare_diff_for_poller uses it. */
    if (peer_type != common::BROKER)
      _engine_peers.erase(poller_id);
    _broker_peers.erase(key);
    _unknown_peers.erase(key);

    switch (peer_type) {
      case common::BROKER:
        _broker_peers[key] = broker_peer{poller_id, poller_name, broker_name,
                                         time(nullptr), extended_negotiation};
        break;
      case common::ENGINE:
        _engine_peers[poller_id] = engine_peer{poller_id,
                                               poller_name,
                                               time(nullptr),
                                               extended_negotiation,
                                               "",
                                               effective_engine_conf,
                                               false,
                                               true,
                                               false,
                                               0u};
        _engine_peers[poller_id].timezone = timezone;
        if (is_relay() && extended_negotiation)
          _pending_config_requests[poller_id] = {poller_name,
                                                 effective_engine_conf};
        break;
      default:
        _unknown_peers[key] =
            unknown_peer{poller_id,     poller_name, broker_name,
                         time(nullptr), peer_type,   extended_negotiation};
        break;
    }
  }
  if (extended_negotiation) {
    if (!_safety_timer) {
      _logger->debug("Starting engine configuration watcher");
      auto& io_ctx = com::centreon::common::pool::instance().io_context();
      _watch_strand = std::make_unique<
          boost::asio::strand<boost::asio::io_context::executor_type>>(
          io_ctx.get_executor());
      _safety_timer = std::make_unique<boost::asio::steady_timer>(io_ctx);
      _debounce_timer = std::make_unique<boost::asio::steady_timer>(io_ctx);
      _config_strand = std::make_unique<
          boost::asio::strand<boost::asio::io_context::executor_type>>(
          io_ctx.get_executor());
      _start_watching();
    }

    /* Feeding the cache and waking up resources in the database */
    _feed_cache_and_wake_up_resources(poller_id);
  }
}

/**
 * @brief Feed the global cache with the poller configuration and wake up
 * resources in the database. Reads the <poller_id>.prot file and publishes
 * the Engine state. If neither a .prot file nor a .lck file is found, the
 * poller configuration is considered lost and Broker will request it from
 * Engine via a DiffState{unknown=true} at the next negotiation.
 *
 * @param poller_id The poller ID.
 * @return true if the configuration was found, false if it is lost/unknown.
 */
bool broker_state::_feed_cache_and_wake_up_resources(uint64_t poller_id) {
  bool retval = true;
  if (!pollers_config_dir_usable()) {
    /* Nothing to feed the cache from, and no relative path to stumble into:
     * this instance simply does not hold poller configurations. */
    _logger->debug(
        "No pollers configuration directory: poller {} keeps the configuration "
        "it came with",
        poller_id);
    return retval;
  }
  std::filesystem::path prot_file =
      pollers_config_dir() / fmt::format("{}.prot", poller_id);
  std::fstream f(prot_file);
  multiplexing::publisher pblshr;
  bool poller_conf_lost = false;
  if (f) {
    auto engine_state = std::make_shared<neb::pb_engine_state>();
    auto& state = engine_state->mut_obj();
    state.ParseFromIstream(&f);
    _logger->debug("Publishing poller {} configuration", poller_id);
    pblshr.write(engine_state);
  } else {
    _logger->info("Unable to fill global cache: cannot open '{}'",
                  prot_file.string());
    poller_conf_lost = true;
  }

  /* The directory watcher has been started but may be there were <ID>.lck
   * files already present in the cache directory. We need to check them
   * and apply the diff if needed.
   */
  _logger->debug("Checking for existing {}.lck file", poller_id);
  /* The lock file is the announcement, and it alone says a delivery is
   * pending. Without it there is nothing to hand over: a `new-<ID>.prot` left
   * on disk is the residue of a delivery whose announcement was already
   * consumed, and pushing it would send this poller a configuration nobody
   * asked for -- possibly one older than what it runs.
   *
   * With it, and provided the prepared file post-dates it
   * (_prepared_conf_is_current), `new-<ID>.prot` is the configuration prepared
   * while the poller was away, and it can be handed over as it stands: the
   * state is on disk and all that is left is the diff against what the poller
   * says it runs. Reading the sources again would redo the expensive half of a
   * cycle -- hash, parse, expand, resolve, and the reload of every stored
   * configuration behind it -- to reach a result already sitting there. */
  if (uint32_t existing_lck = _lck_file_for_poller(poller_id)) {
    if (supports_centralized_conf() && _prepared_conf_is_current(poller_id) &&
        _prepare_diff_from_new_prot_file(poller_id)) {
      _logger->info(
          "Poller {} has a configuration prepared from when it was away: "
          "handing it over without reading its sources again",
          poller_id);
      /* The announcement has been honoured, so it must not survive: the
       * delivery is done and nothing is waiting any more. */
      _remove_lck_file(poller_id);
    } else {
      /* An announcement with nothing prepared for it -- either no
       * `new-<ID>.prot` at all, or one older than the announcement, which
       * belongs to an earlier push. Either way the sources have to be read,
       * which means a full cycle. */
      {
        absl::MutexLock lck(&_lck_set_m);
        _lck_set.insert(existing_lck);
      }
      /* Nothing in the directory changed, so inotify has nothing to say and
       * would never wake the watcher up for this poller: the wait has to be
       * nudged from here, or its configuration would sit until the safety timer
       * fires. */
      if (_watch_strand)
        boost::asio::post(*_watch_strand, [this] { _arm_debounce(); });
    }
    poller_conf_lost = false;
  }
  if (poller_conf_lost) {
    /* Broker is unable to update the cache concerning this poller because
     * no <ID>.prot file is present in the pollers configuration directory
     * and no <ID>.lck file is present in the cache configuration directory.
     * So, no known configuration and no new configuration for this poller.
     * In that case, Broker sends an empty DiffState to the poller that
     * forces the poller to send its current configuration if it has one.
     */
    _logger->info(
        "The configuration of poller {} seems lost or unknown, asking for "
        "it "
        "to the poller",
        poller_id);
    set_poller_engine_conf_unknown(poller_id, true);
    retval = false;
  }
  return retval;
}

/**
 * @brief Check whether the given poller is currently registered as a connected
 * Engine peer. A pending configuration can only be delivered to a connected
 * poller, so this gates the consumption of its <ID>.lck file.
 *
 * @param poller_id The poller ID.
 * @return true if the poller is a connected Engine peer.
 */
bool broker_state::_is_engine_peer_connected(uint64_t poller_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  return _engine_peers.contains(poller_id);
}

/**
 * @brief Get the local timezone advertised by an Engine peer at negotiation
 * time.
 *
 * @param poller_id The poller ID.
 * @return The poller machine's timezone (IANA name), or an empty string when
 * the poller is unknown or sent no timezone.
 */
std::string broker_state::poller_timezone(uint64_t poller_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  auto found = _engine_peers.find(poller_id);
  if (found == _engine_peers.end())
    return {};
  return found->second.timezone;
}

/**
 * @brief Whether a `<poller_id>.lck` is still waiting in the cache directory.
 *
 * That file is the announcement *and* the pending-delivery marker, which is why
 * it is kept until the poller shows up. It says a configuration is waiting, but
 * not which one: the sources may well have changed since, so the poller has to
 * go through a full cycle rather than be handed anything directly.
 *
 * A batch announcement (`pollers.lck`) never leaves such a file behind -- it is
 * consumed as soon as it is read -- and needs none: what it leaves is the
 * prepared configuration itself, which _prepare_diff_from_new_prot_file()
 * hands over as it stands.
 *
 * @param poller_id The poller ID.
 * @return The poller ID when a lock file is waiting for it, 0 otherwise.
 */
uint32_t broker_state::_lck_file_for_poller(uint32_t poller_id) noexcept {
  /* No watcher means no cache directory to look into, and an empty one would
   * make the path below relative. */
  if (!_cache_config_dir_watcher || _cache_config_dir.empty()) {
    return 0;
  }

  std::error_code ec;
  std::filesystem::path lck_file(_cache_config_dir /
                                 fmt::format("{}.lck", poller_id));

  if (std::filesystem::is_regular_file(lck_file, ec)) {
    _logger->debug("Found lock file '{}' for poller id {}", lck_file.string(),
                   poller_id);
    return poller_id;
  }
  if (ec)
    _logger->warn("Cannot check if '{}' is a regular file: {}",
                  lck_file.string(), ec.message());
  return 0;
}

/**
 * @brief Remove the `<poller_id>.lck` announcement, its configuration having
 * been delivered.
 *
 * The lock file is what says "PHP pushed a configuration for this poller and
 * it has not reached it yet". Every path that completes such a delivery --
 * whether it read the sources or handed over an already prepared
 * `new-<poller_id>.prot` -- has to remove it, or the announcement stands
 * forever: PHP waits for the file to disappear, and a leftover one makes
 * Broker redo the same delivery at every restart.
 *
 * @param poller_id The poller ID.
 */
void broker_state::_remove_lck_file(uint32_t poller_id) noexcept {
  if (_cache_config_dir.empty())
    return;
  std::error_code ec;
  std::filesystem::path lck_file =
      _cache_config_dir / fmt::format("{}.lck", poller_id);
  std::filesystem::remove(lck_file, ec);
  if (ec)
    _logger->warn("Cannot remove lock file '{}': {}", lck_file.string(),
                  ec.message());
  else
    _logger->debug("Removed lock file '{}' after processing",
                   lck_file.string());
}

/**
 * @brief Read the poller batch file and consume it.
 *
 * Consumed as soon as it is read, unlike a `<poller_id>.lck`: it only says
 * which pollers an export covers, never that a delivery is still pending --
 * `new-<poller_id>.prot` is what says that. PHP waits for the file to disappear
 * before announcing another batch, which is what keeps the two from racing.
 *
 * @return The poller IDs the batch names, empty when there is no batch.
 */
absl::flat_hash_set<uint32_t> broker_state::_consume_poller_batch() {
  absl::flat_hash_set<uint32_t> retval;
  if (_cache_config_dir.empty())
    return retval;

  const std::filesystem::path batch_file(_cache_config_dir / poller_batch_file);
  std::error_code ec;
  if (!std::filesystem::is_regular_file(batch_file, ec))
    return retval;

  std::ifstream f(batch_file);
  if (!f) {
    _logger->error("Cannot read the poller batch file '{}': {}",
                   batch_file.string(), strerror(errno));
    return retval;
  }
  std::string line;
  while (std::getline(f, line)) {
    std::string_view id = absl::StripAsciiWhitespace(line);
    if (id.empty())
      continue;
    uint32_t poller_id;
    if (absl::SimpleAtoi(id, &poller_id))
      retval.insert(poller_id);
    else
      _logger->warn(
          "Ignoring '{}' in the poller batch file '{}': not a poller id", id,
          batch_file.string());
  }
  f.close();

  std::filesystem::remove(batch_file, ec);
  if (ec)
    _logger->error(
        "Cannot remove the poller batch file '{}': {}. The batch would be "
        "handled again on the next cycle",
        batch_file.string(), ec.message());
  if (!retval.empty())
    _logger->info(
        "Poller batch '{}' announces the configuration of {} poller(s)",
        batch_file.string(), retval.size());
  return retval;
}

/**
 * @brief Called from a Broker. Set the engine configuration of a poller
 * among the list of connected peers.
 *
 * @param poller_id The poller ID.
 * @param engine_conf The new Engine configuration version.
 */
void broker_state::set_poller_engine_conf(uint32_t poller_id,
                                          const std::string& engine_conf) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto found = _engine_peers.find(poller_id);
  if (found == _engine_peers.end()) {
    _logger->info("Poller with id {} not found in connected peers", poller_id);
  } else {
    auto& peer = found->second;
    _logger->info(
        "Poller with id {} available conf '{}' and current version changed "
        "from '{}' to '{}'",
        poller_id, peer.available_conf, peer.engine_conf, engine_conf);
    peer.engine_conf = engine_conf;
  }
}

/**
 * @brief Set the engine configuration unknown flag for the given poller.
 * When set to true, Broker will send a DiffState{unknown=true} to Engine
 * at the next negotiation, asking it to send back its full configuration.
 *
 * @param poller_id The poller ID.
 * @param unknown true to mark the configuration as unknown, false
 * otherwise.
 */
void broker_state::set_poller_engine_conf_unknown(uint64_t poller_id,
                                                  bool unknown) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto found = _engine_peers.find(poller_id);
  if (found != _engine_peers.end()) {
    _logger->info("Poller with id {} engine conf is now {}", poller_id,
                  unknown ? "unknown" : "known");
    found->second.conf_unknown = unknown;
  }
}

/**
 * @brief Check if the Engine configuration for the given poller is known
 * to Broker. Returns false if the configuration has been marked unknown,
 * for example after losing its .prot file with no .lck file available.
 *
 * @param poller_id The poller ID.
 * @return true if the configuration is known, false if unknown.
 */
bool broker_state::is_peer_conf_known(uint64_t poller_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  auto found = _engine_peers.find(poller_id);
  if (found != _engine_peers.end()) {
    return !found->second.conf_unknown;
  }
  return true;
}

/**
 * @brief Remove a poller from the list of connected pollers.
 *
 * @param poller_id The id of the poller to remove.
 */
void broker_state::remove_peer(uint64_t poller_id,
                               const std::string& poller_name,
                               const std::string& broker_name) {
  assert(poller_id && !broker_name.empty());
  absl::WriterMutexLock lck(&_connected_peers_m);
  const peer_key key{poller_id, poller_name, broker_name};
  bool erased = _engine_peers.erase(poller_id) || _broker_peers.erase(key) ||
                _unknown_peers.erase(key);
  if (erased) {
    _logger->info("Peer poller: '{}' - broker: '{}' with id {} disconnected",
                  poller_name, broker_name, poller_id);
  } else {
    _logger->warn(
        "Peer poller: '{}' - broker: '{}' with id {} not found in connected "
        "peers",
        poller_name, broker_name, poller_id);
  }
}

/**
 * @brief Check if a poller is currently connected.
 *
 * @param poller_id The poller to check.
 */
bool broker_state::has_connection_from_poller(uint64_t poller_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  auto it = _engine_peers.find(poller_id);
  return it != _engine_peers.end() && it->second.running;
}

void broker_state::set_instance_running(uint64_t poller_id,
                                        bool running) noexcept {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto it = _engine_peers.find(poller_id);
  if (it != _engine_peers.end())
    it->second.running = running;
}

/**
 * @brief Get the list of connected pollers.
 *
 * @return A vector of engine_peers.
 */
std::vector<broker_state::engine_peer> broker_state::connected_pollers() const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  std::vector<engine_peer> retval;
  retval.reserve(_engine_peers.size());
  for (const auto& [_, peer] : _engine_peers) {
    retval.push_back(peer);
  }
  return retval;
}

/**
 * @brief Get the list of connected peers.
 *
 * @return A vector of peers.
 */
std::vector<broker_state::peer> broker_state::connected_peers() const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  std::vector<peer> retval;
  retval.reserve(_engine_peers.size() + _broker_peers.size() +
                 _unknown_peers.size());
  for (const auto& [_, bp] : _broker_peers) {
    retval.push_back({.poller_id = bp.poller_id,
                      .poller_name = bp.poller_name,
                      .broker_name = bp.broker_name,
                      .connected_since = bp.connected_since,
                      .extended_negotiation = bp.extended_negotiation,
                      .peer_type = common::BROKER});
  }
  for (const auto& [_, ep] : _engine_peers) {
    retval.push_back({.poller_id = ep.poller_id,
                      .poller_name = ep.poller_name,
                      .connected_since = ep.connected_since,
                      .extended_negotiation = ep.extended_negotiation,
                      .peer_type = common::ENGINE,
                      .available_conf = ep.available_conf,
                      .engine_conf = ep.engine_conf,
                      .via_remote = ep.via_remote,
                      .timezone = ep.timezone});
  }
  for (const auto& [_, up] : _unknown_peers) {
    retval.push_back({.poller_id = up.poller_id,
                      .poller_name = up.poller_name,
                      .broker_name = up.broker_name,
                      .connected_since = up.connected_since,
                      .extended_negotiation = up.extended_negotiation,
                      .peer_type = up.peer_type});
  }
  return retval;
}

/**
 * @brief Check if all Engine peers, whose an available configuration has
 * been sent, acknowledged their configuration. If it is the case, Broker
 * can prepare the database for them.
 *
 * This function is a "test-and-reset": if all ENGINE peers have
 * acknowledged, it resets all their conf_acknowledged flags to false before
 * returning true. This prevents two concurrent ack handlers from both
 * entering the global diff block when they check simultaneously under the
 * same write lock.
 *
 * @return True if all Engine peers acknowledged their configuration,
 * false otherwise.
 */
bool broker_state::all_engine_peers_acknowledged() {
  absl::WriterMutexLock lck(&_connected_peers_m);
  bool retval = true;
  uint32_t engine_count = 0;
  uint32_t engine_good = 0;
  uint32_t engine_pending = 0;
  for (const auto& [key, peer] : _engine_peers) {
    /* A peer whose configuration is prepared but not delivered yet is part of
     * this round, even though nothing was sent to it. Leaving it out would let
     * one peer's acknowledgement close the round on its own -- and the global
     * diff then consumes every diff-<N>.prot of the directory, including the
     * ones still waiting for their poller. Those files are never written again,
     * so the peer keeps asking to be updated and Broker keeps failing to open a
     * file that no longer exists. */
    if (_peer_needs_update(peer)) {
      ++engine_pending;
      retval = false;
      continue;
    }
    if (peer.available_conf_sent) {
      if (!peer.conf_acknowledged)
        retval = false;
      else
        ++engine_good;
      ++engine_count;
    }
  }
  _logger->debug(
      "All engine peers acknowledged? {}/{} acknowledged, {} still waiting to "
      "be sent",
      engine_good, engine_count, engine_pending);
  if (retval && engine_count > 0) {
    /* Reset all flags so that a concurrent or subsequent call won't
     * trigger a second global diff publication for the same round. */
    for (auto& [key, peer] : _engine_peers) {
      peer.conf_acknowledged = false;
      peer.available_conf_sent = false;
    }
  }
  return retval && engine_count > 0;
}

/**
 * @brief Tell whether the configuration of a poller is already prepared and
 * only waits for that poller to show up.
 *
 * A poller that is not connected cannot be handed anything, so once its
 * new-<ID>.prot is written there is nothing left to do but wait. Its .lck is
 * deliberately kept in that situation, which means the watcher finds it again
 * on every cycle: without this test, an unreachable poller would have Broker
 * redo the whole preparation -- and, above all, reload every stored poller
 * configuration -- every five seconds forever. The delivery happens when the
 * poller connects, through _lck_file_for_poller().
 *
 * @param poller_id The poller ID.
 * @return True when the poller is absent and its configuration is ready.
 */
bool broker_state::_conf_prepared_for_disconnected_poller(
    uint32_t poller_id) const {
  return !_is_engine_peer_connected(poller_id) &&
         _prepared_conf_is_current(poller_id);
}

/**
 * @brief Whether `new-<poller_id>.prot` is the prepared form of the
 * announcement currently waiting, rather than a leftover of an older one.
 *
 * The two files are written by different actors -- PHP touches the `.lck`, a
 * cycle writes the `.prot` -- and their mere coexistence says nothing: a
 * configuration prepared while the poller was away keeps its `.lck` on disk, so
 * a *newer* push lands next to an *older* prepared file. Taking one for the
 * other loses the new configuration outright: the cycle skips it as "already
 * prepared", or the hand-over delivers the old state and consumes the new
 * announcement with it.
 *
 * What distinguishes them is the order in which they were written. A cycle
 * reads the `.lck` and only then writes the `.prot`, so a prepared file that
 * post-dates the announcement is that announcement, prepared. One that
 * pre-dates it belongs to an earlier push and the sources have to be read
 * again.
 *
 * @param poller_id The poller ID.
 * @return True when a prepared configuration exists and is not older than the
 * waiting announcement (or no announcement waits at all).
 */
bool broker_state::_prepared_conf_is_current(uint32_t poller_id) const {
  if (!pollers_config_dir_usable())
    return false;

  std::error_code ec;
  auto prepared = std::filesystem::last_write_time(
      pollers_config_dir() / fmt::format("new-{}.prot", poller_id), ec);
  if (ec)
    return false;  // nothing prepared

  if (_cache_config_dir.empty())
    return true;
  std::error_code lck_ec;
  auto announced = std::filesystem::last_write_time(
      _cache_config_dir / fmt::format("{}.lck", poller_id), lck_ec);
  if (lck_ec)
    return true;  // nothing announced, so nothing newer to prepare

  /* Equality goes to the announcement: a cycle cannot have prepared a
   * configuration before reading the file that announced it, so the same
   * timestamp means the two are indistinguishable, and redoing the cycle is the
   * only harmless way out. */
  return prepared > announced;
}

/**
 * For each <ID>.lck file found in the cache directory, this function checks
 * if there is a new Engine configuration for the poller with this ID and
 * prepares the diff to propagate.
 *
 * @param force_scan Scan the cache directory whatever the events said. Set by
 * the safety timer and by the first cycle, the two moments when events are not
 * what we are relying on.
 */
void broker_state::_check_last_engine_conf(bool force_scan) {
  _logger->trace("Checking for new Engine configurations");
  absl::flat_hash_set<uint32_t> pollers_set;
  {
    absl::MutexLock lck(&_lck_set_m);
    pollers_set.swap(_lck_set);
  }

  /* A batch names its pollers outright, so the whole export is handled in this
   * single pass whatever the time PHP took to generate it -- which is what the
   * delay-based coalescing cannot promise. Read here rather than on the event,
   * so a batch the events did not report is still picked up by a scan. */
  for (uint32_t poller_id : _consume_poller_batch())
    pollers_set.insert(poller_id);

  /* Fallback: scan the directory for any .lck files that inotify did not
   * report, so that no configuration update is permanently lost.
   *
   * This is a safety net, not the detection mechanism -- inotify is. It runs
   * when the watcher says the events were incomplete (the kernel dropped some,
   * or the watch was lost), and otherwise on a slow period, so that whatever
   * neither inotify nor the watcher's own reporting covers -- a directory on a
   * filesystem where inotify does not work, say -- cannot stay hidden forever.
   * Scanning on every cycle instead would mean walking the directory, and
   * stat-ing every .lck still waiting for its poller, forever and for nothing.
   */
  const bool requested = _scan_requested_by_watcher.load();
  if ((force_scan || requested) && !_cache_config_dir.empty()) {
    if (requested)
      _logger->info(
          "Scanning the engine configuration directory '{}': the changes "
          "reported by inotify were incomplete",
          _cache_config_dir.string());
    std::error_code scan_ec;
    std::filesystem::directory_iterator dir_it(_cache_config_dir, scan_ec);
    if (scan_ec) {
      _logger->warn("Error scanning engine config directory '{}': {}",
                    _cache_config_dir.string(), scan_ec.message());
    } else {
      /* A scan that failed answered nothing, so an explicit request stands and
       * is honoured on the next occasion rather than being dropped here. */
      _scan_requested_by_watcher.store(false);
      for (const auto& entry : dir_it) {
        const auto& p = entry.path();
        if (p.extension() == ".lck") {
          std::string stem = p.stem().string();
          uint32_t poller_id;
          if (absl::SimpleAtoi(stem, &poller_id)) {
            if (pollers_set.contains(poller_id))
              continue;  // already queued by inotify
            /* No inotify event reported this file, so it is a leftover of a
             * push already handled: if its configuration is prepared and the
             * poller is away, the only thing left is its connection. Requeuing
             * it here would reload the whole configuration store for nothing --
             * and the poller's own connection requeues it anyway. */
            if (_conf_prepared_for_disconnected_poller(poller_id)) {
              _logger->debug(
                  "Lock file '{}' left for poller {}, whose configuration is "
                  "already prepared: waiting for the poller to connect",
                  p.string(), poller_id);
              continue;
            }
            _logger->info(
                "Found orphan lock file '{}' not reported by inotify — "
                "scheduling configuration check for poller {}",
                p.string(), poller_id);
            /* The .lck is kept until the poller is connected and its
             * configuration has been delivered. It is consumed in
             * _check_last_engine_conf. */
            pollers_set.insert(poller_id);
          }
        }
      }
    }
  }

  std::error_code ec;
  /* Read once for the whole batch: on a deploy-all, pollers_set holds every
   * poller and re-reading the store for each of them would parse it as many
   * times as it has entries. Each iteration only points `self` at the poller it
   * validates. The read is deferred to the first poller that really needs
   * validating: a batch made only of pollers waiting for their connection must
   * not pay for the whole store, since it is retried on every cycle. */
  /* What the cycle did, told at the end in one line. Broker cannot know how
   * many pollers the platform has, but it does know how many configurations it
   * just prepared -- and which of them it could not hand over. */
  uint32_t conf_ready = 0;
  uint32_t conf_sent = 0;
  uint32_t conf_rejected = 0;
  std::vector<uint32_t> pollers_away;

  std::optional<foreign_states> foreign;
  for (uint32_t poller_id : pollers_set) {
    _logger->debug(
        "Checking if there is a new Engine configuration for poller {}",
        poller_id);
    if (_conf_prepared_for_disconnected_poller(poller_id)) {
      _logger->debug(
          "Poller {} configuration already prepared; waiting for the poller to "
          "connect before delivering it",
          poller_id);
      ++conf_ready;
      pollers_away.push_back(poller_id);
      continue;
    }
    if (!foreign)
      foreign = load_foreign_objects();
    foreign->objects.self = poller_id;
    auto state = std::make_unique<engine::configuration::State>();
    engine::configuration::state_helper state_hlp(state.get());
    engine::configuration::error_cnt err;
    std::string version = common::hash_directory(
        cache_config_dir() / fmt::to_string(poller_id), ec);
    if (ec) {
      _logger->error(
          "Cannot compute the Engine configuration version for poller "
          "'{}': "
          "{}",
          poller_id, ec.message());
      continue;
    }
    engine::configuration::parser p;
    std::filesystem::path centengine_test =
        cache_config_dir() / fmt::to_string(poller_id) / "centengine.test";
    std::filesystem::path centengine_cfg =
        cache_config_dir() / fmt::to_string(poller_id) / "centengine.cfg";
    engine::configuration::parser::build_test_file(centengine_test,
                                                   centengine_cfg, ec);
    if (!ec) {
      try {
        p.parse(centengine_test, state.get(), err);
        state->set_config_version(version);
        state->set_poller_id(poller_id);
        state_hlp.expand(err);
        /* We do not trust the pushed configuration: validate it before storing
         * and delivering it. If it is invalid, refuse to push it (the throw is
         * caught below, so the .prot is not written and no diff is prepared).
         *
         * Being the central, we can do better than a poller alone: the
         * configurations stored for the other pollers tell whether an object
         * this one references is genuinely undefined or merely lives elsewhere.
         * `foreign` is declared outside the loop and owns the strings its
         * index borrows, so it outlives every resolve of the batch. */
        state_hlp.resolve(err, _logger, foreign->objects);
        if (err.config_errors)
          throw com::centreon::exceptions::msg_fmt(
              "configuration for poller {} (version '{}') has {} error(s); "
              "refusing to push it to the poller",
              poller_id, version, err.config_errors);
        if (!pollers_config_dir_usable())
          throw com::centreon::exceptions::msg_fmt(
              "no pollers configuration directory is configured: refusing to "
              "write the configuration of poller {} to a relative path",
              poller_id);
        if (!std::filesystem::exists(pollers_config_dir())) {
          std::filesystem::create_directories(pollers_config_dir(), ec);
          if (ec) {
            _logger->error(
                "Cannot create pollers configuration directory '{}': {}",
                pollers_config_dir().string(), ec.message());
          }
        }
        std::filesystem::path last_prot_conf =
            pollers_config_dir() / fmt::format("new-{}.prot", poller_id);
        std::ofstream f(last_prot_conf);
        if (f) {
          state->SerializeToOstream(&f);
          f.close();
          _logger->info(
              "New Engine configuration for poller {} stored, version '{}'",
              poller_id, version);
        } else {
          _logger->error(
              "Cannot write the new Engine protobuf configuration '{}': {}",
              last_prot_conf.string(), strerror(errno));
        }
        /* The .lck marks a PHP-pushed configuration still pending delivery.
         * Consume it only once the poller is connected, so the diff prepared
         * below can be attached to its peer and delivered. If the poller is not
         * connected yet, keep the .lck so the configuration is retried — and
         * recovered through _lck_file_for_poller when the poller finally
         * connects — instead of being silently dropped, which would otherwise
         * leave Broker believing the poller configuration is "lost or unknown".
         */
        ++conf_ready;
        bool peer_connected = _is_engine_peer_connected(poller_id);
        if (peer_connected)
          ++conf_sent;
        else
          pollers_away.push_back(poller_id);
        _prepare_diff_for_poller(poller_id, std::move(state));
        if (peer_connected)
          _remove_lck_file(poller_id);
        else
          _logger->info(
              "Poller {} is not connected yet; keeping its lock file so its "
              "configuration is retried once it connects",
              poller_id);
      } catch (const std::exception& e) {
        ++conf_rejected;
        _logger->error("rejecting invalid configuration for poller {}: {}",
                       poller_id, e.what());
        /* The pushed configuration is structurally invalid
         * (parse/expand/resolve error): it will never become valid on its own,
         * so consume its .lck unconditionally instead of retrying it forever.
         * PHP creates a fresh .lck when it pushes a corrected configuration. */
        std::filesystem::path lck_file =
            cache_config_dir() / fmt::format("{}.lck", poller_id);
        std::error_code lck_ec;
        std::filesystem::remove(lck_file, lck_ec);
        if (lck_ec)
          _logger->warn("Cannot remove lock file '{}' of rejected config: {}",
                        lck_file.string(), lck_ec.message());
      }
    } else
      _logger->error("Cannot create Engine configuration test file '{}': {}",
                     centengine_test.string(), ec.message());
  }

  /* One line for the whole cycle, so that a configuration that went nowhere is
   * visible without cross-reading the log. The pollers named here have their
   * configuration ready on disk and will be served the moment they connect. */
  if (conf_ready || conf_rejected) {
    if (pollers_away.empty())
      _logger->info("Configuration cycle: {} ready, {} sent, {} refused",
                    conf_ready, conf_sent, conf_rejected);
    else
      _logger->info(
          "Configuration cycle: {} ready, {} sent, {} refused, {} poller(s) "
          "not "
          "connected: {}",
          conf_ready, conf_sent, conf_rejected, pollers_away.size(),
          fmt::join(pollers_away, ", "));
  }
}

/**
 * @brief Start watching the Engine configuration directory.
 *
 * Three things are set going: an immediate first cycle, which scans and so
 * picks up whatever was pushed while Broker was down; the wait on the inotify
 * descriptor, which is what detects a push from now on; and the safety timer.
 *
 * Every handler below is bound to _watch_strand, so they are serialized with
 * one another and with the drain barrier the destructor posts: once that
 * barrier runs, no handler is in flight or queued and the watched resources can
 * be destroyed without a race.
 */
void broker_state::_start_watching() {
  _watch_engine_conf_stopped.store(false);
  _post_config_work(true);
  /* Posted rather than called: _inotify_wait_armed belongs to the strand, and
   * this runs on whichever thread applied the configuration or accepted the
   * peer. */
  boost::asio::post(*_watch_strand, [this] { _arm_inotify_wait(); });
  _arm_safety_timer();
}

/**
 * @brief Wait for the next thing inotify has to say, and read it when it comes.
 *
 * The wait is re-armed from its own handler before the coalescing starts, so a
 * burst larger than one buffer is drained right away rather than one buffer per
 * cycle.
 */
void broker_state::_arm_inotify_wait() {
  if (!_cache_config_dir_watcher || _watch_engine_conf_stopped.load() ||
      _inotify_wait_armed)
    return;
  _inotify_wait_armed = true;
  _cache_config_dir_watcher->async_wait_readable(boost::asio::bind_executor(
      *_watch_strand,
      [this, logger = _logger](const boost::system::error_code& ec) {
        _inotify_wait_armed = false;
        if (_watch_engine_conf_stopped.load())
          return;
        if (ec) {
          /* A cancelled wait is the shutdown path, nothing to report. Anything
           * else leaves us with no way to be woken up, so the safety timer is
           * all that is left -- say so. */
          if (ec != boost::asio::error::operation_aborted)
            logger->error(
                "Waiting on the engine configuration directory failed: {}. "
                "Changes will only be seen by the periodic scan",
                ec.message());
          return;
        }
        const bool batch_announced = _read_watch_events();
        _arm_inotify_wait();
        if (batch_announced) {
          /* A batch names its pollers, so there is nothing left to guess and no
           * reason to wait: coalescing exists to find the end of a burst, and
           * this one announced its own. Any burst under way is folded in, since
           * the cycle drains _lck_set whole. */
          _burst_started_at.reset();
          _debounce_timer->cancel();
          _post_config_work(false);
        } else
          _arm_debounce();
        /* Reading may just have found the watch lost, with no way to put it
         * back. From here on nothing will be reported and no event will bring
         * us back, so the net is the only thing left -- and it has to come
         * round quickly. */
        if (_cache_config_dir_watcher->watch_lost())
          _arm_safety_timer();
      }));
}

/**
 * @brief Push back the handling of the events read so far, so that a burst is
 * handled as one batch.
 *
 * Called on every event, and on a poller connecting -- which queues its poller
 * id without any file having changed, so nothing else would wake us up for it.
 */
void broker_state::_arm_debounce() {
  if (!_debounce_timer || _watch_engine_conf_stopped.load())
    return;
  const auto now = std::chrono::steady_clock::now();
  if (!_burst_started_at)
    _burst_started_at = now;
  /* Pushing back on every event is what coalesces the burst; the ceiling is
   * what keeps a burst that never ends from being handled never. */
  _debounce_timer->expires_at(
      std::min(now + debounce_delay, *_burst_started_at + debounce_max_delay));
  _debounce_timer->async_wait(boost::asio::bind_executor(
      *_watch_strand, [this](const boost::system::error_code& ec) {
        /* Re-arming cancels the pending wait, which lands here with
         * operation_aborted: that handler must leave the burst alone, the one
         * that replaced it will handle it. */
        if (ec || _watch_engine_conf_stopped.load())
          return;
        _burst_started_at.reset();
        _post_config_work(false);
      }));
}

/**
 * @brief Hand a configuration cycle over to the worker thread.
 *
 * Called from the strand, and deliberately the only way the work is started:
 * what the strand serializes is the watching, what the worker serializes is the
 * reading of configurations, and neither should wait on the other.
 *
 * @param force_scan Passed through to _check_last_engine_conf().
 */
void broker_state::_post_config_work(bool force_scan) {
  if (!_config_strand || _watch_engine_conf_stopped.load())
    return;
  boost::asio::post(*_config_strand, [this, force_scan] {
    if (_watch_engine_conf_stopped.load())
      return;
    _check_last_engine_conf(force_scan);
  });
}

/**
 * @brief Arm the safety net.
 */
void broker_state::_arm_safety_timer() {
  if (_watch_engine_conf_stopped.load())
    return;
  /* Re-arming an armed timer cancels its pending wait, whose handler then
   * returns on operation_aborted: switching between the two paces costs
   * nothing more than that. */
  const bool blind =
      _cache_config_dir_watcher && _cache_config_dir_watcher->watch_lost();
  if (blind)
    _safety_timer->expires_after(watch_retry_period);
  else
    _safety_timer->expires_after(safety_period);
  _safety_timer->async_wait(boost::asio::bind_executor(
      *_watch_strand, [this](const boost::system::error_code& ec) {
        if (ec || _watch_engine_conf_stopped.load())
          return;
        /* Reading the events too: a watch that was lost is re-established from
         * watch(), and without this nothing would ever call it again. */
        /* The return value is of no use here: a scan is forced either way. */
        _read_watch_events();
        _post_config_work(true);
        _arm_safety_timer();
      }));
}

/**
 * @brief Read what inotify has to report and queue the pollers it names.
 *
 * Reading and handling are two separate steps now: this one runs as soon as the
 * kernel has something, the handling waits for the burst to settle. The poller
 * ids therefore go to _lck_set, which is where a connecting poller queues
 * itself too, rather than to a set local to one cycle.
 */
bool broker_state::_read_watch_events() {
  bool batch_announced = false;
  if (_cache_config_dir_watcher) {
    _logger->debug("Watch engine configuration directory");
    /* Gathered here and handed over in one go below: a deploy-all names one
     * poller per event, and taking the lock for each of them would serialize
     * the whole burst against the handling side for nothing. */
    absl::flat_hash_set<uint32_t> found;
    auto it = _cache_config_dir_watcher->watch();
    /* The watcher reports when the kernel dropped events or when the watch had
     * to be established again. In both cases what happened in the directory is
     * unknown to us and only a scan can recover it. */
    if (_cache_config_dir_watcher->take_rescan_request())
      _scan_requested_by_watcher.store(true);
    for (auto end = _cache_config_dir_watcher->end(); it != end; ++it) {
      _logger->debug("Change detected in '{}'", _cache_config_dir.string());
      auto [event, name] = *it;
      _logger->debug("event: {}, name: '{}'", event, name);
      if (absl::EndsWith(name, ".lck")) {
        if (name == poller_batch_file) {
          /* The file has contents, so only the events that mean it is finished
           * count: its writer closed it, or it was renamed into place. On
           * IN_CREATE it exists but is still empty, and reading it then would
           * see an incomplete batch -- or none at all. */
          if (!(event & (IN_CLOSE_WRITE | IN_MOVED_TO)))
            continue;
          _logger->info("A poller batch was announced in '{}'", name);
          batch_announced = true;
          continue;
        }
        std::string_view prefix(name.data(), name.size() - 4);
        uint32_t poller_id;
        if (absl::SimpleAtoi(prefix, &poller_id)) {
          _logger->info(
              "New Engine configuration available, change in '{}' detected "
              "for poller id '{}'",
              name, poller_id);
          /* The .lck is NOT removed here: it marks a configuration still
           * pending delivery. It is consumed later, in _check_last_engine_conf,
           * only once the poller is connected. Keeping it until then allows the
           * configuration to be recovered (via _lck_file_for_poller) should
           * the poller connect after this detection. */
          found.insert(poller_id);
        } else
          _logger->warn("Change in '{}' detected but poller id not found",
                        _cache_config_dir.string());
      }
    }
    if (!found.empty()) {
      absl::MutexLock lck(&_lck_set_m);
      _lck_set.insert(found.begin(), found.end());
    }
  }
  return batch_announced;
}

/**
 * @brief Prepare the diff between the previous and the new Engine
 * configurations.
 *
 * @param poller_id The poller ID.
 * @param state The new Engine configuration.
 */
bool broker_state::_prepare_diff_for_poller(
    uint64_t poller_id,
    std::unique_ptr<engine::configuration::State>&& state) {
  if (!pollers_config_dir_usable()) {
    _logger->error(
        "No pollers configuration directory: refusing to prepare a diff for "
        "poller {} at a relative path",
        poller_id);
    return false;
  }
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto it = _engine_peers.find(poller_id);
  if (it == _engine_peers.end())
    return false;
  auto& peer = it->second;
  if (peer.engine_conf == state->config_version()) {
    _logger->info(
        "Poller '{}' with id {} already has the latest configuration "
        "(conf: '{}')",
        peer.poller_name, poller_id, peer.engine_conf);
    return false;
  }
  _logger->debug(
      "Poller '{}' with id {} has a new configuration available "
      "(old: '{}', new: '{}')",
      peer.poller_name, poller_id, peer.engine_conf, state->config_version());
  std::filesystem::path previous_prot_conf =
      pollers_config_dir() / fmt::format("{}.prot", poller_id);
  std::fstream f(previous_prot_conf);
  std::unique_ptr<engine::configuration::DiffState> diff_state;
  std::string new_version = state->config_version();
  if (f) {
    /* There is a previous configuration */
    auto previous_state = std::make_unique<engine::configuration::State>();
    previous_state->ParseFromIstream(&f);
    /* If the known configuration by Broker is the same as the one
     * sent by the poller, we can compute the diff. */
    if (previous_state->config_version() == peer.engine_conf) {
      diff_state = std::make_unique<engine::configuration::DiffState>();
      auto previous_indexed_state =
          engine::configuration::indexed_state(std::move(previous_state));
      previous_indexed_state.diff_with_new_config(*state, _logger,
                                                  diff_state.get());
    } else {
      /* Otherwise, we do as if there was no previous configuration,
       * so the diff will be the whole new configuration. */
      _logger->warn(
          "Poller '{}' with id {} has a new configuration available, but "
          "the previous configuration is not the same as the one sent by "
          "the poller (previous: '{}', new: '{}'). The diff will be the "
          "whole new configuration.",
          peer.poller_name, poller_id, peer.engine_conf,
          state->config_version());
      diff_state = std::make_unique<engine::configuration::DiffState>();
      diff_state->set_allocated_state(state.release());
    }
  } else {
    /* No previous configuration */
    diff_state = std::make_unique<engine::configuration::DiffState>();
    diff_state->set_allocated_state(state.release());
  }
  std::filesystem::path diff_prot_conf =
      pollers_config_dir() / fmt::format("diff-{}.prot", poller_id);
  std::ofstream df(diff_prot_conf);
  if (df) {
    diff_state->SerializeToOstream(&df);
    df.close();

    /* The new configuration to send to the poller is
     * new-<poller-ID>.prot. Once sent to it, this file must be renamed
     * into <poller-ID>.prot and the diff file can be removed.
     *
     * The "not sent yet" flag is armed only when the version actually changes.
     * Re-preparing a version already in flight -- two announcements for one
     * export, which PHP is free to make -- would otherwise orphan its
     * acknowledgement: the ack closes the round by looking for a peer marked as
     * served, finds none, and the round never completes. No global diff is then
     * published and nothing reaches the database, even though the poller did
     * answer. */
    if (peer.available_conf != new_version) {
      peer.available_conf = new_version;
      peer.available_conf_sent = false;
    }
    return true;
  }
  _logger->error("Cannot write the diff Engine protobuf configuration '{}': {}",
                 diff_prot_conf.string(), strerror(errno));
  return false;
}

/**
 * @brief Check if the poller engine peer needs an update. This function is
 * called from Broker.
 *
 * @param poller_id The poller ID.
 *
 * @return A boolean indicating if the poller engine peer needs an update.
 */
/**
 * @brief Whether a configuration is prepared for a peer and still owes it a
 * delivery.
 *
 * available_conf is never cleared, so its mere presence proves nothing: what
 * says a delivery is still due is that it differs from what the peer told us it
 * runs. Once the peer acknowledges, engine_conf takes the acknowledged version
 * and the two match again.
 *
 * @param peer The peer to look at. _connected_peers_m must be held.
 */
bool broker_state::_peer_needs_update(const engine_peer& peer) {
  return !peer.available_conf_sent && !peer.available_conf.empty() &&
         peer.available_conf != peer.engine_conf;
}

bool broker_state::engine_peer_needs_update(uint64_t poller_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  _logger->trace("engine_peer_needs_update called for poller id {}", poller_id);
  auto found = _engine_peers.find(poller_id);
  if (found == _engine_peers.end())
    return false;
  const auto& peer = found->second;
  if (_peer_needs_update(peer)) {
    _logger->debug("Available conf: '{}', current conf: '{}' for poller {}",
                   peer.available_conf, peer.engine_conf, poller_id);
    return true;
  }
  return false;
}

/**
 * @brief Acknowledge or not the poller engine peer configuration. When
 * true, the poller is well up to date. When false, broker has a new
 * configuration and the poller did not send any acknowledgement.
 *
 * @param poller_id
 */
void broker_state::acknowledge_engine_peer(uint64_t poller_id) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto found = _engine_peers.find(poller_id);
  if (found != _engine_peers.end())
    found->second.conf_acknowledged = true;
}

/**
 * @brief Called from Broker side when the new configuration has been sent
 * to the poller engine peer.
 *
 * @param poller_id
 */
void broker_state::set_available_conf_sent_to_engine_peer(uint32_t poller_id) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto found = _engine_peers.find(poller_id);
  if (found != _engine_peers.end()) {
    found->second.available_conf_sent = true;
    found->second.conf_acknowledged = false;
    _logger->debug("New configuration sent to poller {}", poller_id);
  } else {
    _logger->info("Unable to send configuration to poller {}: it doesn't exist",
                  poller_id);
  }
}

const std::filesystem::path& broker_state::cache_config_dir() const noexcept {
  return _cache_config_dir;
}

/**
 * @brief Returns true if at least one connected Broker peer has
 * extended_negotiation enabled (i.e. is a BBDO3 central broker or relay).
 */
bool broker_state::broker_peer_supports_extended_negotiation() const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  for (const auto& [key, bp] : _broker_peers) {
    if (bp.extended_negotiation)
      return true;
  }
  return false;
}

/**
 * @brief Returns true when this broker instance is a relay.  A relay is a cbd
 * that participates in BBDO3 centralized configuration (it has Engine peers
 * with extended_negotiation) but does NOT own a pollers_config_dir: it
 * forwards configuration requests upstream to the central Broker.
 */
bool broker_state::is_relay() const noexcept {
  return _pollers_config_dir.empty();
}

/**
 * @brief Atomically drains and returns all pending ConfigRequests.
 * Each entry is {poller_id, config_version_known_by_relay}.
 */
std::vector<std::tuple<uint64_t, std::string, std::string>>
broker_state::pop_pending_config_requests() {
  absl::WriterMutexLock lck(&_connected_peers_m);
  std::vector<std::tuple<uint64_t, std::string, std::string>> result;
  result.reserve(_pending_config_requests.size());
  for (auto& [id, p] : _pending_config_requests) {
    result.emplace_back(id, std::move(p.first), std::move(p.second));
  }
  _pending_config_requests.clear();
  return result;
}

void broker_state::push_pending_diff_state(uint64_t poller_id,
                                           std::shared_ptr<io::data> diff) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  _pending_diff_states[poller_id] = std::move(diff);
}

std::shared_ptr<io::data> broker_state::pop_pending_diff_state_for_engine(
    uint64_t poller_id) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto it = _pending_diff_states.find(poller_id);
  if (it == _pending_diff_states.end())
    return nullptr;
  auto result = std::move(it->second);
  _pending_diff_states.erase(it);
  return result;
}

void broker_state::push_pending_diff_state_ack(std::shared_ptr<io::data> ack) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  _pending_diff_state_acks.push_back(std::move(ack));
}

std::vector<std::shared_ptr<io::data>>
broker_state::pop_pending_diff_state_acks() {
  absl::WriterMutexLock lck(&_connected_peers_m);
  std::vector<std::shared_ptr<io::data>> result;
  std::swap(result, _pending_diff_state_acks);
  return result;
}

/**
 * @brief Queue a pb_notification_execute for delivery to the poller supervising
 * the resource (notification_mode=broker). Called from the notification
 * dispatcher on the multiplexing thread; the event is drained later by that
 * poller's ENGINE-connected stream in read().
 *
 * @param poller_id The id of the poller that must run the notification.
 * @param evt The pb_notification_execute event to deliver.
 */
void broker_state::push_pending_notification_execute(
    uint64_t poller_id,
    std::shared_ptr<io::data> evt) {
  absl::WriterMutexLock lck(&_pending_notif_m);
  _pending_notification_executes[poller_id].push_back(std::move(evt));
}

/**
 * @brief Drain and return the notification executes queued for a poller
 * (notification_mode=broker). Called from that poller's ENGINE-connected
 * stream read().
 *
 * @param poller_id The id of the poller whose queue must be drained.
 *
 * @return The queued events in arrival order, or an empty vector if the poller
 * has nothing pending.
 */
std::vector<std::shared_ptr<io::data>>
broker_state::pop_pending_notification_executes(uint64_t poller_id) {
  absl::WriterMutexLock lck(&_pending_notif_m);
  auto it = _pending_notification_executes.find(poller_id);
  if (it == _pending_notification_executes.end())
    return {};
  auto result = std::move(it->second);
  _pending_notification_executes.erase(it);
  return result;
}

/**
 * @brief Register an engine peer that is reachable via a relay.  Called at
 * the central when it receives a ConfigRequest from relay R for poller N.
 * Creates (or updates) an engine_peer entry in _engine_peers with
 * via_remote = relay_poller_id.
 *
 * @param engine_id       Poller ID of the Engine behind the relay.
 * @param relay_poller_id Poller ID of the relay that sent the ConfigRequest.
 * @param config_version  Config version currently known by the relay (may be
 *                        empty if the relay has no cached config for N).
 */
void broker_state::register_engine_peer_via_relay(
    uint64_t engine_id,
    const std::string& engine_name,
    uint64_t relay_poller_id,
    const std::string& config_version) {
  absl::WriterMutexLock lck(&_connected_peers_m);

  auto it = _engine_peers.find(engine_id);
  if (it != _engine_peers.end()) {
    const uint64_t old_relay = it->second.via_remote;
    if (old_relay != 0 && old_relay != relay_poller_id) {
      _logger->info(
          "Engine {} migrated from relay {} to relay {} — queuing ConfigRevoke "
          "for old relay",
          engine_id, old_relay, relay_poller_id);
      _pending_config_revokes[old_relay].push_back(engine_id);
    } else {
      _logger->info("Updating engine peer {} via relay {}: config version '{}'",
                    engine_id, relay_poller_id, config_version);
    }
    it->second.via_remote = relay_poller_id;
    if (!config_version.empty())
      it->second.engine_conf = config_version;
  } else {
    _logger->info(
        "Registering engine peer {} via relay {} with config version '{}'",
        engine_id, relay_poller_id, config_version);
    _engine_peers[engine_id] = engine_peer{
        engine_id,      engine_name, time(nullptr), true,  "",
        config_version, false,       true,          false, relay_poller_id};
  }
  _last_known_topology[engine_id] = relay_poller_id;
}

/**
 * @brief Load new-{N}.prot and call _prepare_diff_for_poller so that
 * diff-{N}.prot is written (if the relay does not already have the latest
 * version).
 *
 * @return std::nullopt if new-{N}.prot was absent or unreadable (caller should
 *         continue to the next lookup step); true if it was found and a diff
 *         was written; false if it was found but no diff was needed (relay
 *         already has the latest version).
 */
std::optional<bool> broker_state::_prepare_diff_from_new_prot_file(
    uint64_t poller_id) {
  if (!pollers_config_dir_usable()) {
    /* Without a directory, the path below would be relative and read whatever
     * file of that name sits in cbd's working directory -- a configuration
     * belonging to nobody. */
    _logger->debug(
        "No pollers configuration directory: nothing can have been prepared "
        "for poller {}",
        poller_id);
    return std::nullopt;
  }
  const auto new_file =
      pollers_config_dir() / fmt::format("new-{}.prot", poller_id);
  std::ifstream f(new_file);
  if (!f)
    return std::nullopt;
  auto state = std::make_unique<engine::configuration::State>();
  if (!state->ParseFromIstream(&f)) {
    _logger->error("Failed to parse new-{}.prot for poller {}", poller_id,
                   poller_id);
    return std::nullopt;
  }
  f.close();
  return _prepare_diff_for_poller(poller_id, std::move(state));
}

/**
 * @brief Determine what DiffState to send to a relay in response to a
 * ConfigRequest for engine poller @p engine_id, and write diff-{N}.prot if
 * it does not exist yet.
 *
 * Lookup order:
 * 1. diff-{N}.prot already exists → diff_ready
 * 2. new-{N}.prot exists → delegate to _prepare_diff_for_poller via
 *    _prepare_diff_from_new_prot_file; if diff-{N}.prot was written →
 *    diff_ready, otherwise → up_to_date (relay already has latest version)
 * 3. {N}.prot exists (last acknowledged state):
 *    - version matches relay → up_to_date
 *    - relay is behind → write full state to diff-{N}.prot → diff_ready
 * 4. No file → unknown
 *
 * @param engine_id            Poller ID of the Engine behind the relay.
 * @param relay_config_version Config version currently known by the relay.
 */
broker_state::relay_config_response broker_state::prepare_relay_config_response(
    uint64_t engine_id,
    const std::string& relay_config_version) {
  if (!pollers_config_dir_usable()) {
    /* A relay has no pollers configuration directory, and this is the central's
     * side of the exchange: without one there is nothing stored to answer with.
     */
    _logger->error(
        "No pollers configuration directory: cannot answer the configuration "
        "request for poller {}",
        engine_id);
    return relay_config_response::unknown;
  }
  const auto diff_file =
      pollers_config_dir() / fmt::format("diff-{}.prot", engine_id);
  const auto prev_file =
      pollers_config_dir() / fmt::format("{}.prot", engine_id);

  /* 1. */
  if (std::filesystem::exists(diff_file))
    return relay_config_response::diff_ready;

  /* 2. */
  if (auto r = _prepare_diff_from_new_prot_file(engine_id))
    return *r ? relay_config_response::diff_ready
              : relay_config_response::up_to_date;

  /* 3. */
  {
    std::ifstream f(prev_file);
    if (f) {
      auto state = std::make_unique<engine::configuration::State>();
      if (!state->ParseFromIstream(&f)) {
        _logger->error("Failed to parse {}.prot for relay poller {}", engine_id,
                       engine_id);
        return relay_config_response::unknown;
      }
      f.close();
      if (state->config_version() == relay_config_version)
        return relay_config_response::up_to_date;

      /* Relay is behind the last acknowledged state: send it as full state. */
      const std::string version = state->config_version();
      engine::configuration::DiffState diff;
      diff.set_allocated_state(state.release());
      std::ofstream df(diff_file);
      if (!df) {
        _logger->error("Cannot write diff-{}.prot for relay poller {}: {}",
                       engine_id, engine_id, strerror(errno));
        return relay_config_response::unknown;
      }
      diff.SerializeToOstream(&df);
      df.close();
      {
        absl::WriterMutexLock lck(&_connected_peers_m);
        auto it = _engine_peers.find(engine_id);
        /* Same rule as in _prepare_diff_for_poller: arming the "not sent yet"
         * flag for a version already in flight orphans its acknowledgement. */
        if (it != _engine_peers.end() && it->second.available_conf != version) {
          it->second.available_conf = version;
          it->second.available_conf_sent = false;
        }
      }
      return relay_config_response::diff_ready;
    }
  }

  /* 4. */
  _logger->debug("Relay config response for poller {}: unknown (no prot file)",
                 engine_id);
  return relay_config_response::unknown;
}

/**
 * @brief Drain and return all ConfigRevoke poller IDs destined for relay_id.
 * Called from the BROKER-connected stream's read() to send ConfigRevoke
 * messages to the relay when a poller has migrated away from it.
 *
 * @param relay_id The poller ID of the relay to drain revokes for.
 * @return Vector of engine poller IDs that the relay must revoke.
 */
std::vector<uint64_t> broker_state::pop_pending_config_revokes(
    uint64_t relay_id) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto it = _pending_config_revokes.find(relay_id);
  if (it == _pending_config_revokes.end())
    return {};
  std::vector<uint64_t> result = std::move(it->second);
  _pending_config_revokes.erase(it);
  return result;
}

/**
 * @brief Clear all pending state (DiffStates, ConfigRequests) for a poller
 * that the central just revoked.  Called on the relay side when a ConfigRevoke
 * is received, to discard any stale forwarding state for that poller.
 *
 * @param poller_id The poller ID to clear.
 */
void broker_state::clear_pending_for_poller(uint64_t poller_id) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  _pending_diff_states.erase(poller_id);
  _pending_config_requests.erase(poller_id);
}

/**
 * @brief Returns the poller IDs of engine peers that are reachable via the
 * given relay and have a pending configuration update (available_conf differs
 * from engine_conf and has not yet been sent to the relay).
 *
 * Called from the BROKER-connected stream's read() on the central to push
 * new DiffStates to the relay after a PHP configuration push.
 *
 * @param relay_id The poller ID of the relay.
 * @return Vector of engine poller IDs needing an update via this relay.
 */
std::vector<uint64_t> broker_state::engine_peers_via_relay_needing_update(
    uint64_t relay_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  std::vector<uint64_t> result;
  for (const auto& [id, peer] : _engine_peers) {
    if (peer.via_remote != relay_id)
      continue;
    if (peer.available_conf_sent)
      continue;
    if (!peer.available_conf.empty() && peer.available_conf != peer.engine_conf)
      result.push_back(id);
  }
  return result;
}

}  // namespace com::centreon::broker::config::applier
