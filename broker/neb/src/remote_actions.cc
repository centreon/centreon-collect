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

#include "com/centreon/broker/neb/remote_actions.hh"

#include "broker/core/bbdo/internal.hh"
#include "broker/core/bbdo/remote.hh"
#include "com/centreon/common/file_system.hh"
#include "com/centreon/common/pool.hh"
#include "com/centreon/common/process/process.hh"
#include "com/centreon/common/utf8.hh"
#include "common/log_v2/log_v2.hh"

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/strings/match.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_split.h>
#include <absl/synchronization/mutex.h>
#include <fnmatch.h>
#include <openssl/evp.h>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <boost/asio/steady_timer.hpp>
#include <deque>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace com::centreon::broker;
using com::centreon::common::log_v2::log_v2;
namespace fs = std::filesystem;

namespace {

std::shared_ptr<spdlog::logger> logger() {
  return log_v2::instance().get(log_v2::NEB);
}

/* Engine functions, installed by Engine (see engine_bridge). */
absl::Mutex g_bridge_m;
neb::remote_actions::engine_bridge g_bridge ABSL_GUARDED_BY(g_bridge_m);

namespace bridge {
void run_external_command(const std::string& command,
                          std::function<void(int, const std::string&)>&& done) {
  decltype(g_bridge.run_external_command) f;
  {
    absl::MutexLock lck(&g_bridge_m);
    f = g_bridge.run_external_command;
  }
  if (f)
    f(command, std::move(done));
  else
    done(-1, "engine bridge not installed");
}

std::string main_config_file() {
  absl::MutexLock lck(&g_bridge_m);
  return g_bridge.main_config_file ? g_bridge.main_config_file() : "";
}

void request_reload() {
  absl::MutexLock lck(&g_bridge_m);
  if (g_bridge.request_reload)
    g_bridge.request_reload();
}

void request_restart() {
  absl::MutexLock lck(&g_bridge_m);
  if (g_bridge.request_restart)
    g_bridge.request_restart();
}
}  // namespace bridge

/* Request ids already handled, to drop a replayed request. */
constexpr size_t max_known_requests = 1000;
absl::Mutex g_known_m;
std::deque<std::string> g_known_order ABSL_GUARDED_BY(g_known_m);
absl::flat_hash_set<std::string> g_known ABSL_GUARDED_BY(g_known_m);

/* Directory of the cbmod configuration file (deployment target "broker"). */
absl::Mutex g_conf_m;
fs::path g_broker_dir ABSL_GUARDED_BY(g_conf_m);

/**
 * @brief Remember a request id.
 *
 * @return false if it was already known.
 */
bool remember(const std::string& request_id) {
  absl::MutexLock lck(&g_known_m);
  if (!g_known.insert(request_id).second)
    return false;
  g_known_order.push_back(request_id);
  if (g_known_order.size() > max_known_requests) {
    g_known.erase(g_known_order.front());
    g_known_order.pop_front();
  }
  return true;
}

void reply(const std::shared_ptr<bbdo::remote_channel>& channel,
           const std::string& request_id,
           int32_t index,
           RemoteStatus status,
           const std::string& message,
           const std::string& config_version = {}) {
  auto r = std::make_shared<bbdo::pb_remote_result>();
  auto& obj = r->mut_obj();
  obj.set_request_id(request_id);
  obj.set_index(index);
  obj.set_status(status);
  obj.set_message(message);
  obj.set_config_version(config_version);
  if (!channel->push(r, message.size()))
    SPDLOG_LOGGER_WARN(logger(),
                       "remote actions: cannot answer request '{}', stream "
                       "closed",
                       request_id);
}

/**
 * @brief Queue the commands on the Engine main loop; each one answers with
 * its own RemoteResult.
 */
void handle_command(const RemoteCommand& cmd,
                    const std::shared_ptr<bbdo::remote_channel>& channel) {
  const std::string& request_id = cmd.request_id();
  if (!remember(request_id)) {
    SPDLOG_LOGGER_INFO(logger(),
                       "remote actions: request '{}' already handled, ignored",
                       request_id);
    return;
  }
  time_t deadline = cmd.deadline();
  if (deadline && time(nullptr) > static_cast<time_t>(deadline)) {
    reply(channel, request_id, -1, REMOTE_REJECTED,
          "request expired before reaching the poller");
    return;
  }
  SPDLOG_LOGGER_INFO(logger(), "remote actions: request '{}' with {} commands",
                     request_id, cmd.commands_size());
  for (int i = 0; i < cmd.commands_size(); ++i) {
    bridge::run_external_command(
        cmd.commands(i),
        [channel, request_id, i](int code, const std::string& error) {
          /* On the Engine main loop, after execution. */
          if (code == 0)
            reply(channel, request_id, i, REMOTE_APPLIED, "");
          else
            reply(channel, request_id, i, REMOTE_REJECTED, error);
        });
  }
}

/********************************************************************
 * Configuration deployments
 ********************************************************************/

std::string to_hex(const unsigned char* d, unsigned int size) {
  static const char digits[] = "0123456789abcdef";
  std::string retval;
  retval.reserve(size * 2);
  for (unsigned int i = 0; i < size; ++i) {
    retval.push_back(digits[d[i] >> 4]);
    retval.push_back(digits[d[i] & 0xf]);
  }
  return retval;
}

/**
 * @brief A relative path without "..", without leading '/', not empty.
 */
bool valid_relative_path(const std::string& p) {
  if (p.empty() || p[0] == '/' || p.find('\0') != std::string::npos)
    return false;
  for (std::string_view part : absl::StrSplit(p, '/'))
    if (part.empty() || part == "." || part == "..")
      return false;
  return true;
}

struct staged_file {
  std::string target;
  std::string path;
  std::string sha256;
  fs::path staged;
  fs::path final_path;
  EVP_MD_CTX* md = nullptr;
  bool complete = false;
  ~staged_file() {
    if (md)
      EVP_MD_CTX_free(md);
  }
};

struct deployment {
  std::string id;
  std::shared_ptr<bbdo::remote_channel> channel;
  /* staging directory per target directory: <dir>/.staging-<id> */
  std::map<fs::path, fs::path> staging_dirs;
  std::map<uint32_t, std::unique_ptr<staged_file>> files;
  std::string error;
};

absl::Mutex g_deploy_m;
absl::flat_hash_map<std::string, std::unique_ptr<deployment>> g_deployments
    ABSL_GUARDED_BY(g_deploy_m);

/* Deployment waiting for the outcome of the reload it asked for. */
struct pending_reload {
  std::string id;
  std::shared_ptr<bbdo::remote_channel> channel;
  std::string config_version;
};
std::unique_ptr<pending_reload> g_pending_reload ABSL_GUARDED_BY(g_deploy_m);

/* A cbmod configuration was installed by a deployment without a reload
 * (files first, action 'none', then a reload in a later deployment): the next
 * reload must restart Engine instead. Cleared by the restart itself (process
 * memory). */
bool g_broker_restart_pending ABSL_GUARDED_BY(g_deploy_m) = false;

fs::path engine_dir() {
  return fs::path(bridge::main_config_file()).parent_path();
}

fs::path target_dir(const std::string& target) {
  if (target == "engine")
    return engine_dir();
  if (target == "broker") {
    absl::MutexLock lck(&g_conf_m);
    return g_broker_dir;
  }
  return {};
}

std::string staging_name(const std::string& id) {
  std::string retval{".staging-"};
  for (char c : id)
    retval.push_back(std::isalnum(static_cast<unsigned char>(c)) ? c : '_');
  return retval;
}

void remove_staging(const deployment& d) {
  std::error_code ec;
  for (auto& p : d.staging_dirs)
    fs::remove_all(p.second, ec);
}

std::string engine_config_version() {
  std::error_code ec;
  return com::centreon::common::hash_directory(engine_dir(), ec);
}

/**
 * @brief Append a chunk to its staged file, check the SHA-256 on the last
 * chunk. The first error is kept and reported at commit.
 */
void handle_chunk(const ConfigChunk& c,
                  const std::shared_ptr<bbdo::remote_channel>& channel) {
  absl::MutexLock lck(&g_deploy_m);
  auto& dptr = g_deployments[c.request_id()];
  if (!dptr) {
    dptr = std::make_unique<deployment>();
    dptr->id = c.request_id();
    SPDLOG_LOGGER_INFO(logger(), "remote actions: deployment '{}' started",
                       c.request_id());
  }
  deployment& d = *dptr;
  d.channel = channel;
  if (!d.error.empty())
    return;

  auto& fptr = d.files[c.index()];
  if (!fptr) {
    fs::path dir = target_dir(c.target());
    if (dir.empty() || !valid_relative_path(c.path())) {
      d.error = fmt::format("invalid target '{}' or path '{}'", c.target(),
                            c.path());
      return;
    }
    auto& staging = d.staging_dirs[dir];
    if (staging.empty())
      staging = dir / staging_name(d.id);
    fptr = std::make_unique<staged_file>();
    fptr->target = c.target();
    fptr->path = c.path();
    fptr->sha256 = c.sha256();
    fptr->staged = staging / c.path();
    fptr->final_path = dir / c.path();
    fptr->md = EVP_MD_CTX_new();
    EVP_DigestInit_ex(fptr->md, EVP_sha256(), nullptr);
    std::error_code ec;
    fs::create_directories(fptr->staged.parent_path(), ec);
    std::ofstream truncate(fptr->staged, std::ios::binary | std::ios::trunc);
    if (ec || !truncate) {
      d.error = fmt::format("cannot stage '{}': {}", fptr->staged.string(),
                            ec ? ec.message() : strerror(errno));
      return;
    }
  }
  staged_file& f = *fptr;
  if (f.complete) {
    d.error = fmt::format("file '{}' received twice", f.path);
    return;
  }
  {
    std::ofstream out(f.staged, std::ios::binary | std::ios::app);
    out.write(c.data().data(), c.data().size());
    if (!out) {
      d.error = fmt::format("cannot write '{}': {}", f.staged.string(),
                            strerror(errno));
      return;
    }
  }
  EVP_DigestUpdate(f.md, c.data().data(), c.data().size());
  if (c.last()) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int size = 0;
    EVP_DigestFinal_ex(f.md, hash, &size);
    std::string computed = to_hex(hash, size);
    if (computed != f.sha256) {
      d.error = fmt::format("SHA-256 mismatch on '{}': expected {}, got {}",
                            f.path, f.sha256, computed);
      return;
    }
    f.complete = true;
    /* Progress, keeps the central timeout alive. */
    reply(channel, d.id, c.index(), REMOTE_DELIVERED, f.path);
  }
}

