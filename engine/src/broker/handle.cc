/**
* Copyright 2011-2013,2016, 2021 Centreon
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

#include "com/centreon/engine/broker/handle.hh"
#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/log_v2.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/nebmodules.hh"

using namespace com::centreon::engine::broker;
using namespace com::centreon::engine::logging;

/**
 *  Constructor.
 *
 *  @param[in] filename The module filename.
 *  @param[in] args     The module args.
 */
handle::handle(const std::string& filename, const std::string& args)
    : _args(args), _filename(filename), _name(filename) {}

/**
 *  Destructor.
 */
handle::~handle() noexcept {}

/**
 *  Default egality operator.
 *
 *  @param[in] right The object to compare.
 *
 *  @return true or false.
 */
bool handle::operator==(handle const& right) const noexcept {
  return _args == right._args && _author == right._author &&
         _copyright == right._copyright && _description == right._description &&
         _filename == right._filename && _license == right._license &&
         _name == right._name && _version == right._version &&
         _handle.get() == right._handle.get();
}

/**
 *  Default no egality operator.
 *
 *  @param[in] right The object to compare.
 *
 *  @return true or false.
 */
bool handle::operator!=(handle const& right) const noexcept {
  return !operator==(right);
}

/**
 *  Close and unload module.
 */
void handle::close() {
  if (_handle.get()) {
    if (_handle->is_loaded()) {
      typedef int (*func_deinit)(int, int);
      func_deinit deinit(
          (func_deinit)_handle->resolve_proc("nebmodule_deinit"));
      if (!deinit) {
        engine_logger(log_info_message, basic)
            << "Cannot resolve symbole 'nebmodule_deinit' in module '"
            << _filename << "'.";
        log_v2::process()->error(
            "Cannot resolve symbole 'nebmodule_deinit' in module '{}'.",
            _filename);
      } else
        deinit(NEBMODULE_FORCE_UNLOAD | NEBMODULE_ENGINE,
               NEBMODULE_NEB_SHUTDOWN);
      _handle->unload();
    }
    _handle.reset();
  }
}

/**
 *  Get the module's arguments.
 *
 *  @return The arguments.
 */
const std::string& handle::get_args() const noexcept {
  return _args;
}

/**
 *  Get the module's author name.
 *
 *  @return The author name.
 */
const std::string& handle::get_author() const noexcept {
  return _author;
}

/**
 *  Get the module's copyright.
 *
 *  @return The copyright.
 */
const std::string& handle::get_copyright() const noexcept {
  return _copyright;
}

/**
 *  Get the module's description.
 *
 *  @return The description.
 */
const std::string& handle::get_description() const noexcept {
  return _description;
}

/**
 *  Get the module's filename.
 *
 *  @return The filename.
 */
const std::string& handle::get_filename() const noexcept {
  return _filename;
}

/**
 *  Get the handle of the module.
 *
 *  @return pointer on a library.
 */
com::centreon::library* handle::get_handle() const noexcept {
  return _handle.get();
}

/**
 *  Get the module's license.
 *
 *  @return The license.
 */
const std::string& handle::get_license() const noexcept {
  return _license;
}

/**
 *  Get the module's name.
 *
 *  @return The name.
 */
const std::string& handle::get_name() const noexcept {
  return _name;
}

/**
 *  Get the module's version.
 *
 *  @return The version.
 */
const std::string& handle::get_version() const noexcept {
  return _version;
}

/**
 *  Check if the module is loaded.
 *
 *  @return true if the module is loaded, false otherwise.
 */
bool handle::is_loaded() {
  return _handle.get() && _handle->is_loaded();
}

/**
 *  Open and load module.
 */
void handle::open() {
  if (is_loaded())
    return;

  try {
    _handle = std::make_shared<library>(_filename);
    _handle->load();

    int api_version(*static_cast<int*>(_handle->resolve("__neb_api_version")));
    if (api_version != CURRENT_NEB_API_VERSION)
      throw(engine_error() << "Module is using an old or unspecified "
                              "version of the event broker API");

    typedef int (*func_init)(int, char const*, void*);
    func_init init((func_init)_handle->resolve_proc("nebmodule_init"));

    if (init(NEBMODULE_NORMAL_LOAD | NEBMODULE_ENGINE, _args.c_str(), this) !=
        OK)
      throw(engine_error() << "Function nebmodule_init "
                              "returned an error");

  } catch (std::exception const& e) {
    log_v2::process()->error("fail to load broker module {}", e.what());
    close();
    throw;
  }
}

/**
 *  Open and load module.
 *
 *  @param[in] filename The module filename.
 *  @param[in] args The module arguments.
 */
void handle::open(const std::string& filename, const std::string& args) {
  if (is_loaded())
    return;

  close();
  _filename = filename;
  _args = args;
  open();
}

/**
 *  Reload the module.
 */
void handle::reload() {
  if (!is_loaded())
    return;
  typedef int (*func_reload)();
  func_reload routine((func_reload)_handle->resolve_proc("nebmodule_reload"));
  if (routine)
    routine();
}

/**
 *  Set the module's author name.
 *
 *  @param[in] The author name.
 */
void handle::set_author(const std::string& author) {
  _author = author;
}

/**
 *  Set the module's copyright.
 *
 *  @param[in] The copyright.
 */
void handle::set_copyright(const std::string& copyright) {
  _copyright = copyright;
}

/**
 *  Set the module's description.
 *
 *  @param[in] The description.
 */
void handle::set_description(const std::string& description) {
  _description = description;
}

/**
 *  Set the module's license.
 *
 *  @param[in] The license.
 */
void handle::set_license(const std::string& license) {
  _license = license;
}

/**
 *  Set the module's name.
 *
 *  @param[in] The name.
 */
void handle::set_name(const std::string& name) {
  _name = name;
}

/**
 *  Set the module's version.
 *
 *  @param[in] The version.
 */
void handle::set_version(const std::string& version) {
  _version = version;
}
