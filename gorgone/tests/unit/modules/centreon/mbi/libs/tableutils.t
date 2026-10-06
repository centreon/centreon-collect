#!/usr/bin/perl

use strict;
use warnings;

use Test2::V0;
use Test2::Plugin::NoWarnings echo => 1;
use FindBin;
use lib "$FindBin::Bin/../../../../../../";
use gorgone::modules::centreon::mbi::libs::TableUtils;

my $TABLE  = 'mod_bi_tmp_foo';
my $CREATE = "CREATE TABLE `$TABLE` (`id` INT) ENGINE=INNODB";
my $DROP   = "DROP TABLE IF EXISTS `$TABLE`";

# Format a database error the way gorgone::class::db dies with it once built
# with `die => 1`: the server message, then the failing query on its own line.
sub db_error {
    my ($message, $query) = @_;

    return "SQL error: $message (caller: gorgone::modules::centreon::mbi::libs::TableUtils:TableUtils.pm:1)\n"
        . "Query: $query\n";
}

# Build a fake DB handle recording every query it is asked to run, with a
# logger recording every error it logs.
# $fail_for is an optional callback receiving the query; when it returns a
# message, the query dies with it, as gorgone::class::db does once it gave up
# (it retries a failed query by itself before dying).
# %options can override the handle fields (e.g. `die => 0`, `logger => undef`).
sub make_db {
    my ($queries, $logs, $fail_for, %options) = @_;

    my $logger = mock {} => (
        add => [
            writeLogError => sub {
                my ($self, $message) = @_;

                push @$logs, $message;
                return 1;
            }
        ]
    );

    return mock { die => 1, logger => $logger, %options } => (
        add => [
            query => sub {
                my ($self, $options) = @_;

                push @$queries, $options->{query};
                my $error = $fail_for ? $fail_for->($options->{query}) : undef;
                die db_error($error, $options->{query}) if defined($error);

                return 0;
            }
        ]
    );
}

sub fail_create_with {
    my ($error) = @_;

    return sub {
        my ($query) = @_;
        return $query eq $CREATE ? $error : undef;
    };
}

# Nominal case: DROP then CREATE both succeed.
sub test_create_succeeds {
    my (@queries, @logs);
    my $db = make_db(\@queries, \@logs);

    ok(lives { gorgone::modules::centreon::mbi::libs::TableUtils::recreate_table($db, $TABLE, $CREATE) },
        'recreate_table should not die when the table is created.');
    is(\@queries, [$DROP, $CREATE], 'only DROP and CREATE should be executed.');
    is(\@logs, [], 'nothing should be logged.');
}

# CREATE fails because of an orphaned tablespace: recreate_table must die with
# an actionable message carrying the original error, and log it.
sub test_orphaned_tablespace {
    my %errors = (
        'MariaDB' => "Can't create table `centreon_storage`.`$TABLE` (errno: 184 \"Tablespace already exists\")",
        'MariaDB (French)' => "Ne peut créer la table `centreon_storage`.`$TABLE` (Errcode: 184 \"Tablespace already exists\")",
        'MySQL 5.7' => "Tablespace for table '`centreon_storage`.`$TABLE`' exists. Please DISCARD the tablespace before IMPORT.",
        'MySQL 8.0' => "Tablespace '`centreon_storage`.`$TABLE`' exists."
    );

    for my $server (sort keys %errors) {
        my (@queries, @logs);
        my $db = make_db(\@queries, \@logs, fail_create_with($errors{$server}));

        my $error = dies { gorgone::modules::centreon::mbi::libs::TableUtils::recreate_table($db, $TABLE, $CREATE) };

        like($error, qr/Cannot create table `\Q$TABLE\E`/, "$server: the table name should be reported.");
        like($error, qr/orphaned InnoDB tablespace/, "$server: the likely cause should be reported.");
        like($error, qr{<datadir>/<database>/\Q$TABLE\E\.ibd}, "$server: the file to remove should be reported.");
        like($error, qr/Original error: SQL error: \Q$errors{$server}\E\n\z/,
            "$server: the original error should be kept, without caller nor query.");
        unlike($error, qr/Query:/, "$server: the failing query should not be reported.");
        is(\@logs, [$error], "$server: the error should be logged.");
        is(\@queries, [$DROP, $CREATE], "$server: recreate_table should not retry by itself.");
    }
}

