/*
 * Copyright 2014, 2023 Centreon
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

#ifndef CCB_BAM_COMPUTABLE_HH
#define CCB_BAM_COMPUTABLE_HH

#include <spdlog/logger.h>
#include <memory>
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/persistent_cache.hh"

namespace com::centreon::broker::bam {
/**
 *  @class computable computable.hh "com/centreon/broker/bam/computable.hh"
 *  @brief Object that get computed by the BAM engine.
 *
 *  The computation of such objects is triggered by the BAM engine. It
 *  provides an effective way to compute whole part of the BA/KPI tree.
 */
class computable {
 protected:
  std::list<std::weak_ptr<computable>> _parents;
  std::shared_ptr<spdlog::logger> _logger;

 public:
  computable(const std::shared_ptr<spdlog::logger>& logger) : _logger(logger) {}
  computable(const computable&) = delete;
  virtual ~computable() noexcept = default;
  computable& operator=(const computable&) = delete;
  void add_parent(const std::shared_ptr<computable>& parent);
  void notify_parents_of_change(io::stream* visitor);
  /**
   *  @brief Update this object because there was a change in the given child.
   *
   *  Once this object is updated, if it has changed, it notifies its parents
   *  to propagate the information.
   *
   *  @param[in] child Recently changed child node.
   *  @param[in] visitor This is used to manage events
   */
  virtual void update_from(computable* child, io::stream* visitor, const std::shared_ptr<spdlog::logger>& logger) = 0;
  void remove_parent(const std::shared_ptr<computable>& parent);
  /**
   * @brief This method is used by the dump() method. It gives a summary of this
   * computable main informations.
   *
   * @return A multiline string with various informations.
   */
  virtual std::string object_info() const = 0;
  /**
   * @brief Recursive or not method that writes object informations to the
   * output stream. If there are children, each one dump() is then called.
   *
   * @param output An output stream.
   */
  virtual void dump(std::ofstream& output) const = 0;
  void dump_parents(std::ofstream& output) const;
};
}  // namespace com::centreon::broker::bam

#endif  // !CCB_BAM_COMPUTABLE_HH
