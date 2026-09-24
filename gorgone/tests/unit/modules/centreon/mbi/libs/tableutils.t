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
sub make_db {
    my ($queries, $logs, $fail_for) = @_;

    my $logger = mock {} => (
        add => [
            writeLogError => sub {
                my ($self, $message) = @_;

                push @$logs, $message;
                return 1;
            }
        ]
    );

    return mock { logger => $logger } => (
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

        like($error, qr/Cannot recreate table `\Q$TABLE\E`/, "$server: the table name should be reported.");
        like($error, qr/orphaned InnoDB tablespace/, "$server: the likely cause should be reported.");
        like($error, qr{<datadir>/<database>/\Q$TABLE\E\.ibd}, "$server: the file to remove should be reported.");
        like($error, qr/Original error: \Q@{[db_error($errors{$server}, $CREATE)]}\E/,
            "$server: the original error should be kept.");
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

sub main {
    test_create_succeeds();
    test_orphaned_tablespace();
    test_other_create_error();
    test_drop_error();

    done_testing();
}
main;
