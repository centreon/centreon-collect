/**
 * Copyright 2011-2013 Merethis
 * Copyright 2017-2024 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include "com/centreon/engine/broker/loader.hh"
#include <filesystem>
#include "com/centreon/engine/broker/handle.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::exceptions;
using namespace com::centreon::engine::broker;
using namespace com::centreon::engine::logging;

/**
 *  Add a new module.
 *
 *  @param[in] filename The module filename.
 *  @param[in] args     The arguments module.
 *
 *  @return The new object module.
 */
std::shared_ptr<engine::broker::handle> loader::add_module(
    std::string const& filename,
    std::string const& args) {
  auto module = std::make_shared<handle>(filename, args);
  _modules.push_back(module);
  return module;
}

/**
 *  Remove a module.
 *
 *  @param[in] mod Module to remove.
 */
void loader::del_module(std::shared_ptr<handle> const& module) {
  for (std::list<std::shared_ptr<handle>>::iterator it(_modules.begin()),
       end(_modules.end());
       it != end; ++it)
    if (it->get() == module.get()) {
      _modules.erase(it);
      break;
    }
}

/**
 *  Get all modules.
 *
 *  @return All modules in a list.
 */
std::list<std::shared_ptr<engine::broker::handle>> const& loader::get_modules()
    const {
  return _modules;
}

/**
 *  Get instance of loader singleton.
 *
 *  @return Class instance.
 */
loader& loader::instance() {
  static loader instance;
  return instance;
}

/**
 *  Load modules in the specify directory.
 *
 *  @param[in] dir Directory to load.
 *
 *  @return Number of modules loaded.
 */
unsigned int loader::load_directory(std::string const& dir) {
  // Get directory entries.

  std::filesystem::path directory(dir);
  std::list<std::filesystem::path> files;
  for (const auto& entry : std::filesystem::directory_iterator(directory)) {
    if (entry.is_regular_file() && entry.path().extension() == ".so") {
      files.push_back(entry.path());
    }
  }

  // Sort by file name.
  std::multimap<std::string, std::filesystem::path> sort_files;
  for (const auto& file : files)
    sort_files.insert(std::make_pair(file.filename().string(), file));

  // Load modules.
  unsigned int loaded = 0;
  for (const auto& [name, f] : sort_files) {
    std::string cfg_file = f.stem().string() + ".cfg";
    std::filesystem::path config_file = dir / std::filesystem::path(cfg_file);
    if (!std::filesystem::exists(config_file))
      config_file.clear();

    std::shared_ptr<handle> module;
    try {
      module = add_module(dir / f, config_file);
      module->open();
      engine_logger(log_info_message, basic)
          << "Event broker module '" << f.filename()
          << "' initialized successfully.";
      events_logger->info("Event broker module '{}' initialized successfully.",
                          f.filename().string());
      ++loaded;
    } catch (error const& e) {
      del_module(module);
      engine_logger(log_runtime_error, basic)
          << "Error: Could not load module '" << f.filename() << "' -> "
          << e.what();
      runtime_logger->error("Error: Could not load module '{}' -> {}",
                            f.filename().string(), e.what());
    }
  }
  return loaded;
}

/**
 *  Unload all modules.
 */
void loader::unload_modules() {
  for (std::list<std::shared_ptr<handle>>::iterator it(_modules.begin()),
       end(_modules.end());
       it != end; ++it) {
    try {
      (*it)->close();
    } catch (...) {
    }
    engine_logger(dbg_eventbroker, basic)
        << "Module '" << (*it)->get_filename() << "' unloaded successfully.";
    eventbroker_logger->trace("Module '{}' unloaded successfully.",
                              (*it)->get_filename());
  }
  _modules.clear();
}

/**
 *  Default constructor.
 */
loader::loader() {}

/**
 *  Default destructor.
 */
loader::~loader() noexcept {
  try {
    unload_modules();
  } catch (...) {
  }
}
