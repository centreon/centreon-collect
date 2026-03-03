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

# Safely drop and recreate a temporary table, handling cases where an orphaned
# InnoDB tablespace (.ibd file) may exist after a database migration (e.g.
# rsync of /var/lib/mysql). Without this, CREATE TABLE dies with errno 184
# "Tablespace already exists" and the ETL crashes.
sub recreate_table {
    my ($db, $logger, $tableName, $createTableQuery) = @_;

    $db->query({ query => "DROP TABLE IF EXISTS `$tableName`" });

    eval {
        $db->query({ query => $createTableQuery });
    };
    return unless $@;

    my $createError = $@;
    $logger->writeLog("WARNING",
        "Failed to create temp table `$tableName`: $createError Attempting recovery.");

    # The error handler disconnected the DB; the next query will auto-reconnect.
    # Check if the table structure still exists (DROP may have only partially
    # succeeded, or the table was left over from a previous crashed run).
    my $tableExists = eval {
        $db->query({ query => "SELECT 1 FROM `$tableName` LIMIT 0" });
        1;
    };

    if ($tableExists) {
        $logger->writeLog("INFO",
            "Table `$tableName` still exists, truncating and reusing it.");
        $db->query({ query => "TRUNCATE TABLE `$tableName`" });
        return;
    }

    # Table not in data dictionary but tablespace file may remain (orphaned).
    # Retry DROP + CREATE on the fresh connection.
    eval {
        $db->query({ query => "DROP TABLE IF EXISTS `$tableName`" });
    };
    eval {
        $db->query({ query => $createTableQuery });
    };
    return unless $@;

    die "Cannot create temp table `$tableName`. This may be caused by an orphaned "
        . "InnoDB tablespace (.ibd file) after a database migration. Please remove "
        . "the orphaned file from the MariaDB data directory and restart MariaDB. "
        . "Original error: $createError";
}

1;
