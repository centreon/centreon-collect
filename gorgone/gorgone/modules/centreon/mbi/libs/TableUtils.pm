#
# Copyright 2019 Centreon (http://www.centreon.com/)
#
# Centreon is a full-fledged industry-strength solution that meets
# the needs in IT infrastructure and application monitoring for
# service performance.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

use strict;
use warnings;

package gorgone::modules::centreon::mbi::libs::TableUtils;

use Try::Tiny;

# Drop and recreate a temporary table.
# A copy of the database server data directory (e.g. rsync of /var/lib/mysql)
# can leave an orphaned InnoDB tablespace (.ibd file) that is not known to the
# data dictionary: DROP TABLE IF EXISTS then does nothing and CREATE TABLE
# fails. This cannot be fixed from SQL, so the error is made actionable.
sub recreate_table {
    my ($db, $tableName, $createTableQuery) = @_;

    # Errors are only caught if queries die on failure.
    die "Cannot recreate table `$tableName`: the database handle must be created with die => 1\n"
        if (!$db->{die});

    try {
        $db->query({ query => "DROP TABLE IF EXISTS `$tableName`" });
        $db->query({ query => $createTableQuery });
    } catch {
        my $error = $_;

        # Errors raised by CREATE TABLE on an orphaned tablespace:
        # - MariaDB: 1005 ER_CANT_CREATE_TABLE, with handler errno 184:
        #     Can't create table `db`.`t` (errno: 184 "Tablespace already exists")
        # - MySQL 5.6/5.7: 1813 ER_TABLESPACE_EXISTS:
        #     Tablespace for table '`db`.`t`' exists. Please DISCARD the tablespace before IMPORT.
        # - MySQL 8.0: 1813 ER_TABLESPACE_EXISTS:
        #     Tablespace '%s' exists.
        # The MariaDB handler message comes from an untranslated list
        # (include/my_handler_errors.h), so it matches whatever lc_messages is.
        # Only the first line of the error is matched: gorgone::class::db
        # appends the failing query on a second line.
        my ($serverError) = split(/\n/, $error);
        die $error if (!defined($serverError) || $serverError !~ /Tablespace\b.*\bexists\b/i);

        my $message = "Cannot recreate table `$tableName`: an orphaned InnoDB tablespace "
            . "(.ibd file) was probably left behind by a copy of the database server data "
            . "directory. Check that <datadir>/<database>/$tableName.ibd is not used by any "
            . "table, remove it and restart the database server. Original error: $error";
        # The ETL reports the error in its own log; also make the hint visible in
        # the gorgone log, where the raw database error is logged.
        $db->{logger}->writeLogError($message) if (defined($db->{logger}));
        die $message;
    };
}

1;
