/*
** Copyright 2012-2013 Merethis
**
** This file is part of Centreon Perl Connector.
**
** Centreon Perl Connector is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Perl Connector is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Perl Connector. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef TEST_CONNECTOR_MISC_HH
#  define TEST_CONNECTOR_MISC_HH

std::string& replace_null(std::string& str);
void         write_file(
               char const* filename,
               char const* content,
               unsigned int size = 0);

#endif //! TEST_CONNECTOR_MISC_HH
