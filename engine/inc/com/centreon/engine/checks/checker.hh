/*
** Copyright 2011-2014 Merethis
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCE_CHECKS_CHECKER_HH
#define CCE_CHECKS_CHECKER_HH

#include "com/centreon/engine/anomalydetection.hh"
#include "com/centreon/engine/commands/command.hh"

namespace com::centreon::engine::checks {

/**
 *  @class checks check_result.hh
 *  @brief Run object and reap the result.
 *
 *  Checker is a singleton to run host or service and reap the
 *  result.
 */
class checker : public commands::command_listener {
  static checker* _instance;

 public:
  static checker& instance();
  static void init(bool used_by_test = false);
  static void deinit();

  void clear() noexcept;
  void reap();
  void run_sync(host* hst,
                host::host_state* check_result_code,
                int check_options,
                int use_cached_result,
                unsigned long check_timestamp_horizon);
  void add_check_result(uint64_t id,
                        const check_result::pointer result) noexcept;
  void add_check_result_to_reap(const check_result::pointer result) noexcept;
  static void forget(notifier* n) noexcept;

  enum class e_completion_filter { all, service, host };

  void wait_completion(e_completion_filter filter = e_completion_filter::all);

  template <class queue_handler>
  void inspect_reap_partial(queue_handler&& handler) const;

 private:
  checker(bool used_by_test);
  checker(checker const& right);
  ~checker() noexcept override;
  checker& operator=(checker const& right);
  void finished(commands::result const& res) noexcept override;
  host::host_state _execute_sync(host* hst);

  /* A mutex to protect access on _waiting_check_result and _to_reap_partial */
  mutable std::mutex _mut_reap;
  /*
   * Here is the list of prepared check results but with a command being
   * running. When the command will be finished, each check result is get back
   * updated and moved to _to_reap_partial list. */
  std::unordered_map<uint64_t, check_result::pointer> _waiting_check_result;
  /* This queue is filled during a cycle. When it is time to reap, its elements
   * are passed to _to_reap. It can then be filled in parallel during the
   * _to_reap treatment. */
  std::deque<check_result::pointer> _to_reap_partial;
  /*
   * The list of check_results to reap: they contain data that have to be
   * translated to services/hosts. */
  std::deque<check_result::pointer> _to_reap;

  /* Due to reloads of centengine we have the following list with notifiers
   * that should be forgotten if notifiers are removed. */
  std::deque<notifier*> _to_forget;

  /**
   * used only for test in order to wait for completion
   */
  const bool _used_by_test;
  std::condition_variable _finish_cond;
};

/**
 * @brief allow to inspect _to_reap_partial (used only by tests)
 *
 * @tparam queue_handler
 * @param handler must have () (const std::deque<check_result::pointer>  &)
 */
template <class queue_handler>
void checker::inspect_reap_partial(queue_handler&& handler) const {
  std::lock_guard<std::mutex> lock(_mut_reap);
  handler(_to_reap_partial);
}

}  // namespace com::centreon::engine::checks

#endif  // !CCE_CHECKS_CHECKER_HH
