/*
** Copyright 2014 Centreon
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

#include "com/centreon/broker/bam/computable.hh"

using namespace com::centreon::broker::bam;

/**
 *  Add a new parent.
 *
 *  @param[in] parent Parent node.
 */
void computable::add_parent(std::shared_ptr<computable> const& parent) {
  _parents.push_back(std::weak_ptr<computable>(parent));
}

/**
 *  @brief Propagate the update of a child node.
 *
 *  This method will call the child_has_update() method of the parents
 *  of this node.
 *
 *  @param[out] visitor  Object that will receive events.
 */
void computable::propagate_update(
    io::stream* visitor,
    const std::shared_ptr<spdlog::logger>& logger) {
  //  _logger = logger;
  //  _logger->trace("{}::propagate_update: ", typeid(*this).name());
  //  std::vector<bool> filter(_parents.size());
  //  uint32_t i = 0;
  //  for (std::list<std::weak_ptr<computable> >::iterator it =
  //  _parents.begin(),
  //                                                       end = _parents.end();
  //       it != end; ++it) {
  //    std::shared_ptr<computable> ptr = it->lock();
  //    if (ptr)
  //      filter[i++] = ptr->child_has_update(this, visitor);
  //    else
  //      ++i;
  //  }
  //  i = 0;
  //  for (std::list<std::weak_ptr<computable> >::iterator it =
  //  _parents.begin(),
  //                                                       end = _parents.end();
  //       it != end; ++it)
  //    if (filter[i++]) {
  //      std::shared_ptr<computable> ptr = it->lock();
  //      if (ptr)
  //        ptr->propagate_update(visitor, _logger);
  //    }
}

/**
 *  Remove a parent.
 *
 *  @param[in] parent Parent node.
 */
void computable::remove_parent(std::shared_ptr<computable> const& parent) {
  for (std::list<std::weak_ptr<computable> >::iterator it(_parents.begin()),
       end(_parents.end());
       it != end; ++it)
    if (it->lock().get() == parent.get()) {
      _parents.erase(it);
      break;
    }
}

void computable::notify_parents_of_change(
    io::stream* visitor,
    const std::shared_ptr<spdlog::logger>& logger) {
  for (auto& p : _parents) {
    if (std::shared_ptr<computable> parent = p.lock())
      parent->update_from(this, visitor, logger);
  }
}