/**
 * @brief Tell if a staged broker file is new or differs from the installed
 * one: cbmod reads its configuration once, so Engine must restart.
 */
bool broker_config_changed(const deployment& d) {
  for (auto& p : d.files) {
    const staged_file& f = *p.second;
    /* Only cbmod configurations: on the central, the broker directory also
     * holds the cbd ones, reloaded by cbd itself. */
    if (f.target != "broker" ||
        !absl::EndsWith(f.final_path.filename().string(), "module.json"))
      continue;
    std::error_code ec;
    if (!fs::exists(f.final_path, ec))
      return true;
    std::string content;
    try {
      content = com::centreon::common::read_file_content(f.final_path);
    } catch (const std::exception&) {
      return true;
    }
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int size = 0;
    EVP_Digest(content.data(), content.size(), hash, &size, EVP_sha256(),
               nullptr);
    if (to_hex(hash, size) != f.sha256)
      return true;
  }
  return false;
}

/**
 * @brief Move every staged file in place, keeping <file>.prev. On a failure,
 * the files already moved are restored.
 *
 * @return An empty string on success, the error otherwise.
 */
std::string swap_files(deployment& d) {
  std::vector<staged_file*> done;
  std::error_code ec;
  for (auto& p : d.files) {
    staged_file& f = *p.second;
    fs::create_directories(f.final_path.parent_path(), ec);
    fs::path prev = f.final_path;
    prev += ".prev";
    if (fs::exists(f.final_path, ec))
      fs::rename(f.final_path, prev, ec);
    if (!ec)
      fs::rename(f.staged, f.final_path, ec);
    if (ec) {
      std::string error = fmt::format("cannot install '{}': {}",
                                      f.final_path.string(), ec.message());
      for (auto* r : done) {
        fs::path rprev = r->final_path;
        rprev += ".prev";
        std::error_code ec2;
        if (fs::exists(rprev, ec2))
          fs::rename(rprev, r->final_path, ec2);
      }
      return error;
    }
    done.push_back(&f);
    SPDLOG_LOGGER_DEBUG(logger(), "remote actions: installed '{}'",
                        f.final_path.string());
  }
  return {};
}

