/**
 * Copyright 2014-2024 Centreon
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

#include "com/centreon/broker/bam/computable.hh"

using namespace com::centreon::broker::bam;

/**
 *  Add a new parent.
 *
 *  @param[in] parent Parent node.
 */
void computable::add_parent(std::shared_ptr<computable> const& parent) {
  for (auto it = _parents.begin(), end = _parents.end(); it != end; ++it)
    if (it->lock().get() == parent.get())
      return;
  _parents.push_back(std::weak_ptr<computable>(parent));
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

/**
 * @brief Notify parents of this object because of a change made in this.
 *
 * A parent that is gone -- a KPI the applier removed at a reload while this
 * object, its source, stayed -- is dropped from the list on the way: nothing
 * calls remove_parent() today, so the expired entries used to pile up, one per
 * removed KPI, each costing a lock() at every notification for ever.
 *
 * @param visitor Used to handle events.
 */
void computable::notify_parents_of_change(io::stream* visitor) {
  _logger->trace("{}::notify_parents_of_change: ", typeid(*this).name());
  for (auto it = _parents.begin(); it != _parents.end();) {
    if (std::shared_ptr<computable> parent = it->lock()) {
      parent->update_from(this, visitor);
      ++it;
    } else
      it = _parents.erase(it);
  }
}

/**
 * @brief Add to the output stream informations about this computable parents.
 *
 * @param output The output stream.
 */
void computable::dump_parents(std::ofstream& output) const {
  for (auto& p : _parents) {
    if (std::shared_ptr<computable> parent = p.lock())
      output << fmt::format("\"{}\" -> \"{}\" [style=dashed]\n", object_info(),
                            parent->object_info());
  }
}
