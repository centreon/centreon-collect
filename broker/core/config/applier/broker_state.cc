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
  /* Here and not at the end of apply(): the output streams are created
   * asynchronously by their failover, and it is their constructors that declare
   * which cache sections they need. Filling the cache from apply() therefore
   * stored nothing at all on a cold start -- measured: "0 hosts and 0 services
   * known" on the first run, 50 and 1000 on the next -- which is the very
   * non-determinism this loading is meant to remove.
   *
   * The readiness barrier releases once every output endpoint has registered as
   * ready, so by here they exist and have spoken. And it runs before the
   * re-injections below, which need the cache to already know the resource. */
  /* Net: on a platform where no poller connects, nothing else would pull the
   * load, and the cache would stay empty for the re-injections below. */
  _ensure_pollers_config_in_cache();

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
      _post_individual_work(true);
      _post_batch_work();
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
  TopologyCache cache = _peers.topology_cache();
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
 * _pollers_config_dir is set. Hands the registry the via_remote hints so that
 * PHP diffs pushed during the outage are routed correctly before the relays
 * reconnect.
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
  _peers.restore_pollers_from_cache(cache);
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
 * @brief Fill the global cache with every poller configuration Broker stores.
 *
 * An object defined on the platform is in the cache, whether its poller is
 * connected or not. That is what lets Broker carry downtimes and notification
 * states for a poller that is momentarily down, and it is why the base marks a
 * stopped poller's hosts `enabled=0` rather than deleting them.
 *
 * The stored `<poller_id>.prot` files are the source: Broker writes them
 * itself, and each is a configuration Engine has acknowledged. Which also makes
 * the configuration part of the on-disk cache file redundant -- and it could be
 * stale, since nothing recalibrated it on an export.
 *
 * Called from _on_barrier_released(), and the moment is not free to choose:
 *  - it must run once every output stream exists, since it is their
 *    constructors that declare the cache sections they need (`unified_sql` asks
 *    for all of them) and `merge()` stores nothing for a section nobody wants.
 *    The streams are created asynchronously, so the end of apply() is too
 *    early;
 *  - it must run before any re-injection of persisted downtimes or
 *    notification states, which need the cache to already know the resource.
 *
 *    @param path The path of a stored configuration file.
 *    @param poller_id The poller whose configuration is stored in @p path.
 *
 *    @return true if the configuration was read and merged, false if it could
 *            not be read (the cache will not know what this poller defines).
 */
bool broker_state::_merge_stored_config_in_cache(
    const std::filesystem::path& path,
    uint64_t poller_id) {
  engine::configuration::State state;
  std::ifstream f(path, std::ios::binary);
  if (!f || !state.ParseFromIstream(&f)) {
    /* One unreadable configuration costs the cache what that poller defines,
     * nothing more. */
    _logger->warn(
        "Cannot read the stored configuration '{}' of poller {}: the cache "
        "will not know what this poller defines",
        path.string(), poller_id);
    return false;
  }
  cache().merge(state);
  return true;
}

/**
 * @brief Load the stored configurations into the cache, once, before anything
 * else touches it.
 *
 * Lazily and not from a fixed point in the startup, because there is no fixed
 * point that works. Two constraints pull in opposite directions:
 *
 *  - too early -- the end of apply() -- and nothing is stored at all: the
 *    output streams are created asynchronously by their failover, and it is
 *    their constructors that declare the cache sections, without which merge()
 *    keeps nothing;
 *  - too late -- the readiness barrier -- and it *overwrites* fresher data: a
 *    poller connects and acknowledges through its BBDO stream well before the
 *    multiplexing engine, hence before the barrier releases. Measured: the
 *    configuration acknowledged at 29.678 was undone by the load at 32.007.
 *
 * So the load is pulled by its first user instead of being pushed at a moment
 * chosen in advance. Whoever is about to read or update the cache calls this
 * first, and by then the sections have been declared -- otherwise there would
 * be nothing to read.
 */
void broker_state::_ensure_pollers_config_in_cache() {
  absl::call_once(_pollers_config_in_cache_once,
                  [this] { load_pollers_config_in_cache(); });
}

/**
 * @brief Carry into the cache the difference poller @p poller_id has just
 * acknowledged.
 *
 * Called from the BBDO stream, right after `new-<ID>.prot` became `<ID>.prot`
 * -- the moment that configuration becomes the reference, because Engine
 * confirmed applying it. It runs before the global diff is published, so
 * whoever handles that diff finds the cache already describing what it refers
 * to.
 *
 * @param poller_id The poller whose configuration was acknowledged.
 */