# Any other CREATE failure must be rethrown unchanged, without the orphaned
# tablespace hint, even when the failing query mentions a tablespace.
sub test_other_create_error {
    my %cases = (
        'denied' => "CREATE command denied to user 'centreonbi'",
        'table exists' => "Table '$TABLE' already exists"
    );
    my $create_with_tablespace = "CREATE TABLE `$TABLE` (`id` INT) TABLESPACE innodb_file_per_table, COMMENT 'exists'";

    for my $case (sort keys %cases) {
        my (@queries, @logs);
        my $db = make_db(\@queries, \@logs, sub {
            my ($query) = @_;
            return $query eq $create_with_tablespace ? $cases{$case} : undef;
        });

        my $error = dies {
            gorgone::modules::centreon::mbi::libs::TableUtils::recreate_table($db, $TABLE, $create_with_tablespace)
        };

        is($error, db_error($cases{$case}, $create_with_tablespace), "$case: the original error should be rethrown unchanged.");
        is(\@logs, [], "$case: nothing should be logged.");
    }
}

# A DROP failure must be rethrown unchanged, and CREATE must not run. A missing
# tablespace is not the orphaned case and must not get its hint.
sub test_drop_error {
    my (@queries, @logs);
    my $original = "Tablespace is missing for table `centreon_storage`.`$TABLE`";
    my $db = make_db(\@queries, \@logs, sub {
        my ($query) = @_;
        return $query eq $DROP ? $original : undef;
    });

    my $error = dies { gorgone::modules::centreon::mbi::libs::TableUtils::recreate_table($db, $TABLE, $CREATE) };

    is($error, db_error($original, $DROP), 'the original error should be rethrown unchanged.');
    is(\@queries, [$DROP], 'CREATE should not be executed.');
    is(\@logs, [], 'nothing should be logged.');
}

# The orphaned tablespace hint must still be given when the handle has no
# logger.
sub test_orphaned_tablespace_without_logger {
    my (@queries, @logs);
    my $db = make_db(\@queries, \@logs, fail_create_with("Tablespace '`centreon_storage`.`$TABLE`' exists."),
        logger => undef);

    my $error = dies { gorgone::modules::centreon::mbi::libs::TableUtils::recreate_table($db, $TABLE, $CREATE) };

    like($error, qr/orphaned InnoDB tablespace/, 'the likely cause should be reported.');
}

# A message only mentioning a word starting with "exists" is not the orphaned
# tablespace case.
sub test_exists_as_word_prefix {
    my (@queries, @logs);
    my $original = "Tablespace is missing for table `centreon_storage`.`exists_foo`";
    my $db = make_db(\@queries, \@logs, fail_create_with($original));

    my $error = dies { gorgone::modules::centreon::mbi::libs::TableUtils::recreate_table($db, $TABLE, $CREATE) };

    is($error, db_error($original, $CREATE), 'the original error should be rethrown unchanged.');
}

# Errors of a handle that does not die on failure would be silently ignored:
# such a handle must be refused before running any query.
sub test_handle_not_dying {
    my (@queries, @logs);
    my $db = make_db(\@queries, \@logs, undef, die => 0);

    my $error = dies { gorgone::modules::centreon::mbi::libs::TableUtils::recreate_table($db, $TABLE, $CREATE) };

    like($error, qr/must be created with the die option/, 'the handle should be refused.');
    is(\@queries, [], 'no query should be executed.');

    for my $function ('create_table', 'execute_statement') {
        my @function_queries;
        my $db = make_db(\@function_queries, \@logs, undef, die => 0);
        my $call = \&{"gorgone::modules::centreon::mbi::libs::TableUtils::$function"};
        my @args = $function eq 'create_table' ? ($db, $TABLE, $CREATE) : ($db, $CREATE);

        like(dies { $call->(@args) }, qr/must be created with the die option/, "$function should refuse the handle.");
        is(\@function_queries, [], "$function should not execute any query.");
    }
}

# create_table() only creates the table, with the same hint on an orphaned
# tablespace.
sub test_create_table {
    my (@queries, @logs);
    my $db = make_db(\@queries, \@logs, fail_create_with("Tablespace '`centreon_storage`.`$TABLE`' exists."));

    my $error = dies { gorgone::modules::centreon::mbi::libs::TableUtils::create_table($db, $TABLE, $CREATE) };

    like($error, qr/Cannot create table `\Q$TABLE\E`: an orphaned InnoDB tablespace/, 'the hint should be given.');
    is(\@queries, [$CREATE], 'the table should not be dropped.');
}

