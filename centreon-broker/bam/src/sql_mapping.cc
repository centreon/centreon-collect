/*
** Copyright 2014 Merethis
**
** This file is part of Centreon Broker.
**
** Centreon Broker is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Broker is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Broker. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/broker/bam/sql_mapping.hh"
#include <map>

using namespace com::centreon::broker::bam;

/**
 *  @class  create_map
 *  @brief  A clever piece of code found on the net to
 *          automate the loading of a map from a literal expression.
 */
template <typename T, typename U>
class create_map {
public:
  /**
   *  Constructor
   */
  create_map(T const& key,U const& val) {
    m_map[key] = val;
  }

  /**
   *  @brief Operator (key, value).
   *
   *  This operator takes the same parameters as constructor so that the
   *  row for the constructor and all preceding rows are identical. This
   *  allows for a clean layout of table rows.
   *
   *  @param[in] key Key.
   *  @param[in] val Value.
   *
   *  @return This object.
   */
  create_map<T,U>& operator()(T const& key, U const& val) {
    m_map[key] = val;
    return (*this);
  }

  /**
   *  Operator map
   *
   *  @return  Returns the internal map loaded with all the values of
   *           the literal table.
   */
  operator std::map<T, U>() {
    return (m_map);
  }

private:
  std::map<T,U> m_map;
};

 /**
 *  This function maps the logical name for a RDBMS to the QT lib name.
 *
 *  @param[in]  The logical name for the database.
 *
 *  @return     The QT lib name for the database system.
 */
QString map_2_qt(std::string const& dbtype) {
  using namespace std;

  // Lower case the string.
  QString qt_db_type(dbtype.c_str());
  qt_db_type = qt_db_type.toLower();

  // Load map only ONCE at first execution.
  // Advantage   :  logN, typesafe, thread-safe ( AFTER init).
  // Disavantage :  race on initialisation.
  typedef map<QString, QString> string_2_string;
  static string_2_string name_2_qname =
    create_map<QString,QString>
      ("db2",        "QDB2")
      ("ibase",      "QIBASE")
      ("interbase",  "QIBASE")
      ("mysql",      "QMYSQL")
      ("oci",        "QOCI")
      ("oracle",     "QOCI")
      ("odbc",       "QODBC")
      ("psql",       "QPSQL")
      ("postgres",   "QPSQL")
      ("postgresql", "QPSQL")
      ("sqlite",     "QSQLITE")
      ("tds",        "QTDS")
      ("sybase",     "QTDS");

  // Find the database in table.
  string_2_string::iterator found = name_2_qname.find(qt_db_type);
  return (found != name_2_qname.end() ? found->first : qt_db_type);
}
