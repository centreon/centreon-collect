/*
** Copyright 2011-2013,2016 Centreon
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

#ifndef CCE_BROKER_HANDLE_HH
#define CCE_BROKER_HANDLE_HH

#include "com/centreon/library.hh"

namespace com::centreon::engine {

namespace broker {
/**
 *  @class handle handle.hh
 *  @brief Handle contains module informations.
 *
 *  Handle is a module object, contains information
 *  about module, start and stop module.
 */
class handle {
  std::string _args;
  std::string _author;
  std::string _copyright;
  std::string _description;
  std::string _filename;
  std::shared_ptr<library> _handle;
  std::string _license;
  std::string _name;
  std::string _version;

 public:
  handle(const std::string& filename = "", const std::string& args = "");
  virtual ~handle() noexcept;
  handle(const handle&) = delete;
  handle& operator=(const handle&) = delete;
  bool operator==(handle const& right) const noexcept;
  bool operator!=(handle const& right) const noexcept;
  void close();
  library* get_handle() const noexcept;
  const std::string& get_author() const noexcept;
  const std::string& get_copyright() const noexcept;
  const std::string& get_description() const noexcept;
  const std::string& get_filename() const noexcept;
  const std::string& get_license() const noexcept;
  const std::string& get_name() const noexcept;
  const std::string& get_version() const noexcept;
  const std::string& get_args() const noexcept;
  bool is_loaded();
  void open();
  void open(const std::string& filename, const std::string& args);
  void reload();
  void set_author(const std::string& author);
  void set_copyright(const std::string& copyright);
  void set_description(const std::string& description);
  void set_license(const std::string& license);
  void set_name(const std::string& name);
  void set_version(const std::string& version);
};
}  // namespace broker

}

#endif  // !CCE_BROKER_HANDLE_HH
