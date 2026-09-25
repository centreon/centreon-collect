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

# Helpers to create tables with an actionable error when the database server
# data directory was copied (e.g. rsync of /var/lib/mysql) and left an orphaned
# InnoDB tablespace (.ibd file) that is not known to the data dictionary:
# DROP TABLE IF EXISTS then does nothing and CREATE TABLE fails. This cannot be
# fixed from SQL.
# The database handle must be created with die => 1.

# Create a table.
sub create_table {
    my ($db, $tableName, $createTableQuery) = @_;

    check_handle($db, $tableName);

    try {
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
        # Only the server message is matched: gorgone::class::db appends the
        # caller to it, and the failing query on a second line.
        my ($serverError) = split(/\n/, $error);
        $serverError =~ s/ \(caller: [^)]*\)\z// if (defined($serverError));
        die $error if (!defined($serverError) || $serverError !~ /Tablespace\b.*\bexists\b/i);

        # A partitioned InnoDB table has one tablespace per partition:
        # t#P#p1.ibd (MariaDB, MySQL 5.7) or t#p#p1.ibd (MySQL 8.0).
        my $files = $createTableQuery =~ /\bPARTITION\s+BY\b/i
            ? "<datadir>/<database>/$tableName#P#*.ibd (or $tableName#p#*.ibd on MySQL 8.0) are"
            : "<datadir>/<database>/$tableName.ibd is";
        # The failing query is left out: it can be huge (one clause per
        # partition) and would hide the hint.
        my $message = "Cannot create table `$tableName`: an orphaned InnoDB tablespace "
            . "(.ibd file) was probably left behind by a copy of the database server data "
            . "directory. Check that $files not used by any table, remove the file(s) and "
            . "restart the database server. Original error: $serverError\n";
        # The ETL reports the error in its own log; also make the hint visible in
        # the gorgone log, where the raw database error is logged.
        $db->{logger}->writeLogError($message) if (defined($db->{logger}));
        die $message;
    };
}

# Drop and create a table.
sub recreate_table {
    my ($db, $tableName, $createTableQuery) = @_;

    check_handle($db, $tableName);

    $db->query({ query => "DROP TABLE IF EXISTS `$tableName`" });
    create_table($db, $tableName, $createTableQuery);
}

# Execute any statement, going through create_table() for CREATE TABLE ones.
sub execute_statement {
    my ($db, $query) = @_;

    # The table name may be qualified with the database name.
    if ($query =~ /\A\s*CREATE\s+TABLE\s+(?:IF\s+NOT\s+EXISTS\s+)?(?:`?[^`\s.(]+`?\.)?`?([^`\s.(]+)`?/i) {
        create_table($db, $1, $query);
    } else {
        $db->query({ query => $query });
    }
}

# Errors are only caught if queries die on failure, i.e. if the handle was
# created with the die option.
sub check_handle {
    my ($db, $tableName) = @_;

    die "Cannot create table `$tableName`: the database handle must be created with the die option\n"
        if (!$db->{die});
}

1;