void broker_state::apply_poller_diff_in_cache(uint64_t poller_id) {
  if (!pollers_config_dir_usable())
    return;
  /* The stored configurations first: a difference only means something applied
   * to the state it was computed against. */
  _ensure_pollers_config_in_cache();

  const auto path =
      pollers_config_dir() / fmt::format("diff-{}.prot", poller_id);
  engine::configuration::DiffState diff;
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    /* No difference was prepared for this poller: it acknowledged a
     * configuration it already ran, and there is nothing to carry over. */
    return;
  }
  if (!diff.ParseFromIstream(&f)) {
    _logger->warn(
        "Cannot read '{}': the cache will not follow what poller {} just "
        "acknowledged",
        path.string(), poller_id);
    return;
  }
  f.close();

  const auto started_at = std::chrono::steady_clock::now();
  cache().apply(diff);
  /* The counts say what the cache holds afterwards, which is the only way to
   * see that the difference had an effect -- a section no module asked for
   * makes apply() store nothing. Guarded because they walk the whole cache: the
   * arguments of a log call are evaluated whether or not the level is on. */
  if (_logger->should_log(spdlog::level::debug))
    _logger->debug(
        "Global cache follows the difference poller {} acknowledged, applied "
        "in "
        "{} ms: {} hosts and {} services known",
        poller_id,
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started_at)
            .count(),
        cache().host_ids().size(), cache().service_ids().size());
}

/**
 * @brief Forget everything stored about a poller removed from the platform.
 *
 * The three files a poller can have in the configuration directory go together:
 * `<ID>.prot` is the configuration it acknowledged, `new-<ID>.prot` one waiting
 * to be acknowledged, `diff-<ID>.prot` a difference waiting to be delivered.
 * None of them means anything once the poller is gone.
 *
 * Leaving them behind is not harmless. The startup load turns every stored
 * configuration into cache entries, so a leftover file would keep describing
 * hosts and services nobody monitors any more -- and a leftover `diff-` is
 * worse, since the global diff is built by merging whatever `diff-*.prot` the
 * directory holds.
 *
 * @param poller_id The poller being removed.
 */
void broker_state::remove_poller_config(uint64_t poller_id) {
  if (!pollers_config_dir_usable())
    return;
  for (const char* pattern : {"{}.prot", "new-{}.prot", "diff-{}.prot"}) {
    const auto path =
        pollers_config_dir() / fmt::format(fmt::runtime(pattern), poller_id);
    std::error_code ec;
    if (std::filesystem::remove(path, ec))
      _logger->info("Removed '{}': poller {} is no longer on the platform",
                    path.string(), poller_id);
    else if (ec)
      _logger->warn("Cannot remove '{}' of removed poller {}: {}",
                    path.string(), poller_id, ec.message());
  }
}

/**
 * @brief Fill the global cache with every poller configuration Broker stores.
 */
void broker_state::load_pollers_config_in_cache() {
  if (!pollers_config_dir_usable() ||
      !std::filesystem::exists(pollers_config_dir()))
    return;

  const auto started_at = std::chrono::steady_clock::now();
  /* The pollers are named, not counted. A configuration is stored for as long
   * as Broker holds its file, and nothing removes that file when a poller
   * leaves the platform by any other route than the RemovePoller command --
   * someone deleting it from the interface without it, say. Naming them is what
   * lets an unexpected id be spotted at a glance instead of by walking the
   * directory. Sorted so two starts read the same way. */
  std::vector<uint64_t> pollers;
  std::error_code ec;
  for (const auto& entry :
       std::filesystem::directory_iterator(pollers_config_dir(), ec)) {
    const uint64_t id = stored_poller_config_id(entry.path());
    if (id == 0)
      continue;
    if (_merge_stored_config_in_cache(entry.path(), id))
      pollers.push_back(id);
  }
  std::sort(pollers.begin(), pollers.end());
  if (ec)
    _logger->warn("Cannot browse the pollers configuration directory '{}': {}",
                  pollers_config_dir().string(), ec.message());
  if (!pollers.empty())
    /* The counts are what the cache actually holds, not what was read: a
     * section no module asked for makes merge() store nothing, and the number
     * of files parsed would say nothing about it. Walking the cache once at
     * startup costs nothing worth guarding against. */
    _logger->info(
        "Global cache filled from the stored configurations of poller(s) {} in "
        "{} ms: {} hosts and {} services known",
        fmt::join(pollers, ", "),
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started_at)
            .count(),
        cache().host_ids().size(), cache().service_ids().size());
}

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
 * poller a given validation is about is told through
 * `foreign_objects::set_to_exclude()`,
 * and that is what keeps its own objects out of the answers.
 *
 * The index mirrors the intra-poller one built by `state_helper::resolve`,
 * hence services only: anomaly detections are not part of the by-name service
 * index there either.
 *
 * @return The states read, and the index borrowing their strings. Both must
 * outlive the validation that uses the index.
 */
