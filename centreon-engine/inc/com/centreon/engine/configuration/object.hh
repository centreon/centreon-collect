/*
** Copyright 2011-2013 Merethis
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

#ifndef CCE_CONFIGURATION_OBJECT_HH
#  define CCE_CONFIGURATION_OBJECT_HH

#  include <algorithm>
#  include <list>
#  include <map>
#  include <sstream>
#  include <string>
#  include "com/centreon/engine/namespace.hh"
#  include "com/centreon/shared_ptr.hh"
#  include "com/centreon/unordered_hash.hh"

typedef std::list<std::string>             list_string;
typedef std::map<std::string, std::string> properties;

CCE_BEGIN()

namespace                  configuration {
  class                    object {
  public:
    enum                   object_type {
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
      timeperiod = 14
    };

                           object(object_type type);
                           object(object const& right);
    virtual                ~object() throw ();
    object&                operator=(object const& right);
    bool                   operator==(
                             object const& right) const throw ();
    bool                   operator!=(
                             object const& right) const throw ();
    virtual void           check_validity() const = 0;
    static shared_ptr<object>
                           create(std::string const& type_name);
    virtual std::size_t    id() const throw () = 0;
    bool                   is_template() const throw ();
    virtual void           merge(object const& obj) = 0;
    std::string const&     name() const throw ();
    virtual bool           parse(
                             std::string const& key,
                             std::string const& value);
    virtual bool           parse(std::string const& line);
    void                   resolve_template(
                             umap<std::string, shared_ptr<object> >& templates);
    object_type            type() const throw ();
    std::string const&     type_name() const throw ();

  protected:
    template<typename T, typename U, bool (T::*ptr)(U)>
    struct setter {
      static bool generic(T& obj, std::string const& value) {
        U val;
        std::istringstream iss(value);
        if (!(iss >> val) || !iss.eof())
          return (false);
        return ((obj.*ptr)(val));
      }
    };

    template<typename T, bool (T::*ptr)(std::string const&)>
    struct              setter<T, std::string const&, ptr> {
      static bool       generic(T& obj, std::string const& value) {
        return ((obj.*ptr)(value));
      }
    };

    bool                   _set_is_not_template(bool value);
    bool                   _set_name(std::string const& value);
    bool                   _set_templates(std::string const& value);

    mutable std::size_t    _id;
    bool                   _is_resolve;
    bool                   _is_template;
    std::string            _name;
    list_string            _templates;
    object_type            _type;
  };

  typedef shared_ptr<object>            object_ptr;
  typedef std::list<object_ptr>         list_object;
  typedef umap<std::string, object_ptr> map_object;
}

CCE_END()

namespace std {
  template <typename T>
  struct  less<com::centreon::shared_ptr<T> >
            : std::binary_function<
                     com::centreon::shared_ptr<T>,
                     com::centreon::shared_ptr<T>,
                     bool> {
    bool  operator()(
            com::centreon::shared_ptr<T> const& left,
            com::centreon::shared_ptr<T> const& right) {
      bool retval;
      if (left.get()) {
        if (right.get())
          retval = *left < *right;
        else
          retval = false;
      }
      else if (right.get())
        retval = true;
      else
        retval = false;
      return (retval);
    }
  };
}

#  define MRG_TAB(prop) \
  do { \
    for (unsigned int i(0), end(prop.size()); \
         i < end; \
         ++i) \
      if (prop[i].empty()) prop[i] = tmpl.prop[i]; \
  } while (false)
#  define MRG_DEFAULT(prop) \
  if (prop.empty()) prop = tmpl.prop
#  define MRG_IMPORTANT(prop) \
  if (prop.empty() || tmpl.prop##_is_important) prop = tmpl.prop
#  define MRG_INHERIT(prop) \
  if (!prop.is_set() || prop.get().is_add_inherit()) prop.set(tmpl.prop)
#  define MRG_MAP(prop) \
  prop.insert(tmpl.prop.begin(), tmpl.prop.end())
#  define MRG_OPTION(prop) \
  do { \
    if (!prop.is_set()) { \
      if (tmpl.prop.is_set()) \
        prop = tmpl.prop; \
      else \
        prop = default##prop; \
    } \
  } while (false)

#endif // !CCE_CONFIGURATION_OBJECT_HH