/**
 * @brief Restart Engine a little later, so that the APPLIED answer is
 * written on the stream before Engine stops.
 */
void delayed_restart() {
  auto timer = std::make_shared<boost::asio::steady_timer>(
      com::centreon::common::pool::io_context());
  timer->expires_after(std::chrono::seconds(2));
  timer->async_wait([timer](const boost::system::error_code& err) {
    if (!err)
      bridge::request_restart();
  });
}

/********************************************************************
 * Plugin install (plugins.json committed by a deployment)
 ********************************************************************/

/* RemoteResult index of a plugin install outcome. */
constexpr int32_t plugins_result_index = -2;
constexpr std::chrono::seconds plugins_install_timeout{1800};

struct plugins_install {
  std::string id;
  std::shared_ptr<bbdo::remote_channel> channel;
  std::string plugins_json;
};

/* One install at a time; a newer trigger replaces the queued one. */
absl::Mutex g_plugins_m;
bool g_plugins_running ABSL_GUARDED_BY(g_plugins_m) = false;
std::unique_ptr<plugins_install> g_plugins_queued ABSL_GUARDED_BY(g_plugins_m);

/**
 * @brief The engine plugins.json of the deployment when it is new or differs
 * from the installed one, an empty path otherwise.
 */
fs::path plugins_json_changed(const deployment& d) {
  for (auto& p : d.files) {
    const staged_file& f = *p.second;
    if (f.target != "engine" || f.path != "plugins.json")
      continue;
    std::error_code ec;
    if (!fs::exists(f.final_path, ec))
      return f.final_path;
    std::string content;
    try {
      content = com::centreon::common::read_file_content(f.final_path);
    } catch (const std::exception&) {
      return f.final_path;
    }
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int size = 0;
    EVP_Digest(content.data(), content.size(), hash, &size, EVP_sha256(),
               nullptr);
    if (to_hex(hash, size) != f.sha256)
      return f.final_path;
  }
  return {};
}

void plugins_reply(const plugins_install& p,
                   RemoteStatus status,
                   const std::string& message,
                   const std::vector<PluginPackage>& packages = {}) {
  auto r = std::make_shared<bbdo::pb_remote_result>();
  auto& obj = r->mut_obj();
  obj.set_request_id(p.id);
  obj.set_index(plugins_result_index);
  obj.set_status(status);
  obj.set_message(com::centreon::common::check_string_utf8(message));
  for (auto& pkg : packages)
    *obj.add_plugins() = pkg;
  if (!p.channel->push(r, message.size()))
    SPDLOG_LOGGER_WARN(logger(),
                       "remote actions: cannot report the plugin install of "
                       "deployment '{}', stream closed",
                       p.id);
}

/**
 * @brief Parse the install script output: one line per package,
 * "installed <pkg>", "uptodate <pkg>" or "failed <pkg> <message>".
 */
std::vector<PluginPackage> parse_plugins_output(const std::string& out) {
  std::vector<PluginPackage> retval;
  for (std::string_view line : absl::StrSplit(out, '\n')) {
    std::vector<std::string_view> parts =
        absl::StrSplit(line, absl::MaxSplits(' ', 2), absl::SkipEmpty());
    if (parts.size() < 2)
      continue;
    PluginPackage pkg;
    pkg.set_name(com::centreon::common::check_string_utf8(parts[1]));
    if (parts[0] == "installed") {
      pkg.set_status("installed");
    } else if (parts[0] == "uptodate") {
      pkg.set_status("installed");
      pkg.set_message("already installed");
    } else if (parts[0] == "failed") {
      pkg.set_status("failed");
      pkg.set_message(com::centreon::common::check_string_utf8(
          parts.size() > 2 ? parts[2] : "install failed"));
    } else {
      continue;
    }
    retval.push_back(std::move(pkg));
  }
  return retval;
}