engine::configuration::foreign_objects broker_state::load_foreign_objects()
    const {
  const auto started_at = std::chrono::steady_clock::now();
  engine::configuration::foreign_objects retval;
  if (pollers_config_dir().empty() ||
      !std::filesystem::exists(pollers_config_dir()))
    return retval;

  std::error_code ec;
  for (const auto& entry :
       std::filesystem::directory_iterator(pollers_config_dir(), ec)) {
    uint64_t id = stored_poller_config_id(entry.path());
    if (id == 0)
      continue;

    engine::configuration::State state;
    std::ifstream f(entry.path(), std::ios::binary);
    if (!f || !state.ParseFromIstream(&f)) {
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
    for (const auto& h : state.hosts())
      retval.add_host(h.host_name(), id);
    for (const auto& s : state.services())
      retval.add_service(s.host_name(), s.service_description(), id);
    /* `state` dies here: what the index needed of it has been copied into
     * `retval.names`, and keeping the message alive for the rest of the
     * validation would hold the whole configuration for a handful of names. */
  }
  if (ec)
    _logger->warn("Cannot browse the pollers configuration directory '{}': {}",
                  pollers_config_dir().string(), ec.message());
  _logger->debug(
      "Loaded the stored poller configurations for the cross-poller "
      "validation: {} hosts and {} services indexed over {} distinct names in "
      "{} ms",
      retval.host_count(), retval.service_count(), retval.name_count(),
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - started_at)
          .count());
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

  // Logs the skip reason, records that Broker knows this poller's
  // configuration, and signals to the caller that creation should be skipped.
  auto skip = [&](std::string_view reason) {
    _logger->info("Skipping prot file creation for poller {}: {}", poller_id,
                  reason);
    set_broker_knows_poller_conf(poller_id, true);
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
    set_broker_knows_poller_conf(poller_id, true);
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
  std::string engine_conf_known =
      _peers.add_peer(poller_id, poller_name, broker_name, peer_type,
                      extended_negotiation, engine_conf, timezone);

  if (peer_type == common::ENGINE && is_relay() && extended_negotiation) {
    absl::WriterMutexLock lck(&_connected_peers_m);
    _pending_config_requests[poller_id] = {poller_name, engine_conf_known};
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
  /* Same reason as in merge_poller_config_in_cache(): what this publishes ends
   * up merged into the cache, so the stored configurations must already be
   * there. */
  _ensure_pollers_config_in_cache();
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

  /* A poller that was away when its configuration was pushed finds it here.
   * Two files can be waiting for it and they mean different things:
   *
   *  - `<ID>.lck` says PHP announced a configuration that has not been read
   *    yet. Work remains to be done, so the poller goes back into the list and
   *    a full cycle reads its sources.
   *  - `new-<ID>.prot` says a cycle already read them and the resulting state
   *    is on disk, waiting to be acknowledged. Nothing is left but the diff
   *    against what the poller says it runs.
   *
   * The announcement is looked at first, and that order needs no timestamps to
   * justify itself: a cycle removes the `<ID>.lck` as soon as it has written
   * the prepared file, so an announcement that is still there is necessarily
   * about a *later* push than the prepared file next to it. Handing the
   * prepared state over then would deliver the older of the two. */
  _logger->debug("Checking what is waiting for poller {}", poller_id);
  if (uint32_t existing_lck = _lck_file_for_poller(poller_id)) {
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
    poller_conf_lost = false;
  } else if (supports_centralized_conf() &&
             _prepare_diff_from_new_prot_file(poller_id)) {
    /* Reading the sources again would redo the expensive half of a cycle --
     * hash, parse, expand, resolve, and the reload of every stored
     * configuration behind it -- to reach a result already sitting there.
     *
     * There is no announcement left to consume: whichever form it took, it was
     * consumed by the cycle that wrote this file. And the file itself cannot be
     * a leftover of a delivery already made -- an acknowledged one is renamed
     * to `<ID>.prot`. At worst the acknowledgement was lost, and delivering it
     * again is precisely what that calls for. */
    _logger->info(
        "Poller {} has a configuration prepared from when it was away: "
        "handing it over without reading its sources again",
        poller_id);
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
    set_broker_knows_poller_conf(poller_id, false);
    retval = false;
  }
  return retval;
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
  return _peers.poller_timezone(poller_id);
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
  /* No announcement is the ordinary answer now that one does not outlive its
   * reading, so only a real failure to look is worth a line. */
  if (ec && ec != std::errc::no_such_file_or_directory)
    _logger->warn("Cannot check if '{}' is a regular file: {}",
                  lck_file.string(), ec.message());
  return 0;
}

/**
 * @brief Remove the `<poller_id>.lck` announcement, its configuration having
 * been prepared.
 *
 * The announcement says "PHP pushed a configuration for this poller and Broker
 * has not read it yet" -- nothing more. What says a delivery is still pending
 * is `new-<poller_id>.prot`, in the directory Broker owns, and it says it
 * whether the poller is connected or not.
 *
 * So the two hand over to one another: the announcement goes as soon as the
 * prepared file is there, and never before, or a failure between the two would
 * leave the push with no trace at all. Which also means PHP no longer waits for
 * a delivery it cannot influence -- the file disappearing tells it Broker has
 * taken the configuration over, which is all PHP needs to push the next one.
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
 * @brief Remove the `pollers.lck` announcement, the batch it named having been
 * handled.
 *
 * Removed at the end of the cycle rather than as soon as it is read, for the
 * same reason as a `<poller_id>.lck`: until the configurations it names have
 * been prepared, this file is the only trace that they were announced at all.
 * PHP waits for it to disappear before announcing another batch, so the wait
 * now covers the reading of the configurations too.
 */
void broker_state::_remove_poller_batch() {
  if (_cache_config_dir.empty())
    return;
  const std::filesystem::path batch_file(_cache_config_dir / poller_batch_file);
  std::error_code ec;
  std::filesystem::remove(batch_file, ec);
  if (ec)
    _logger->error(
        "Cannot remove the poller batch file '{}': {}. The batch would be "
        "handled again on the next cycle",
        batch_file.string(), ec.message());
  else
    _logger->debug("Removed the poller batch file '{}' after processing",
                   batch_file.string());
}

/**
 * @brief Read the poller batch file.
 *
 * Unlike a `<poller_id>.lck`, this file names several pollers at once, so it
 * cannot be handed over to `new-<poller_id>.prot` one poller at a time: it is
 * removed once the whole batch has been through the cycle, by
 * _remove_poller_batch().
 *
 * @return The poller IDs the batch names, empty when there is no batch.
 */
absl::flat_hash_set<uint32_t> broker_state::_read_poller_batch() {
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
  _peers.set_poller_engine_conf(poller_id, engine_conf);
}

/**
 * @brief Record whether Broker holds the content of this poller's
 * configuration. When set to false, Broker sends a DiffState{unknown=true} to
 * Engine at the next negotiation, asking it to send its full configuration
 * back.
 *
 * @param poller_id The poller ID.
 * @param known false when Broker has nothing stored for this poller.
 */
void broker_state::set_broker_knows_poller_conf(uint64_t poller_id,
                                                bool known) {
  _peers.set_broker_knows_poller_conf(poller_id, known);
}

/**
 * @brief Check if the Engine configuration for the given poller is known
 * to Broker. Returns false if the configuration has been marked unknown,
 * for example after losing its .prot file with no .lck file available.
 *
 * @param poller_id The poller ID.
 * @return true if the configuration is known, false if unknown.
 */
bool broker_state::broker_knows_poller_conf(uint64_t poller_id) const {
  return _peers.broker_knows_poller_conf(poller_id);
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
  if (_peers.remove_peer(poller_id, poller_name, broker_name))
    _logger->info("Peer poller: '{}' - broker: '{}' with id {} disconnected",
                  poller_name, broker_name, poller_id);
  else
    _logger->warn(
        "Peer poller: '{}' - broker: '{}' with id {} not found in connected "
        "peers",
        poller_name, broker_name, poller_id);
}

/**
 * @brief Check if a poller has a live link to this Broker, directly or through
 * a relay.
 *
 * An entry restored from topology.cache is not a connection: it only says
 * which relay to talk to should that poller show up, and carries no connection
 * date. It is excluded here.
 *
 * @param poller_id The poller to check.
 *
 * @return true when the poller is connected.
 */
bool broker_state::is_poller_connected(uint64_t poller_id) const {
  return _peers.is_poller_connected(poller_id);
}

/**
 * @brief Check if this poller's Engine is running, as told by the last
 * pb_instance received for it.
 *
 * @param poller_id The poller to check.
 *
 * @return true when the Engine of this poller is running.
 */
bool broker_state::is_poller_running(uint64_t poller_id) const {
  return _peers.is_poller_running(poller_id);
}

/**
 * @brief Record what the last pb_instance said about this poller's Engine.
 *
 * A peer that is not connected cannot be running: an entry restored from
 * topology.cache is a routing hint, and marking it running would make it look
 * like a live poller to everything downstream. Such an event is dropped and
 * signalled -- in a sane run it cannot happen, since a peer is registered at
 * negotiation, well before its Engine announces itself.
 *
 * @param poller_id The poller ID.
 * @param running What the event said.
 */
void broker_state::set_instance_running(uint64_t poller_id,
                                        bool running) noexcept {
  _peers.set_instance_running(poller_id, running);
}

/**
 * @brief Get the Engine peers currently connected, directly or through a
 * relay.
 *
 * @return A vector of engine_peers, each one with a connection date.
 */
std::vector<broker_state::engine_peer> broker_state::connected_pollers() const {
  return _peers.connected_pollers();
}

/**
 * @brief Get the list of connected peers.
 *
 * @return A vector of peers.
 */
std::vector<broker_state::peer> broker_state::connected_peers() const {
  return _peers.connected_peers();
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
bool broker_state::try_close_conf_round() {
  return _peers.try_close_conf_round();
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
absl::flat_hash_set<uint32_t> broker_state::_scan_for_announcements(
    const absl::flat_hash_set<uint32_t>& already_queued) {
  absl::flat_hash_set<uint32_t> found;
  if (_cache_config_dir.empty())
    return found;

  std::error_code scan_ec;
  std::filesystem::directory_iterator dir_it(_cache_config_dir, scan_ec);
  if (scan_ec) {
    /* A scan that failed answered nothing, so an explicit request stands and is
     * honoured on the next occasion rather than being dropped here. */
    _logger->warn("Error scanning engine config directory '{}': {}",
                  _cache_config_dir.string(), scan_ec.message());
    return found;
  }
  _scan_requested_by_watcher.store(false);
  for (const auto& entry : dir_it) {
    const auto& p = entry.path();
    if (p.extension() != ".lck")
      continue;
    uint32_t poller_id;
    if (absl::SimpleAtoi(p.stem().string(), &poller_id)) {
      if (already_queued.contains(poller_id))
        continue;  // inotify reported it, so it is not orphan
      _logger->info(
          "Found orphan lock file '{}' not reported by inotify — scheduling "
          "configuration check for poller {}",
          p.string(), poller_id);
      found.insert(poller_id);
    }
  }
  return found;
}

/**
 * @brief Hand the pollers announced one at a time over to a cycle.
 *
 * This is the `<poller_id>.lck` path. Such announcements arrive one per poller
 * with nothing saying where the export ends, so they are accumulated in
 * _lck_set and it takes a delay -- the debounce -- to decide the burst is over.
 * Whatever has accumulated by then is one lot.
 *
 * @param force_scan Scan the directory whatever the events said, and take
 * whatever it finds into the lot. Set by the first cycle and by the safety net,
 * the two moments when events are not what we are relying on.
 */
void broker_state::_post_individual_work(bool force_scan) {
  if (!_config_strand || _watch_engine_conf_stopped.load())
    return;
  boost::asio::post(*_config_strand, [this, force_scan] {
    if (_watch_engine_conf_stopped.load())
      return;
    absl::flat_hash_set<uint32_t> pollers_set;
    {
      absl::MutexLock lck(&_lck_set_m);
      pollers_set.swap(_lck_set);
    }

    /* Fallback: scan the directory for any .lck files that inotify did not
     * report, so that no configuration update is permanently lost.
     *
     * This is a safety net, not the detection mechanism -- inotify is. It runs
     * when the watcher says the events were incomplete (the kernel dropped
     * some, or the watch was lost), and otherwise on a slow period, so that
     * whatever neither inotify nor the watcher's own reporting covers -- a
     * directory on a filesystem where inotify does not work, say -- cannot stay
     * hidden forever. Scanning on every cycle instead would mean walking the
     * directory for nothing. */
    const bool requested = _scan_requested_by_watcher.load();
    if (force_scan || requested) {
      if (requested)
        _logger->info(
            "Scanning the engine configuration directory '{}': the changes "
            "reported by inotify were incomplete",
            _cache_config_dir.string());
      pollers_set.merge(_scan_for_announcements(pollers_set));
    }

    if (!pollers_set.empty())
      _run_config_cycle(pollers_set);
  });
}

/**
 * @brief Hand a batch announced by `pollers.lck` over to a cycle.
 *
 * This is the other path, and it owes nothing to the first one. A batch names
 * the pollers it covers, so there is no end of burst to guess and nothing to
 * wait for: the lot is complete the moment the file is read, whatever time PHP
 * took to generate it -- which is exactly what a delay cannot promise.
 *
 * The file is read here, from the worker, rather than at the moment of the
 * event: the safety net posts this too, so a batch the events did not report is
 * still picked up. Reading an absent batch costs one stat.
 */
void broker_state::_post_batch_work() {
  if (!_config_strand || _watch_engine_conf_stopped.load())
    return;
  boost::asio::post(*_config_strand, [this] {
    if (_watch_engine_conf_stopped.load())
      return;
    absl::flat_hash_set<uint32_t> batch = _read_poller_batch();
    if (batch.empty())
      return;
    _run_config_cycle(batch);
    /* Every configuration the batch named has been through the cycle --
     * prepared or refused -- so the announcement has nothing left to say. */
    _remove_poller_batch();
  });
}

/**
 * @brief Read, validate and store the configuration a poller was pushed.
 *
 * The two halves of this are told apart on purpose, because a failure does not
 * mean the same thing in each:
 *
 *  - **reading and validating** work on the pushed content and on nothing else,
 *    so a failure there is a property of that content. It will not become valid
 *    on its own, and retrying would reject it forever: the configuration is
 *    refused and its announcement consumed.
 *  - **storing** is about the disk, or about a directory that is not
 *    configured. It says nothing of the configuration, so the announcement is
 *    kept -- it is then the only trace left of the push, and what makes the
 * next cycle read it again.
 *
 * @param poller_id The poller whose pushed configuration is read.
 * @param foreign The configurations stored for the other pollers, so that an
 * object living elsewhere is not taken for an undefined one.
 * @return The stored state, ready to be handed over; nullptr when nothing was
 * stored. @p refused then says which of the two halves failed, so the caller
 * can count it and know whether an announcement is still waiting.
 */
std::unique_ptr<engine::configuration::State> broker_state::_read_poller_conf(
    uint32_t poller_id,
    const engine::configuration::foreign_objects& foreign,
    bool& refused) {
  refused = false;
  const std::filesystem::path dir =
      cache_config_dir() / fmt::to_string(poller_id);

  std::error_code ec;
  const std::string version = common::hash_directory(dir, ec);
  if (ec) {
    _logger->error(
        "Cannot compute the Engine configuration version for poller '{}': {}",
        poller_id, ec.message());
    return nullptr;
  }

  const std::filesystem::path centengine_test = dir / "centengine.test";
  engine::configuration::parser::build_test_file(centengine_test,
                                                 dir / "centengine.cfg", ec);
  if (ec) {
    /* Nothing was even read, so this says nothing about the configuration
     * itself: the announcement stays and the next cycle tries again. */
    _logger->error("Cannot create Engine configuration test file '{}': {}",
                   centengine_test.string(), ec.message());
    return nullptr;
  }

  auto state = std::make_unique<engine::configuration::State>();
  try {
    engine::configuration::state_helper state_hlp(state.get());
    engine::configuration::error_cnt err;
    engine::configuration::parser p;
    p.parse(centengine_test, state.get(), err);
    state->set_config_version(version);
    state->set_poller_id(poller_id);
    state_hlp.expand(err);
    /* Being the central, we can do better than a poller alone: the
     * configurations stored for the other pollers tell whether an object this
     * one references is genuinely undefined or merely lives elsewhere. */
    state_hlp.resolve(err, _logger, foreign);
    if (err.config_errors)
      throw com::centreon::exceptions::msg_fmt(
          "configuration for poller {} (version '{}') has {} error(s); "
          "refusing to push it to the poller",
          poller_id, version, err.config_errors);
  } catch (const std::exception& e) {
    refused = true;
    _logger->error("rejecting invalid configuration for poller {}: {}",
                   poller_id, e.what());
    /* The announcement is consumed even though nothing was prepared -- the one
     * case where the two markers do not hand over to one another, because there
     * is nothing to hand over to. Retrying it forever would reject it forever.
     * PHP creates a fresh announcement when it pushes a corrected
     * configuration. */
    _remove_lck_file(poller_id);
    return nullptr;
  }

  if (!_store_poller_conf(poller_id, *state, version))
    return nullptr;
  return state;
}

/**
 * @brief Write the validated configuration of a poller to `new-<ID>.prot`, and
 * consume the announcement that brought it.
 *
 * Every path out of here that is not a success keeps the announcement, and
 * hands nothing to the poller: a state that is not on disk cannot be
 * acknowledged, since the acknowledgement renames the very file that was not
 * written.
 *
 * @return True when the state is on disk and the announcement consumed.
 */
bool broker_state::_store_poller_conf(uint32_t poller_id,
                                      const engine::configuration::State& state,
                                      const std::string& version) {
  if (!pollers_config_dir_usable()) {
    _logger->error(
        "No pollers configuration directory: refusing to write the "
        "configuration of poller {} to a relative path. Keeping its "
        "announcement",
        poller_id);
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(pollers_config_dir())) {
    std::filesystem::create_directories(pollers_config_dir(), ec);
    if (ec) {
      _logger->error(
          "Cannot create pollers configuration directory '{}': {}. Keeping the "
          "announcement of poller {}",
          pollers_config_dir().string(), ec.message(), poller_id);
      return false;
    }
  }
  const std::filesystem::path last_prot_conf =
      pollers_config_dir() / fmt::format("new-{}.prot", poller_id);
  std::ofstream f(last_prot_conf);
  if (!f) {
    _logger->error(
        "Cannot write the new Engine protobuf configuration '{}': {}. Keeping "
        "the announcement of poller {}",
        last_prot_conf.string(), strerror(errno), poller_id);
    return false;
  }
  if (!state.SerializeToOstream(&f)) {
    _logger->error(
        "Cannot serialize the new Engine configuration of poller {} to '{}'. "
        "Keeping its announcement",
        poller_id, last_prot_conf.string());
    return false;
  }
  f.close();
  _logger->info("New Engine configuration for poller {} stored, version '{}'",
                poller_id, version);
  /* The announcement has been read and what it announced is now on disk, so it
   * has nothing left to say: from here on it is `new-<ID>.prot` that says a
   * delivery is pending, whether the poller is connected or not. Removed only
   * once that file exists -- the two markers hand over to one another, and at
   * no instant is there neither. */
  _remove_lck_file(poller_id);
  return true;
}

/**
 * @brief Validate, store and hand over the configuration of every poller of a
 * lot.
 *
 * What a lot is made of, and when it is complete, is the business of whoever
 * posts it -- the two announcement shapes answer that differently. From here on
 * they are the same work.
 *
 * @param pollers_set The pollers to handle. Never empty.
 */
void broker_state::_run_config_cycle(
    const absl::flat_hash_set<uint32_t>& pollers_set) {
  _logger->trace("Handling the configuration of {} poller(s)",
                 pollers_set.size());
  /* What the cycle did, told at the end in one line. Broker cannot know how
   * many pollers the platform has, but it does know how many configurations it
   * just prepared -- and which of them it could not hand over. */
  uint32_t conf_ready = 0;
  uint32_t conf_sent = 0;
  uint32_t conf_rejected = 0;
  std::vector<uint32_t> pollers_away;

  /* Read once for the whole lot: on a deploy-all, pollers_set holds every
   * poller and re-reading the store for each of them would parse it as many
   * times as it has entries. Each iteration only moves the exclusion onto the
   * poller it validates. The read is deferred to the first poller of the lot,
   * so a cycle woken up for nothing pays nothing. */
  std::optional<engine::configuration::foreign_objects> foreign;
  for (uint32_t poller_id : pollers_set) {
    _logger->debug(
        "Checking if there is a new Engine configuration for poller {}",
        poller_id);
    if (!foreign)
      foreign = load_foreign_objects();
    foreign->set_to_exclude(poller_id);

    bool refused = false;
    auto state = _read_poller_conf(poller_id, *foreign, refused);
    if (!state) {
      conf_rejected += refused;
      continue;
    }

    ++conf_ready;
    if (is_poller_connected(poller_id))
      ++conf_sent;
    else
      pollers_away.push_back(poller_id);
    _prepare_diff_for_poller(poller_id, std::move(state));
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
  /* Both paths, so that whatever was pushed while Broker was down is picked up
   * whichever shape it took. */
  _post_individual_work(true);
  _post_batch_work();
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
        const watch_report report = _read_watch_events();
        _arm_inotify_wait();
        /* The two announcement shapes are handled apart, and neither disturbs
         * the other. A batch has nothing to wait for and goes straight to a
         * cycle; individual announcements start or extend a burst whose end
         * only a delay can find. When both arrive at once -- which the contract
         * does not foresee, one PHP writing one shape or the other -- the burst
         * keeps its own pace instead of being cut short by the batch. */
        if (report.batch_announced)
          _post_batch_work();
        if (report.pollers_announced || report.rescan_requested)
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
        _post_individual_work(false);
      }));
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
        /* What the events said is of no use here: both paths are driven anyway,
         * the scan for what inotify did not report about individual
         * announcements, and a read for a batch file it did not report either.
         */
        _read_watch_events();
        _post_individual_work(true);
        _post_batch_work();
        _arm_safety_timer();
      }));
}