# execute_statement() gives the hint for CREATE TABLE statements only, naming
# the table found in the statement.
sub test_execute_statement {
    my $tablespace_error = "Tablespace '`centreon_storage`.`mod_bi_foo`' exists.";
    my %statements = (
        "CREATE TABLE `mod_bi_foo` (\n  `id` int(11) NOT NULL\n) ENGINE=InnoDB" => 'mod_bi_foo',
        " create table if not exists mod_bi_foo (`id` INT)" => 'mod_bi_foo',
        "CREATE TABLE `centreon_storage`.`mod_bi_foo` (`id` INT)" => 'mod_bi_foo',
        "CREATE TABLE centreon_storage.mod_bi_foo (`id` INT)" => 'mod_bi_foo'
    );

    for my $statement (sort keys %statements) {
        my (@queries, @logs);
        my $db = make_db(\@queries, \@logs, sub { return $tablespace_error; });

        my $error = dies { gorgone::modules::centreon::mbi::libs::TableUtils::execute_statement($db, $statement) };

        like($error, qr/Cannot create table `\Q$statements{$statement}\E`: an orphaned InnoDB tablespace/,
            "the hint should name the created table ($statement).");
        is(\@queries, [$statement], "only the statement should be executed ($statement).");
    }

    my (@queries, @logs);
    my $import = "ALTER TABLE `mod_bi_foo` IMPORT TABLESPACE";
    my $db = make_db(\@queries, \@logs, sub { return $tablespace_error; });

    my $error = dies { gorgone::modules::centreon::mbi::libs::TableUtils::execute_statement($db, $import) };

    is($error, db_error($tablespace_error, $import), 'other statements should fail with their original error.');
    is(\@queries, [$import], 'other statements should be executed as is.');
    is(\@logs, [], 'nothing should be logged for other statements.');
}

# A partitioned table has one tablespace file per partition: the hint must
# point to them.
sub test_orphaned_tablespace_partitioned {
    my %statements = (
        'built' => "CREATE TABLE `$TABLE` (`time_id` INT) ENGINE=InnoDB PARTITION BY RANGE(`time_id`) "
            . "(PARTITION p20260101 VALUES LESS THAN (1767225600))",
        # As returned by SHOW CREATE TABLE.
        'dumped' => "CREATE TABLE `$TABLE` (\n  `time_id` int(11) NOT NULL\n) ENGINE=InnoDB DEFAULT CHARSET=utf8mb3\n"
            . "/*!50100 PARTITION BY RANGE (`time_id`)\n(PARTITION `p20260101` VALUES LESS THAN (1767225600) ENGINE = InnoDB) */"
    );

    for my $case (sort keys %statements) {
        my (@queries, @logs);
        my $db = make_db(\@queries, \@logs, sub { return "Tablespace '`centreon_storage`.`$TABLE`' exists."; });

        my $error = dies {
            gorgone::modules::centreon::mbi::libs::TableUtils::create_table($db, $TABLE, $statements{$case})
        };

        like($error, qr{<datadir>/<database>/\Q$TABLE\E#P#\*\.ibd \(or \Q$TABLE\E#p#\*\.ibd on MySQL 8\.0\)},
            "$case: the partition files should be reported.");
        unlike($error, qr{\Q$TABLE\E\.ibd}, "$case: a single table file should not be reported.");
    }
}

# Only the server message is matched, not the caller appended to it by
# gorgone::class::db.
sub test_caller_not_matched {
    my @queries;
    my $original = "SQL error: Lock wait timeout exceeded (caller: Tablespace::Exists:/opt/tablespace/exists.pm:1)\n"
        . "Query: $CREATE\n";
    my $db = mock { die => 1 } => (
        add => [
            query => sub {
                my ($self, $options) = @_;

                push @queries, $options->{query};
                die $original;
            }
        ]
    );

    my $error = dies { gorgone::modules::centreon::mbi::libs::TableUtils::create_table($db, $TABLE, $CREATE) };

    is($error, $original, 'the original error should be rethrown unchanged.');
}

# The hint is only given for CREATE errors: a DROP failing with a message
# looking like the orphaned tablespace one must be rethrown unchanged.
sub test_drop_error_looking_like_orphaned_tablespace {
    my (@queries, @logs);
    my $original = "Tablespace '`centreon_storage`.`$TABLE`' exists.";
    my $db = make_db(\@queries, \@logs, sub {
        my ($query) = @_;
        return $query eq $DROP ? $original : undef;
    });

    my $error = dies { gorgone::modules::centreon::mbi::libs::TableUtils::recreate_table($db, $TABLE, $CREATE) };

    is($error, db_error($original, $DROP), 'the original error should be rethrown unchanged.');
    is(\@logs, [], 'nothing should be logged.');
}

sub main {
    test_create_succeeds();
    test_orphaned_tablespace();
    test_other_create_error();
    test_drop_error();
    test_orphaned_tablespace_without_logger();
    test_exists_as_word_prefix();
    test_handle_not_dying();
    test_create_table();
    test_execute_statement();
    test_orphaned_tablespace_partitioned();
    test_caller_not_matched();
    test_drop_error_looking_like_orphaned_tablespace();

    done_testing();
}
main;
