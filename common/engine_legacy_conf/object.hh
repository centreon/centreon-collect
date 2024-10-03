/**
 * Copyright 2011-2015 Merethis
 * Copyright 2016-2022 Centreon
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

#ifndef CCE_CONFIGURATION_OBJECT_HH
#define CCE_CONFIGURATION_OBJECT_HH

typedef std::list<std::string> list_string;
typedef std::set<std::string> set_string;

#ifndef LEGACY_CONF
#error This file should not be included.
#endif

namespace com::centreon::engine {

namespace configuration {

/**
 * @brief Error counter, it contains two attributes, one for warnings and
 * another for errors.
 */
struct error_cnt {
  uint32_t config_warnings = 0;
  uint32_t config_errors = 0;
};

class object {
 public:
  enum object_type {
    command = 0,
    connector = 1,
    contact = 2,
    contactgroup = 3,
    host = 4,
    hostdependency = 5,
    hostescalation = 6,
    hostextinfo = 7,
    hostgroup = 8,
    service = 9,
    servicedependency = 10,
    serviceescalation = 11,
    serviceextinfo = 12,
    servicegroup = 13,
    timeperiod = 14,
    anomalydetection = 15,
    severity = 16,
    tag = 17,
  };

  object(object_type type);
  object(object const& right);
  virtual ~object() noexcept = default;
  object& operator=(object const& right);
  bool operator==(object const& right) const noexcept;
  bool operator!=(object const& right) const noexcept;
  virtual void check_validity(error_cnt& err) const = 0;
  static std::shared_ptr<object> create(std::string const& type_name);
  virtual void merge(object const& obj) = 0;
  const std::string& name() const noexcept;
  virtual bool parse(char const* key, char const* value);
  virtual bool parse(std::string const& line);
  void resolve_template(
      std::unordered_map<std::string, std::shared_ptr<object> >& templates);
  bool should_register() const noexcept;
  object_type type() const noexcept;
  std::string const& type_name() const noexcept;

 protected:
  struct setters {
    char const* name;
    bool (*func)(object&, const char*);
  };

  template <typename T, typename U, bool (T::*ptr)(U)>
  struct setter {
    static bool generic(T& obj, const char* value) {
      U val(0);
      if constexpr (std::is_same_v<U, bool>) {
        if (absl::SimpleAtob(value, &val))
          return (obj.*ptr)(val);
        else
          return false;
      } else if constexpr (std::is_integral<U>::value) {
        if (absl::SimpleAtoi(value, &val))
          return (obj.*ptr)(val);
        else
          return false;
      } else if constexpr (std::is_same_v<U, float>) {
        if (absl::SimpleAtof(value, &val))
          return (obj.*ptr)(val);
        else
          return false;
      } else if constexpr (std::is_same_v<U, double>) {
        if (absl::SimpleAtod(value, &val))
          return (obj.*ptr)(val);
        else
          return false;
      } else {
        static_assert(std::is_integral_v<U> || std::is_floating_point_v<U> ||
                      std::is_same_v<U, bool>);
      }
    }
  };

  template <typename T, bool (T::*ptr)(std::string const&)>
  struct setter<T, std::string const&, ptr> {
    static bool generic(T& obj, const char* value) { return (obj.*ptr)(value); }
  };

  bool _set_name(std::string const& value);
  bool _set_should_register(bool value);
  bool _set_templates(std::string const& value);

  bool _is_resolve;
  std::string _name;
  static setters const _setters[];
  bool _should_register;
  list_string _templates;
  object_type _type;
  std::shared_ptr<spdlog::logger> _logger;
};

typedef std::shared_ptr<object> object_ptr;
typedef std::list<object_ptr> list_object;
typedef std::unordered_map<std::string, object_ptr> map_object;
}  // namespace configuration

}  // namespace com::centreon::engine

#define MRG_TAB(prop)                                       \
  do {                                                      \
    for (unsigned int i(0), end(prop.size()); i < end; ++i) \
      if (prop[i].empty())                                  \
        prop[i] = tmpl.prop[i];                             \
  } while (false)
#define MRG_DEFAULT(prop) \
  if (prop.empty())       \
  prop = tmpl.prop
#define MRG_IMPORTANT(prop)                     \
  if (prop.empty() || tmpl.prop##_is_important) \
  prop = tmpl.prop
#define MRG_INHERIT(prop)       \
  do {                          \
    if (!prop.is_set())         \
      prop = tmpl.prop;         \
    else if (prop.is_inherit()) \
      prop += tmpl.prop;        \
    prop.is_inherit(false);     \
  } while (false)
#define MRG_MAP(prop) prop.insert(tmpl.prop.begin(), tmpl.prop.end())
#define MRG_OPTION(prop)      \
  do {                        \
    if (!prop.is_set()) {     \
      if (tmpl.prop.is_set()) \
        prop = tmpl.prop;     \
    }                         \
  } while (false)

#endif  // !CCE_CONFIGURATION_OBJECT_HH
