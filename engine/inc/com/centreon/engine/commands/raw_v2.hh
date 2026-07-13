/**
 * Copyright 2025 Centreon
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

#ifndef CCE_COMMANDS_RAW_V2_HH
#define CCE_COMMANDS_RAW_V2_HH
#include "com/centreon/common/process/process.hh"
#include "com/centreon/common/process/process_args.hh"
#include "com/centreon/engine/commands/command.hh"

namespace com::centreon::engine {

namespace commands {
class environment;

/**
 *  @brief raw_v2 is a specific implementation of command.
 * It uses common::process to start commands
 *
 */
class raw_v2 : public command {
  std::shared_ptr<asio::io_context> _io_context;

  /**
   * @brief in order to not parse cmdlines each time, we store result of cmd
   * line parsing in a container indexed by cmd line and last used. We allow a
   * max size of 100 elements by raw_v2 instance
   *
   */
  struct args_cache_element {
    args_cache_element(const std::string& processed__cmd,
                       const common::process_args::pointer& process__args)
        : processed_cmd(processed__cmd),
          process_args(process__args),
          last_used(std::chrono::system_clock::now()) {}
    std::string processed_cmd;
    common::process_args::pointer process_args;
    std::chrono::system_clock::time_point last_used;

    static void refresh_last_used(args_cache_element& to_update) {
      to_update.last_used = std::chrono::system_clock::now();
    }
  };

  using cmd_line_cache = boost::multi_index::multi_index_container<
      args_cache_element,
      boost::multi_index::indexed_by<
          boost::multi_index::hashed_unique<BOOST_MULTI_INDEX_MEMBER(
              args_cache_element,
              std::string,
              processed_cmd)>,
          boost::multi_index::ordered_non_unique<BOOST_MULTI_INDEX_MEMBER(
              args_cache_element,
              std::chrono::system_clock::time_point,
              last_used)>>>;

  /**
   * @brief we keep a reference of all running checks in order to kill them at
   * engine shutdown
   *
   */
  absl::flat_hash_set<std::shared_ptr<com::centreon::common::process<true>>>
      _running ABSL_GUARDED_BY(_data_m);

  cmd_line_cache _args_cache ABSL_GUARDED_BY(_data_m);

  absl::Mutex _data_m;

  std::shared_ptr<spdlog::logger> _commands_logger;

  void _on_complete(const common::process<true>& proc,
                    uint64_t command_id,
                    time_t start,
                    int exit_code,
                    common::e_exit_status exit_status,
                    const std::string& std_out,
                    const std::string& std_err);

  common::process_args::pointer _args_from_cmd_line(
      const std::string& processed_cmd);

 public:
  raw_v2(const std::shared_ptr<asio::io_context> io_context,
         const std::string& name,
         const std::string& command_line,
         const std::shared_ptr<command_listener>& listener = nullptr);
  raw_v2(const raw_v2&) = delete;
  ~raw_v2() override;
  raw_v2& operator=(const raw_v2&) = delete;

  std::shared_ptr<raw_v2> shared_from_this() {
    return std::static_pointer_cast<raw_v2>(command::shared_from_this());
  }

  uint64_t run(const std::string& process_cmd,
               nagios_macros& macros,
               uint32_t timeout,
               const check_result::pointer& to_push_to_checker,
               const notifier* caller = nullptr) override;
  void run(const std::string& process_cmd,
           nagios_macros& macros,
           uint32_t timeout,
           result& res) override;
};
}  // namespace commands

}  // namespace com::centreon::engine

#endif  // !CCE_COMMANDS_RAW_HH
