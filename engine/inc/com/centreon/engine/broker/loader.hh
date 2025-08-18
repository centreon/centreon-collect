/**
 * Copyright 2011-2013 Merethis
 * Copyright 2025 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef CCE_MODULES_LOADER_HH
#define CCE_MODULES_LOADER_HH

#include "com/centreon/engine/broker/handle.hh"

namespace com::centreon::engine::broker {

/**
 *  @class loader loader.hh
 *  @brief Modules loader.
 *
 *  Loader manage all modules.
 */
class loader {
  std::list<std::shared_ptr<handle> > _modules;

  loader();
  loader(loader const& right);
  virtual ~loader() noexcept;
  loader& operator=(loader const& right);

 public:
  std::shared_ptr<handle> add_module(std::string const& filename = "",
                                     std::string const& args = "");
  void del_module(std::shared_ptr<handle> const& mod);
  bool loaded(const std::filesystem::path& filename) const;
  std::list<std::shared_ptr<handle> > const& get_modules() const;
  static loader& instance();
  unsigned int load_directory(std::string const& dir);
  void unload_modules();
};
}  // namespace com::centreon::engine::broker

#endif  // !CCE_MODULES_LOADER_HH