void plugins_start(std::unique_ptr<plugins_install> p);

/**
 * @brief Start the queued install, if any, else mark the installer idle.
 */
void plugins_next() {
  std::unique_ptr<plugins_install> next;
  {
    absl::MutexLock lck(&g_plugins_m);
    std::swap(next, g_plugins_queued);
    if (!next)
      g_plugins_running = false;
  }
  if (next)
    plugins_start(std::move(next));
}

/**
 * @brief Run the install command on the shared pool and report its outcome.
 * Called with g_plugins_running set.
 */
void plugins_start(std::unique_ptr<plugins_install> p) {
  const char* cmd = std::getenv("CENTREON_PLUGINS_INSTALL");
  if (!cmd || !*cmd) {
    plugins_next();
    return;
  }
  std::shared_ptr<plugins_install> sp(std::move(p));
  SPDLOG_LOGGER_INFO(logger(),
                     "remote actions: deployment '{}': plugins.json changed, "
                     "running '{} {}'",
                     sp->id, cmd, sp->plugins_json);
  plugins_reply(*sp, REMOTE_DELIVERED, "plugin install started");
  try {
    auto args = std::make_shared<com::centreon::common::process_args>(
        std::string_view(cmd), std::vector<std::string>{sp->plugins_json});
    auto proc = std::make_shared<com::centreon::common::process<true>>(
        com::centreon::common::pool::io_context_ptr(), logger(), args, true,
        false, nullptr);
    proc->start_process(
        [sp](const com::centreon::common::process<true>&, int exit_code,
             com::centreon::common::e_exit_status exit_status,
             const std::string& out, const std::string& err) {
          try {
            if (exit_status == com::centreon::common::e_exit_status::timeout) {
              SPDLOG_LOGGER_ERROR(logger(),
                                  "remote actions: deployment '{}': plugin "
                                  "install timeout",
                                  sp->id);
              plugins_reply(*sp, REMOTE_REJECTED, "timeout");
            } else {
              std::vector<PluginPackage> packages = parse_plugins_output(out);
              if (packages.empty() &&
                  (exit_status != com::centreon::common::e_exit_status::normal ||
                   exit_code != 0)) {
                std::string msg = fmt::format(
                    "install command failed (exit {}): {}", exit_code,
                    err.substr(0, 512));
                SPDLOG_LOGGER_ERROR(logger(),
                                    "remote actions: deployment '{}': {}",
                                    sp->id, msg);
                plugins_reply(*sp, REMOTE_REJECTED, msg);
              } else {
                size_t failed = 0;
                for (auto& pkg : packages)
                  if (pkg.status() == "failed")
                    ++failed;
                SPDLOG_LOGGER_INFO(logger(),
                                   "remote actions: deployment '{}': plugin "
                                   "install done, {} packages, {} failed",
                                   sp->id, packages.size(), failed);
                plugins_reply(*sp, REMOTE_APPLIED,
                              fmt::format("{} packages, {} failed",
                                          packages.size(), failed),
                              packages);
              }
            }
          } catch (const std::exception& e) {
            SPDLOG_LOGGER_ERROR(logger(),
                                "remote actions: plugin install report: {}",
                                e.what());
          }
          plugins_next();
        },
        plugins_install_timeout);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(logger(),
                        "remote actions: deployment '{}': cannot run the "
                        "plugin install: {}",
                        sp->id, e.what());
    plugins_reply(*sp, REMOTE_REJECTED,
                  fmt::format("cannot run the install command: {}", e.what()));
    plugins_next();
  }
}

/**
 * @brief Install the plugins of a committed plugins.json, or queue it behind
 * the running install (the last queued one wins).
 */
void plugins_trigger(const std::string& id,
                     const std::shared_ptr<bbdo::remote_channel>& channel,
                     const fs::path& plugins_json) {
  const char* cmd = std::getenv("CENTREON_PLUGINS_INSTALL");
  if (!cmd || !*cmd) {
    SPDLOG_LOGGER_DEBUG(logger(),
                        "remote actions: deployment '{}': plugins.json "
                        "changed, CENTREON_PLUGINS_INSTALL not set, no install",
                        id);
    return;
  }
  auto p = std::make_unique<plugins_install>(
      plugins_install{id, channel, plugins_json.string()});
  {
    absl::MutexLock lck(&g_plugins_m);
    if (g_plugins_running) {
      if (g_plugins_queued)
        SPDLOG_LOGGER_INFO(logger(),
                           "remote actions: plugin install of deployment '{}' "
                           "superseded by deployment '{}'",
                           g_plugins_queued->id, id);
      else
        SPDLOG_LOGGER_INFO(logger(),
                           "remote actions: deployment '{}': plugin install "
                           "queued behind the running one",
                           id);
      g_plugins_queued = std::move(p);
      return;
    }
    g_plugins_running = true;
  }
  plugins_start(std::move(p));
}

