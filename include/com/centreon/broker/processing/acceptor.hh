/*
** Copyright 2015-2019 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_PROCESSING_ACCEPTOR_HH
#define CCB_PROCESSING_ACCEPTOR_HH

#include <atomic>
#include <ctime>
#include <list>
#include <memory>
#include <string>
#include "com/centreon/broker/namespace.hh"
#include "com/centreon/broker/processing/thread.hh"

CCB_BEGIN()

// Forward declaration.
namespace io {
class endpoint;
}

namespace processing {
// Forward declaration.
class feeder;

/**
 *  @class acceptor acceptor.hh "com/centreon/broker/processing/acceptor.hh"
 *  @brief Accept incoming connections.
 *
 *  Accept incoming connections and launch a feeder thread.
 */
class acceptor : public bthread {
 public:
  acceptor(std::shared_ptr<io::endpoint> endp, std::string const& name);
  acceptor(acceptor const&) = delete;
  acceptor& operator=(acceptor const&) = delete;
  ~acceptor();
  void accept();
  void exit() override;
  void run() override;
  void set_read_filters(std::unordered_set<uint32_t> const& filters);
  void set_retry_interval(time_t retry_interval);
  void set_write_filters(std::unordered_set<uint32_t> const& filters);

 protected:
  // From stat_visitable
  std::string const& _get_read_filters() const override;
  std::string const& _get_write_filters() const override;
  virtual void _forward_statistic(json11::Json::object& tree) override;

 private:
  std::shared_ptr<io::endpoint> _endp;
  std::list<std::shared_ptr<processing::feeder> > _feeders;
  std::unordered_set<uint32_t> _read_filters;
  std::string _read_filters_str;
  time_t _retry_interval;
  std::unordered_set<uint32_t> _write_filters;
  std::string _write_filters_str;
  std::atomic_bool _listening;

  void _set_listening(bool listening) noexcept;
};
}  // namespace processing

CCB_END()

#endif  // !CCB_PROCESSING_ACCEPTOR_HH
