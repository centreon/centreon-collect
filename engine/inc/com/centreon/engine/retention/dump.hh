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

#ifndef CCE_RETENTION_DUMP_HH
#define CCE_RETENTION_DUMP_HH

#include "com/centreon/engine/customvariable.hh"
#include "common/downtimes/downtime.hh"
#include "engine/src/notifications/notification_types.hh"

// Forward declaration.

namespace com::centreon::engine {
class comment;
class contact;
class customvariable;
class service;
class anomalydetection;
class host;

namespace retention {
namespace dump {
std::ostream& contact(std::ostream& os, contact const& obj);
std::ostream& contacts(std::ostream& os);
std::ostream& customvariables(std::ostream& os,
                              com::centreon::engine::map_customvar const& obj);
std::ostream& notifications(
    std::ostream& os,
    std::array<com::centreon::engine::notifications::notification*, 6> const&
        obj);
std::ostream& scheduled_downtime(
    std::ostream& os,
    const com::centreon::common::downtimes::downtime& obj);
std::ostream& downtimes(std::ostream& os);
std::ostream& header(std::ostream& os);
std::ostream& host(std::ostream& os, com::centreon::engine::host const& obj);
std::ostream& hosts(std::ostream& os);
std::ostream& info(std::ostream& os);
std::ostream& program(std::ostream& os);
bool save(std::string const& path);
std::ostream& service(std::ostream& os,
                      const std::string_view& class_name,
                      com::centreon::engine::service const& obj);

std::ostream& service(std::ostream& os,
                      com::centreon::engine::service const& obj);

std::ostream& anomalydetection(
    std::ostream& os,
    com::centreon::engine::anomalydetection const& obj);
std::ostream& services(std::ostream& os);
}  // namespace dump
}  // namespace retention
}  // namespace com::centreon::engine

#endif  // !CCE_RETENTION_DUMP_HH