void handle_commit(const ConfigCommit& c,
                   const std::shared_ptr<bbdo::remote_channel>& channel) {
  std::unique_ptr<deployment> d;
  {
    absl::MutexLock lck(&g_deploy_m);
    auto it = g_deployments.find(c.request_id());
    if (it != g_deployments.end()) {
      d = std::move(it->second);
      g_deployments.erase(it);
    }
  }
  if (!d) {
    d = std::make_unique<deployment>();
    d->id = c.request_id();
  }
  if (!remember("deploy:" + d->id)) {
    SPDLOG_LOGGER_INFO(logger(),
                       "remote actions: deployment '{}' already handled",
                       d->id);
    remove_staging(*d);
    return;
  }

  std::string error = d->error;
  if (error.empty()) {
    size_t complete = 0;
    for (auto& f : d->files)
      if (f.second->complete)
        ++complete;
    if (complete != c.files_count() || d->files.size() != c.files_count())
      error = fmt::format("{} files expected, {} received complete",
                          c.files_count(), complete);
  }
  bool broker_changed = error.empty() && broker_config_changed(*d);
  fs::path plugins_json =
      error.empty() ? plugins_json_changed(*d) : fs::path();
  if (error.empty())
    error = swap_files(*d);
  remove_staging(*d);

  if (!error.empty()) {
    SPDLOG_LOGGER_ERROR(logger(), "remote actions: deployment '{}' rejected: {}",
                        d->id, error);
    reply(channel, d->id, -1, REMOTE_REJECTED, error);
    return;
  }

  std::string version = engine_config_version();
  SPDLOG_LOGGER_INFO(logger(),
                     "remote actions: deployment '{}': {} files installed, "
                     "action '{}', config version {}",
                     d->id, d->files.size(), c.action(), version);
  if (c.action() == "reload" || c.action() == "restart") {
    absl::MutexLock lck(&g_deploy_m);
    broker_changed = broker_changed || g_broker_restart_pending;
    g_broker_restart_pending = false;
  } else if (broker_changed) {
    absl::MutexLock lck(&g_deploy_m);
    g_broker_restart_pending = true;
    SPDLOG_LOGGER_INFO(logger(),
                       "remote actions: deployment '{}': broker configuration "
                       "changed, the next reload restarts Engine",
                       d->id);
  }
  if (c.action() == "reload" && broker_changed) {
    reply(channel, d->id, -1, REMOTE_APPLIED,
          "restart requested (broker configuration changed)", version);
    delayed_restart();
  } else if (c.action() == "reload") {
    {
      absl::MutexLock lck(&g_deploy_m);
      if (g_pending_reload)
        reply(g_pending_reload->channel, g_pending_reload->id, -1,
              REMOTE_REJECTED, "superseded by a newer deployment");
      g_pending_reload = std::make_unique<pending_reload>(
          pending_reload{d->id, channel, version});
    }
    reply(channel, d->id, -1, REMOTE_DELIVERED, "reload requested");
    bridge::request_reload();
  } else if (c.action() == "restart") {
    reply(channel, d->id, -1, REMOTE_APPLIED, "restart requested", version);
    delayed_restart();
  } else {
    reply(channel, d->id, -1, REMOTE_APPLIED, "", version);
  }
  if (!plugins_json.empty())
    plugins_trigger(d->id, channel, plugins_json);
}

/********************************************************************
 * Remote exec (contracts.md §12): run an allow-listed program, return its
 * output. argv, stdout and stderr are never logged.
 ********************************************************************/

constexpr uint32_t exec_default_timeout_s = 300;
constexpr uint32_t exec_max_timeout_s = 3600;
constexpr size_t exec_chunk_size = 256u << 10;
constexpr size_t exec_default_cap = 8u << 20;
constexpr size_t exec_max_cap = 16u << 20;
constexpr size_t exec_stderr_tail = 4096;
constexpr size_t exec_default_parallel = 4;
constexpr size_t exec_queue_size = 64;
constexpr std::chrono::seconds exec_heartbeat{15};

/* The process class logs its arguments (timeout) and its output (trace):
 * remote exec processes get a logger without sink. */
std::shared_ptr<spdlog::logger> exec_process_logger() {
  static std::shared_ptr<spdlog::logger> l = [] {
    auto r = std::make_shared<spdlog::logger>("remote_exec_process");
    r->set_level(spdlog::level::off);
    return r;
  }();
  return l;
}

size_t env_size(const char* name, size_t def, size_t max) {
  const char* v = std::getenv(name);
  size_t r;
  if (!v || !absl::SimpleAtoi(v, &r) || r == 0)
    return def;
  return std::min(r, max);
}

/* Allow-list: wildcard patterns on the real path of argv[0], read from
 * *.json files of CENTREON_EXEC_ALLOWLIST_DIR. Empty means deny all. */
absl::Mutex g_exec_allow_m;
std::vector<std::string> g_exec_allow ABSL_GUARDED_BY(g_exec_allow_m);
std::string g_exec_allow_error ABSL_GUARDED_BY(g_exec_allow_m);

void load_exec_allowlist() {
  const char* env = std::getenv("CENTREON_EXEC_ALLOWLIST_DIR");
  fs::path dir(env && *env ? env : "/etc/centreon-engine-exec");
  std::vector<std::string> patterns;
  std::string error;
  std::error_code ec;
  if (fs::is_directory(dir, ec)) {
    std::vector<fs::path> files;
    for (auto& e : fs::directory_iterator(dir, ec))
      if (e.path().extension() == ".json")
        files.push_back(e.path());
    std::sort(files.begin(), files.end());
    for (auto& f : files) {
      try {
        nlohmann::json j =
            nlohmann::json::parse(com::centreon::common::read_file_content(f));
        const auto& w = j.at("exec").at("wildcard");
        for (const auto& p : w) {
          std::string s = p.get<std::string>();
          if (s.empty() || s[0] != '/')
            throw std::invalid_argument("patterns must be absolute paths");
          patterns.push_back(std::move(s));
        }
      } catch (const std::exception& e) {
        error = fmt::format("{}: {}", f.string(), e.what());
        patterns.clear();
        break;
      }
    }
  }
  absl::MutexLock lck(&g_exec_allow_m);
  g_exec_allow = std::move(patterns);
  g_exec_allow_error = error;
  if (!error.empty())
    SPDLOG_LOGGER_ERROR(logger(),
                        "remote exec: invalid allow-list, exec disabled: {}",
                        error);
  else
    SPDLOG_LOGGER_INFO(logger(),
                       "remote exec: allow-list {}: {} patterns{}",
                       dir.string(), g_exec_allow.size(),
                       g_exec_allow.empty() ? ", exec disabled" : "");
}

