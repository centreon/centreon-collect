/*
** Copyright 2011 Merethis
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

#ifndef CCE_CONFIGURATION_APPLIER_BASE_HH
# define CCE_CONFIGURATION_APPLIER_BASE_HH

namespace                com {
  namespace              centreon {
    namespace            engine {
      namespace          configuration {
	namespace        applier {
	/**
	 *  @class base base.hh
	 *  @brief Parent class of all applier objects.
	 *
	 *  Parent class of all applier objects.
	 */
	  class          base {
	  public:
	    virtual      ~base() throw() = 0;

	    virtual void apply(state const& config) = 0;
	  };

	  inline base::~base() throw() {}
	}
      }
    }
  }
}

#endif // !CCE_CONFIGURATION_APPLIER_BASE_HH