/**
 * @brief Read what inotify has to report and route it.
 *
 * Reading and handling are two separate steps: this one runs as soon as the
 * kernel has something, the handling waits for the burst to settle. The poller
 * ids of individual announcements therefore go to _lck_set, which is where a
 * connecting poller queues itself too, rather than to a set local to one cycle.
 *
 * A batch carries no poller id here -- its ids are in the file, read by the
 * worker -- so all this reports about it is that one was announced. Which is
 * also why the two are told apart here and not later: from this point on they
 * travel by different paths.
 */
broker_state::watch_report broker_state::_read_watch_events() {
  watch_report report;
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
    if (_cache_config_dir_watcher->take_rescan_request()) {
      _scan_requested_by_watcher.store(true);
      /* This names no poller -- the kernel dropped events, or the watch had to
       * be established again -- so without saying it here nothing would post
       * the work and the scan would wait for the safety net. */
      report.rescan_requested = true;
    }
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
          report.batch_announced = true;
          continue;
        }
        std::string_view prefix(name.data(), name.size() - 4);
        uint32_t poller_id;
        if (absl::SimpleAtoi(prefix, &poller_id)) {
          _logger->info(
              "New Engine configuration available, change in '{}' detected "
              "for poller id '{}'",
              name, poller_id);
          /* The .lck is not removed here: reading an event is not handling
           * what it announced. It is removed by the cycle, once the
           * configuration it announced has been prepared. */
          found.insert(poller_id);
        } else
          _logger->warn("Change in '{}' detected but poller id not found",
                        _cache_config_dir.string());
      }
    }
    if (!found.empty()) {
      report.pollers_announced = true;
      absl::MutexLock lck(&_lck_set_m);
      _lck_set.insert(found.begin(), found.end());
    }
  }
  return report;
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
  /* Held from the version read below to the arming at the end: two
   * preparations of the same poller must not interleave. */
  auto peer = _peers.lock_engine_peer(poller_id);
  /* A poller that is not there gets nothing prepared, and that has to hold for
   * a poller behind a relay exactly as it holds for a direct one. A directly
   * connected poller that is away has no entry at all, so this returns here and
   * its `new-<ID>.prot` simply waits for it. An entry restored from
   * topology.cache would otherwise pass: a diff would be written and
   * `available_conf` armed for a peer nobody can reach, which holds
   * try_close_conf_round() open -- and with it the global diff of
   * every other poller of the round -- until that relay comes back. */
  if (!peer || !peer->connected())
    return false;
  if (peer->engine_conf == state->config_version()) {
    _logger->info(
        "Poller '{}' with id {} already has the latest configuration "
        "(conf: '{}')",
        peer->poller_name, poller_id, peer->engine_conf);
    return false;
  }
  _logger->debug(
      "Poller '{}' with id {} has a new configuration available "
      "(old: '{}', new: '{}')",
      peer->poller_name, poller_id, peer->engine_conf, state->config_version());
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
    if (previous_state->config_version() == peer->engine_conf) {
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
          peer->poller_name, poller_id, peer->engine_conf,
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
    if (peer->available_conf != new_version) {
      peer->available_conf = new_version;
      peer->available_conf_sent = false;
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

bool broker_state::poller_needs_update(uint64_t poller_id) const {
  return _peers.poller_needs_update(poller_id);
}

/**
 * @brief Acknowledge or not the poller engine peer configuration. When
 * true, the poller is well up to date. When false, broker has a new
 * configuration and the poller did not send any acknowledgement.
 *
 * @param poller_id
 */
void broker_state::set_poller_conf_acknowledged(uint64_t poller_id) {
  _peers.set_poller_conf_acknowledged(poller_id);
}

/**
 * @brief Called from Broker side when the new configuration has been sent
 * to the poller engine peer.
 *
 * @param poller_id
 */
void broker_state::set_poller_conf_sent(uint32_t poller_id) {
  _peers.set_poller_conf_sent(poller_id);
}

const std::filesystem::path& broker_state::cache_config_dir() const noexcept {
  return _cache_config_dir;
}

/**
 * @brief Returns true if at least one connected Broker peer has
 * extended_negotiation enabled (i.e. is a BBDO3 central broker or relay).
 */
bool broker_state::broker_peer_supports_extended_negotiation() const {
  return _peers.broker_peer_supports_extended_negotiation();
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
 * @brief Register a poller that is reachable via a relay.  Called at the
 * central when it receives a ConfigRequest from relay R for poller N.
 * Records the peer in the registry with via_remote = relay_poller_id, and
 * queues a ConfigRevoke for the relay it left when it migrated.
 *
 * @param poller_id       Poller ID of the poller behind the relay.
 * @param relay_poller_id Poller ID of the relay that sent the ConfigRequest.
 * @param config_version  Config version currently known by the relay (may be
 *                        empty if the relay has no cached config for N).
 */
void broker_state::register_poller_via_relay(
    uint64_t poller_id,
    const std::string& poller_name,
    uint64_t relay_poller_id,
    const std::string& config_version) {
  uint64_t migrated_from = _peers.register_poller_via_relay(
      poller_id, poller_name, relay_poller_id, config_version);
  if (migrated_from) {
    _logger->info(
        "Engine {} migrated from relay {} to relay {} - queuing ConfigRevoke "
        "for old relay",
        poller_id, migrated_from, relay_poller_id);
    absl::WriterMutexLock lck(&_connected_peers_m);
    _pending_config_revokes[migrated_from].push_back(poller_id);
  } else {
    _logger->info("Engine peer {} reachable via relay {}: config version '{}'",
                  poller_id, relay_poller_id, config_version);
  }
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
 * A pending `<ID>.lck` is looked at first, for the same reason the direct path
 * does it in _feed_cache_and_wake_up_resources(): it means PHP announced a
 * configuration that no cycle has read yet, so whatever sits on disk next to it
 * is older. Unlike the direct path we do not withhold that older state -- a
 * ConfigRequest is a request/response and leaving it unanswered would stall the
 * handshake -- we only make sure the cycle runs now instead of waiting for the
 * five-minute safety net. The newer configuration follows through
 * pollers_via_relay_needing_update() within the debounce.
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

  /* 0. */
  if (uint32_t existing_lck = _lck_file_for_poller(engine_id)) {
    {
      absl::MutexLock lck(&_lck_set_m);
      _lck_set.insert(existing_lck);
    }
    /* Nothing in the directory changed, so inotify has nothing to say and would
     * never wake the watcher up for this poller. */
    if (_watch_strand)
      boost::asio::post(*_watch_strand, [this] { _arm_debounce(); });
    _logger->info(
        "Poller {} behind a relay has a configuration announced but not read "
        "yet: waking the configuration cycle up for it",
        engine_id);
  }

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
        auto peer = _peers.lock_engine_peer(engine_id);
        /* Same rule as in _prepare_diff_for_poller: arming the "not sent yet"
         * flag for a version already in flight orphans its acknowledgement. */
        if (peer && peer->available_conf != version) {
          peer->available_conf = version;
          peer->available_conf_sent = false;
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
std::vector<uint64_t> broker_state::pollers_via_relay_needing_update(
    uint64_t relay_id) const {
  return _peers.pollers_via_relay_needing_update(relay_id);
}

}  // namespace com::centreon::broker::config::applier
