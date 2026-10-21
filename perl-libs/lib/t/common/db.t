#!/usr/bin/perl
use strict;
use warnings;

use Test2::V0;
use FindBin;
use lib "$FindBin::Bin/../../";
use centreon::common::db;
use centreon::common::logger;

# DBI->connect is mocked: we only check the DSN given to the driver.
sub connect_dsn {
    my (%options) = @_;

    my @calls;
    my $mock = mock 'DBI' => (
        override => [ connect => sub { shift; push @calls, [@_]; return bless({}, 'fakeDbh'); } ]
    );
    my $db = centreon::common::db->new(logger => centreon::common::logger->new(), db => 'centreon', %options);
    is($db->connect(), 0, 'connection should succeed.');
    return $calls[0]->[0];
}

sub test_dsn {
    # conf.pm without db_type and db_dsn_options: same DSN as before
    is(connect_dsn(host => 'localhost:3306', port => undef, type => undef, dsn_options => undef),
        'DBI:mysql:centreon:localhost:3306:3306', 'old conf.pm should keep the legacy DSN.');
    is(connect_dsn(type => 'MariaDB', host => 'db', port => undef),
        'DBI:MariaDB:centreon:db:3306', 'MariaDB should use the default port.');
    is(connect_dsn(type => 'MariaDB', host => 'db:3306', port => undef,
            dsn_options => 'mariadb_ssl=1;mariadb_ssl_ca_file=/etc/ssl/ca.pem;mariadb_ssl_verify_server_cert=1'),
        'DBI:MariaDB:database=centreon;host=db:3306;port=3306;mariadb_ssl=1;mariadb_ssl_ca_file=/etc/ssl/ca.pem;mariadb_ssl_verify_server_cert=1',
        'dsn_options should be added to the DSN.');
    is(connect_dsn(type => 'SQLite', db => 'dbname=/tmp/test.sdb', dsn_options => 'ignored=1'),
        'DBI:SQLite:dbname=/tmp/test.sdb', 'SQLite should ignore dsn_options.');
}

sub test_kill {
    package fakeDbh {
        sub do { my ($self, $query) = @_; push @{$self->{queries}}, $query; return 1; }
    }
    for my $test ([ 'mysql', 'mysql_thread_id' ], [ 'MariaDB', 'mariadb_thread_id' ]) {
        my $db = centreon::common::db->new(logger => centreon::common::logger->new(), type => $test->[0]);
        $db->{instance} = bless({ $test->[1] => 42, queries => [] }, 'fakeDbh');
        $db->kill();
        is($db->{instance}->{queries}, [ 'KILL QUERY 42' ], "kill should use $test->[1].");
    }
}

test_dsn();
test_kill();
done_testing();