/**
 * @brief Check argv[0] against the allow-list.
 *
 * @param argv0 The requested program.
 * @param real Filled with its real path when allowed.
 * @return An empty string when allowed, the refusal reason otherwise.
 */
std::string exec_allowed(const std::string& argv0, std::string& real) {
  if (argv0.empty() || argv0[0] != '/')
    return "invalid argv: argv[0] must be an absolute path";
  std::error_code ec;
  fs::path p = fs::canonical(argv0, ec);
  if (ec)
    return "program not found";
  real = p.string();
  absl::MutexLock lck(&g_exec_allow_m);
  if (!g_exec_allow_error.empty())
    return "allow-list invalid";
  if (g_exec_allow.empty())
    return "remote exec disabled: empty allow-list";
  for (const auto& w : g_exec_allow)
    if (!fnmatch(w.c_str(), real.c_str(), FNM_PATHNAME | FNM_PERIOD))
      return {};
  return "program not in the allow-list";
}

struct exec_run : public std::enable_shared_from_this<exec_run> {
  std::string id;
  std::shared_ptr<bbdo::remote_channel> channel;
  std::string program;  // real path of argv[0]
  std::vector<std::string> args;
  std::string name;  // basename, the only part that is logged
  uint32_t timeout_s = exec_default_timeout_s;
  size_t cap = exec_default_cap;

  absl::Mutex m;
  uint32_t seq ABSL_GUARDED_BY(m) = 0;
  std::string pending ABSL_GUARDED_BY(m);
  size_t kept ABSL_GUARDED_BY(m) = 0;
  uint64_t total ABSL_GUARDED_BY(m) = 0;
  bool truncated ABSL_GUARDED_BY(m) = false;
  std::string err_tail ABSL_GUARDED_BY(m);
  std::chrono::steady_clock::time_point last_sent ABSL_GUARDED_BY(m);
  bool finished ABSL_GUARDED_BY(m) = false;
  bool timed_out ABSL_GUARDED_BY(m) = false;
  std::chrono::steady_clock::time_point started;
  /* Both timers are created before the process starts and only used with m
   * held. The deadline kills the whole process group (setpgid), so that a
   * grandchild holding the pipes cannot delay the answer. */
  std::unique_ptr<boost::asio::steady_timer> heartbeat ABSL_GUARDED_BY(m);
  std::unique_ptr<boost::asio::steady_timer> deadline ABSL_GUARDED_BY(m);
  int pid ABSL_GUARDED_BY(m) = 0;

  /* Push one RemoteExecResult. Called with m held. */
  void send(ExecState state,
            std::string&& chunk,
            const std::function<void(RemoteExecResult&)>& fill = {})
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(m) {
    auto r = std::make_shared<bbdo::pb_remote_exec_result>();
    auto& obj = r->mut_obj();
    obj.set_request_id(id);
    obj.set_seq(++seq);
    obj.set_state(state);
    size_t size = chunk.size();
    if (size)
      obj.set_stdout_chunk(std::move(chunk));
    if (fill)
      fill(obj);
    last_sent = std::chrono::steady_clock::now();
    if (!channel->push(r, size + 64))
      SPDLOG_LOGGER_DEBUG(logger(),
                          "remote exec: request '{}': stream closed, result "
                          "dropped",
                          id);
  }

  /* Send the complete 256 KiB chunks, or everything with flush_all. */
  void flush(bool flush_all) ABSL_EXCLUSIVE_LOCKS_REQUIRED(m) {
    while (pending.size() >= exec_chunk_size ||
           (flush_all && !pending.empty())) {
      size_t n = std::min(pending.size(), exec_chunk_size);
      std::string c = pending.substr(0, n);
      pending.erase(0, n);
      send(EXEC_RUNNING, std::move(c));
    }
  }

  void on_stdout(std::string_view data) {
    absl::MutexLock lck(&m);
    if (finished)
      return;
    total += data.size();
    if (kept < cap) {
      size_t n = std::min(data.size(), cap - kept);
      pending.append(data.data(), n);
      kept += n;
    }
    if (total > cap)
      truncated = true;
    flush(false);
  }

  void on_stderr(std::string_view data) {
    absl::MutexLock lck(&m);
    err_tail.append(data.data(), data.size());
    if (err_tail.size() > exec_stderr_tail)
      err_tail.erase(0, err_tail.size() - exec_stderr_tail);
  }

  /* Called with m held. */
  void arm_heartbeat() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m) {
    heartbeat->expires_after(exec_heartbeat);
    heartbeat->async_wait(
        [me = shared_from_this()](const boost::system::error_code& err) {
          if (err)
            return;
          absl::MutexLock lck(&me->m);
          if (me->finished)
            return;
          if (std::chrono::steady_clock::now() - me->last_sent >=
              exec_heartbeat)
            me->send(EXEC_RUNNING, {});
          me->arm_heartbeat();
        });
  }

  /* Called with m held. */
  void arm_deadline() ABSL_EXCLUSIVE_LOCKS_REQUIRED(m);
};

