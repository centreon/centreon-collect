/*
** Copyright 2011-2013,2017 Centreon
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

#ifndef CCE_CONFIGURATION_APPLIER_SERVICEDEPENDENCY_HH
#define CCE_CONFIGURATION_APPLIER_SERVICEDEPENDENCY_HH

#include <absl/container/flat_hash_set.h>
#include "common/configuration/state.pb.h"
// #include "common/configuration/state-generated.pb.h"

namespace com::centreon::engine {

namespace configuration {
// Forward declarations.
class servicedependency;
class state;

#ifdef LEGACY_CONF
size_t servicedependency_key_l(const servicedependency& sd);
#else
size_t servicedependency_key(const Servicedependency& sd);
#endif

namespace applier {
class servicedependency {
  void _expand_services(
      std::list<std::string> const& hst,
      std::list<std::string> const& hg,
      std::list<std::string> const& svc,
      std::list<std::string> const& sg,
      configuration::state& s,
      std::set<std::pair<std::string, std::string>>& expanded);

  void _expand_services(
      const ::google::protobuf::RepeatedPtrField<std::string>& hst,
      const ::google::protobuf::RepeatedPtrField<std::string>& hg,
      const ::google::protobuf::RepeatedPtrField<std::string>& svc,
      const ::google::protobuf::RepeatedPtrField<std::string>& sg,
      configuration::State& s,
      absl::flat_hash_set<std::pair<std::string, std::string>>& expanded);

 public:
  servicedependency() = default;
  ~servicedependency() noexcept = default;
  servicedependency(const servicedependency&) = delete;
  servicedependency& operator=(const servicedependency&) = delete;
#ifdef LEGACY_CONF
  void add_object(configuration::servicedependency const& obj);
  void modify_object(configuration::servicedependency const& obj);
  void expand_objects(configuration::state& s);
  void remove_object(configuration::servicedependency const& obj);
  void resolve_object(configuration::servicedependency const& obj);
#else
  void add_object(const configuration::Servicedependency& obj);
  void modify_object(configuration::Servicedependency* old_obj,
                     const configuration::Servicedependency& new_obj);
  void remove_object(ssize_t idx);
  void expand_objects(configuration::State& s);
  void resolve_object(const configuration::Servicedependency& obj);
#endif
};
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_SERVICEDEPENDENCY_HH
