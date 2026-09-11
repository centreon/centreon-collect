#!/usr/bin/perl

use strict;
use warnings;

use Test2::V0;
use Test2::Plugin::NoWarnings echo => 1;
use FindBin;
use lib "$FindBin::Bin/../../../../../../";
use gorgone::modules::centreon::mbi::libs::TableUtils;

my $TABLE    = 'mod_bi_tmp_foo';
my $CREATE   = "CREATE TABLE `$TABLE` (`id` INT) ENGINE=INNODB";
my $DROP     = "DROP TABLE IF EXISTS `$TABLE`";
my $EXISTS   = "SELECT 1 FROM `$TABLE` LIMIT 0";
my $TRUNCATE = "TRUNCATE TABLE `$TABLE`";

my $TABLESPACE_ERROR = "DBD::mysql::st execute failed: Tablespace for table '$TABLE' exists. errno: 184\n";

# Build a fake DB handle recording every query it is asked to run.
# $fail_for is an optional callback receiving the query and its 1-based call
# index; when it returns a message, the query dies with it, the way
# gorgone::class::db behaves once built with `die => 1`.
sub make_db {
    my ($queries, $fail_for) = @_;

    return mock {} => (
        add => [
            query => sub {
                my ($self, $options) = @_;

                push @$queries, $options->{query};
                my $error = $fail_for ? $fail_for->($options->{query}, scalar(@$queries)) : undef;
                die $error if defined($error);

                return 0;
            }
        ]
    );
}

# Build a fake logger recording every writeLog() call.
sub make_logger {
    my ($logs) = @_;

    return mock {} => (
        add => [
            writeLog => sub {
                my ($self, $severity, $message) = @_;

                push @$logs, { severity => $severity, message => $message };
                return 1;
            }
        ]
    );
}

sub severities {
    my ($logs) = @_;

    return [map { $_->{severity} } @$logs];
}

# Nominal case: DROP then CREATE both succeed, no recovery query is needed.
sub test_create_succeeds {
    my (@queries, @logs);
    my $db = make_db(\@queries);

    ok(lives { gorgone::modules::centreon::mbi::libs::TableUtils::recreate_table($db, make_logger(\@logs), $TABLE, $CREATE) },
        'recreate_table should not die when the table is created.');

    is(\@queries, [$DROP, $CREATE], 'only DROP and CREATE should be executed.');
    is(\@logs, [], 'nothing should be logged when no recovery is needed.');
}

# CREATE fails but the table is still in the data dictionary: it must be reused
# through a TRUNCATE, and CREATE must not be replayed.
sub test_recovery_when_table_still_exists {
    my (@queries, @logs);
    my $db = make_db(\@queries, sub {
        my ($query) = @_;
        return $TABLESPACE_ERROR if $query eq $CREATE;
        return undef;
    });

    ok(lives { gorgone::modules::centreon::mbi::libs::TableUtils::recreate_table($db, make_logger(\@logs), $TABLE, $CREATE) },
        'recreate_table should not die when the existing table can be truncated.');

    is(\@queries, [$DROP, $CREATE, $EXISTS, $TRUNCATE],
        'the existing table should be truncated instead of being created again.');
    is(severities(\@logs), ['WARNING', 'INFO'], 'the failure and the reuse should both be logged.');
    like($logs[1]->{message}, qr/truncating and reusing/, 'the reuse of the table should be logged.');
}

# CREATE fails and the table is not in the data dictionary (orphaned tablespace):
# DROP + CREATE must be replayed on the reconnected handle, without TRUNCATE.
sub test_recovery_when_table_is_missing {
    my (@queries, @logs);
    my $db = make_db(\@queries, sub {
        my ($query, $index) = @_;
        return $TABLESPACE_ERROR if $query eq $CREATE && $index == 2;
        return "Table '$TABLE' doesn't exist\n" if $query eq $EXISTS;
        return undef;
    });

    ok(lives { gorgone::modules::centreon::mbi::libs::TableUtils::recreate_table($db, make_logger(\@logs), $TABLE, $CREATE) },
        'recreate_table should not die when the retried CREATE succeeds.');

    is(\@queries, [$DROP, $CREATE, $EXISTS, $DROP, $CREATE],
        'DROP and CREATE should be retried and TRUNCATE should not be executed.');
    is(severities(\@logs), ['WARNING'], 'only the initial failure should be logged.');
    like($logs[0]->{message}, qr/Attempting recovery/, 'the recovery attempt should be logged.');
}

# A failing DROP during the retry must not abort the recovery.
sub test_recovery_ignores_failing_drop {
    my (@queries, @logs);
    my $db = make_db(\@queries, sub {
        my ($query, $index) = @_;
        return $TABLESPACE_ERROR if $query eq $CREATE && $index == 2;
        return "Table '$TABLE' doesn't exist\n" if $query eq $EXISTS;
        return "Unknown table '$TABLE'\n" if $query eq $DROP && $index == 4;
        return undef;
    });

    ok(lives { gorgone::modules::centreon::mbi::libs::TableUtils::recreate_table($db, make_logger(\@logs), $TABLE, $CREATE) },
        'recreate_table should not die when the retried DROP fails but the CREATE succeeds.');

    is(\@queries, [$DROP, $CREATE, $EXISTS, $DROP, $CREATE], 'the retried CREATE should still be executed.');
}

# Both CREATE attempts fail: recreate_table must die with an actionable message
# carrying the original error.
sub test_dies_when_recovery_fails {
    my (@queries, @logs);
    my $db = make_db(\@queries, sub {
        my ($query) = @_;
        return $TABLESPACE_ERROR if $query eq $CREATE;
        return "Table '$TABLE' doesn't exist\n" if $query eq $EXISTS;
        return undef;
    });

    my $error = dies { gorgone::modules::centreon::mbi::libs::TableUtils::recreate_table($db, make_logger(\@logs), $TABLE, $CREATE) };

    like($error, qr/Cannot create temp table `\Q$TABLE\E`/, 'the table name should be reported.');
    like($error, qr/orphaned\s+InnoDB tablespace/, 'the likely cause should be reported.');
    like($error, qr/errno: 184/, 'the original error should be kept.');
    is(\@queries, [$DROP, $CREATE, $EXISTS, $DROP, $CREATE], 'no query should run after the second CREATE failure.');
    is(severities(\@logs), ['WARNING'], 'only the initial failure should be logged.');
}

sub main {
    test_create_succeeds();
    test_recovery_when_table_still_exists();
    test_recovery_when_table_is_missing();
    test_recovery_ignores_failing_drop();
    test_dies_when_recovery_fails();

    done_testing();
}
main;