void exec_finish(const std::shared_ptr<exec_run>& run,
                 ExecState state,
                 int exit_code,
                 const std::string& message);

void exec_run::arm_deadline() {
  deadline->expires_after(std::chrono::seconds(timeout_s));
  deadline->async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        if (err)
          return;
        {
          absl::MutexLock lck(&me->m);
          if (me->finished)
            return;
          me->timed_out = true;
          if (me->pid > 0)
            ::killpg(me->pid, SIGKILL);
        }
        exec_finish(me, EXEC_TIMEOUT, -1,
                    fmt::format("killed after {} s", me->timeout_s));
      });
}

absl::Mutex g_exec_m;
size_t g_exec_running ABSL_GUARDED_BY(g_exec_m) = 0;
std::deque<std::shared_ptr<exec_run>> g_exec_queue ABSL_GUARDED_BY(g_exec_m);

void exec_start(const std::shared_ptr<exec_run>& run);

/* A run ended: start the next queued one, if any. */
void exec_next() {
  std::shared_ptr<exec_run> next;
  {
    absl::MutexLock lck(&g_exec_m);
    if (!g_exec_queue.empty()) {
      next = std::move(g_exec_queue.front());
      g_exec_queue.pop_front();
    } else if (g_exec_running) {
      --g_exec_running;
    }
  }
  if (next)
    exec_start(next);
}

void exec_finish(const std::shared_ptr<exec_run>& run,
                 ExecState state,
                 int exit_code,
                 const std::string& message) {
  uint64_t total;
  bool truncated;
  {
    absl::MutexLock lck(&run->m);
    if (run->finished)
      return;
    run->flush(true);
    run->finished = true;
    total = run->total;
    truncated = run->truncated;
    std::string tail = std::move(run->err_tail);
    run->send(state, {}, [&](RemoteExecResult& o) {
      if (state == EXEC_DONE)
        o.set_exit_code(exit_code);
      o.set_stderr_tail(std::move(tail));
      o.set_truncated(truncated);
      o.set_output_bytes(total);
      o.set_message(message);
    });
    if (run->heartbeat)
      run->heartbeat->cancel();
    if (run->deadline)
      run->deadline->cancel();
  }
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                     std::chrono::steady_clock::now() - run->started)
                     .count();
  SPDLOG_LOGGER_INFO(logger(),
                     "remote exec: request '{}': {} {} exit {}, {} bytes{} in "
                     "{} s",
                     run->id, run->name,
                     state == EXEC_DONE      ? "done"
                     : state == EXEC_TIMEOUT ? "timeout"
                                             : "failed",
                     exit_code, total, truncated ? ", truncated" : "",
                     elapsed);
  exec_next();
}

/* Start a run; the caller has taken a slot. */
void exec_start(const std::shared_ptr<exec_run>& run) {
  run->started = std::chrono::steady_clock::now();
  {
    absl::MutexLock lck(&run->m);
    run->heartbeat = std::make_unique<boost::asio::steady_timer>(
        com::centreon::common::pool::io_context());
    run->deadline = std::make_unique<boost::asio::steady_timer>(
        com::centreon::common::pool::io_context());
    run->send(EXEC_RUNNING, {});
    run->arm_heartbeat();
    run->arm_deadline();
  }
  SPDLOG_LOGGER_INFO(logger(),
                     "remote exec: request '{}': {}, {} args, started",
                     run->id, run->name, run->args.size() + 1);
  try {
    auto args = std::make_shared<com::centreon::common::process_args>(
        run->program, std::vector<std::string>(run->args));
    auto proc = std::make_shared<com::centreon::common::process<true>>(
        com::centreon::common::pool::io_context_ptr(), exec_process_logger(),
        args, true, false, nullptr);
    proc->start_process(
        [run](const com::centreon::common::process<true>&, int exit_code,
              com::centreon::common::e_exit_status exit_status,
              const std::string&, const std::string&) {
          try {
            bool timed_out;
            {
              absl::MutexLock lck(&run->m);
              timed_out = run->timed_out;
            }
            if (timed_out ||
                exit_status == com::centreon::common::e_exit_status::timeout)
              exec_finish(run, EXEC_TIMEOUT, exit_code,
                          fmt::format("killed after {} s", run->timeout_s));
            else
              exec_finish(run, EXEC_DONE, exit_code,
                          exit_status ==
                                  com::centreon::common::e_exit_status::crash
                              ? "killed by a signal"
                              : "");
          } catch (const std::exception& e) {
            SPDLOG_LOGGER_ERROR(logger(), "remote exec: request '{}': {}",
                                run->id, e.what());
          }
        },
        [run](const boost::system::error_code& err, std::string_view data) {
          if (!err && !data.empty())
            run->on_stdout(data);
        },
        [run](const boost::system::error_code& err, std::string_view data) {
          if (!err && !data.empty())
            run->on_stderr(data);
        },
        /* Backstop only: the deadline timer answers first. */
        std::chrono::seconds(run->timeout_s + 30));
    absl::MutexLock lck(&run->m);
    if (!run->finished)
      run->pid = proc->get_pid();
  } catch (const std::exception&) {
    /* The exception text may quote the command line: not logged. */
    exec_finish(run, EXEC_FAILED, -1, "cannot start the program");
  }
}

