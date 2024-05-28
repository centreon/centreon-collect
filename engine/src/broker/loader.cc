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
#include "com/centreon/engine/broker/handle.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/io/directory_entry.hh"
#include "com/centreon/io/file_stream.hh"

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
  io::directory_entry directory(dir);
  std::list<io::file_entry> const& files(directory.entry_list("*.so"));

  // Sort by file name.
  std::multimap<std::string, io::file_entry> sort_files;
  for (std::list<io::file_entry>::const_iterator it(files.begin()),
       end(files.end());
       it != end; ++it)
    sort_files.insert(std::make_pair(it->file_name(), *it));

  // Load modules.
  unsigned int loaded(0);
  for (std::multimap<std::string, io::file_entry>::const_iterator
           it(sort_files.begin()),
       end(sort_files.end());
       it != end; ++it) {
    io::file_entry const& f(it->second);
    std::string config_file(dir + "/" + f.base_name() + ".cfg");
    if (io::file_stream::exists(config_file.c_str()) == false)
      config_file = "";
    std::shared_ptr<handle> module;
    try {
      module = add_module(dir + "/" + f.file_name(), config_file);
      module->open();
      engine_logger(log_info_message, basic)
          << "Event broker module '" << f.file_name()
          << "' initialized successfully.";
      events_logger->info("Event broker module '{}' initialized successfully.",
                          f.file_name());
      ++loaded;
    } catch (error const& e) {
      del_module(module);
      engine_logger(log_runtime_error, basic)
          << "Error: Could not load module '" << f.file_name() << "' -> "
          << e.what();
      runtime_logger->error("Error: Could not load module '{}' -> {}",
                            f.file_name(), e.what());
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
