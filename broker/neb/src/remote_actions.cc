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
#include "common/log_v2/log_v2.hh"

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/strings/match.h>
#include <absl/strings/str_split.h>
#include <absl/synchronization/mutex.h>
#include <openssl/evp.h>
#include <cstring>
#include <boost/asio/steady_timer.hpp>
#include <deque>
#include <filesystem>
#include <fstream>

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