void exec_reject(const std::string& id,
                 const std::shared_ptr<bbdo::remote_channel>& channel,
                 const std::string& name,
                 const std::string& reason) {
  SPDLOG_LOGGER_INFO(logger(), "remote exec: request '{}': {} rejected: {}",
                     id, name, reason);
  auto r = std::make_shared<bbdo::pb_remote_exec_result>();
  auto& obj = r->mut_obj();
  obj.set_request_id(id);
  obj.set_seq(1);
  obj.set_state(EXEC_REJECTED);
  obj.set_message(reason);
  channel->push(r, reason.size());
}

void handle_exec(const RemoteExec& e,
                 const std::shared_ptr<bbdo::remote_channel>& channel) {
  const std::string& id = e.request_id();
  if (!remember("exec:" + id)) {
    SPDLOG_LOGGER_INFO(logger(),
                       "remote exec: request '{}' already handled, ignored", id);
    return;
  }
  std::string name =
      e.argv_size() ? fs::path(e.argv(0)).filename().string() : "-";
  time_t deadline = e.deadline();
  if (deadline && time(nullptr) > deadline) {
    exec_reject(id, channel, name, "request expired before reaching the poller");
    return;
  }
  if (e.argv_size() == 0) {
    exec_reject(id, channel, name, "invalid argv: empty");
    return;
  }
  if (e.timeout_s() > exec_max_timeout_s) {
    exec_reject(id, channel, name, "invalid timeout");
    return;
  }
  auto run = std::make_shared<exec_run>();
  std::string reason = exec_allowed(e.argv(0), run->program);
  if (!reason.empty()) {
    exec_reject(id, channel, name, reason);
    return;
  }
  run->id = id;
  run->channel = channel;
  run->name = name;
  run->args.assign(e.argv().begin() + 1, e.argv().end());
  run->timeout_s = e.timeout_s() ? e.timeout_s() : exec_default_timeout_s;
  size_t cap = env_size("CENTREON_EXEC_MAX_OUTPUT", exec_default_cap,
                        exec_max_cap);
  run->cap = e.max_output_bytes() ? std::min<size_t>(e.max_output_bytes(), cap)
                                  : cap;
  size_t parallel = env_size("CENTREON_EXEC_MAX_PARALLEL",
                             exec_default_parallel, 64);
  {
    absl::MutexLock lck(&g_exec_m);
    if (g_exec_running >= parallel) {
      if (g_exec_queue.size() >= exec_queue_size) {
        reason = "queue full";
      } else {
        g_exec_queue.push_back(run);
        SPDLOG_LOGGER_INFO(logger(), "remote exec: request '{}': {} queued",
                           id, name);
        return;
      }
    } else {
      ++g_exec_running;
    }
  }
  if (!reason.empty()) {
    exec_reject(id, channel, name, reason);
    return;
  }
  exec_start(run);
}

/**
 * @brief Action handler registered in the BBDO remote registry. Called on the
 * thread owning the stream: it only does local file work, never waits on the
 * Engine loop.
 */
void on_action(const std::shared_ptr<io::data>& d,
               const std::shared_ptr<bbdo::remote_channel>& channel) {
  try {
    switch (d->type()) {
      case bbdo::pb_remote_command::static_type():
        handle_command(
            std::static_pointer_cast<bbdo::pb_remote_command>(d)->obj(),
            channel);
        break;
      case bbdo::pb_config_chunk::static_type():
        handle_chunk(std::static_pointer_cast<bbdo::pb_config_chunk>(d)->obj(),
                     channel);
        break;
      case bbdo::pb_config_commit::static_type():
        handle_commit(
            std::static_pointer_cast<bbdo::pb_config_commit>(d)->obj(),
            channel);
        break;
      case bbdo::pb_remote_exec::static_type():
        handle_exec(std::static_pointer_cast<bbdo::pb_remote_exec>(d)->obj(),
                    channel);
        break;
      default:
        break;
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(logger(), "remote actions: error: {}", e.what());
  }
}

}  // namespace

/**
 * @brief Register the action handler of this poller.
 *
 * @param broker_config_file The cbmod configuration file, its directory is
 * the target of the "broker" deployment files.
 */
void neb::remote_actions::load(const std::string& broker_config_file) {
  {
    absl::MutexLock lck(&g_conf_m);
    g_broker_dir = fs::path(broker_config_file).parent_path();
  }
  load_exec_allowlist();
  bbdo::remote_registry::instance().set_action_handler(&on_action);
}

/**
 * @brief Unregister the action handler.
 */
void neb::remote_actions::unload() {
  bbdo::remote_registry::instance().set_action_handler(nullptr);
}

/**
 * @brief Outcome of an Engine reload (reload thread). Answers the deployment
 * that asked for it, if any.
 */
void neb::remote_actions::reload_done(bool success,
                                      const std::string& message) {
  /* The exec allow-list is read again on every Engine reload. */
  load_exec_allowlist();
  std::unique_ptr<pending_reload> p;
  {
    absl::MutexLock lck(&g_deploy_m);
    std::swap(p, g_pending_reload);
  }
  if (!p)
    return;
  SPDLOG_LOGGER_INFO(logger(), "remote actions: deployment '{}' reload: {} {}",
                     p->id, success ? "applied" : "rejected", message);
  if (success)
    reply(p->channel, p->id, -1, REMOTE_APPLIED, "", p->config_version);
  else
    reply(p->channel, p->id, -1, REMOTE_REJECTED,
          fmt::format("engine reload failed: {}", message), p->config_version);
}

/**
 * @brief Install the Engine functions used by the remote actions.
 */
void neb::remote_actions::set_engine_bridge(engine_bridge&& b) {
  absl::MutexLock lck(&g_bridge_m);
  g_bridge = std::move(b);
}
