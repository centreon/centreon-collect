#!/usr/bin/perl

use strict;
use warnings;

use Test2::V0;
use Test2::Plugin::NoWarnings echo => 1;

use FindBin;
use lib "$FindBin::Bin/../../../../../../../";

# Mock required modules
BEGIN {
    package gorgone::standard::misc;
    sub backtick {}
    $INC{'gorgone/standard/misc.pm'} = 1;
}

use gorgone::modules::centreon::mbi::libs::Messages;
use gorgone::modules::centreon::mbi::etlworkers::import::main;

# Records the queries it is given, and fails the ones whose text matches.
package FakeConnection;
sub new {
    my ($class, %options) = @_;

    return bless({ queries => [], failOn => $options{failOn} }, $class);
}
sub query {
    my ($self, $options) = @_;

    push @{$self->{queries}}, $options->{query};
    die "mocked database failure\n" if (defined($self->{failOn}) && $options->{query} eq $self->{failOn});

    return 1;
}
package main;

sub build_worker {
    my (%options) = @_;

    return {
        messages => gorgone::modules::centreon::mbi::libs::Messages->new(),
        dbbi_centstorage_con => FakeConnection->new(failOn => $options{failOn})
    };
}

my @statements = (
    ['[COLLATION] first', 'ALTER TABLE `first` CONVERT TO CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci'],
    ['[COLLATION] second', 'ALTER TABLE `second` CONVERT TO CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci'],
    ['[COLLATION] third', 'ALTER TABLE `third` DEFAULT CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci']
);

sub test_every_statement_is_executed {
    my $etlwk = build_worker();

    gorgone::modules::centreon::mbi::etlworkers::import::main::sql(
        $etlwk,
        params => { db => 'centstorage', sql => \@statements }
    );

    is(scalar(@{$etlwk->{dbbi_centstorage_con}->{queries}}), 3, 'every statement of the action should be executed.');
    is(
        scalar(grep { $_->[0] eq 'I' } @{$etlwk->{messages}->getLogs()}),
        3,
        'every statement should be logged before being executed.'
    );
}

sub test_a_failure_stops_the_action_by_default {
    my $etlwk = build_worker(failOn => $statements[1]->[1]);

    my $error;
    eval {
        gorgone::modules::centreon::mbi::etlworkers::import::main::sql(
            $etlwk,
            params => { db => 'centstorage', sql => \@statements }
        );
        1;
    } or do { $error = $@; };

    like($error, qr/mocked database failure/, 'a failing statement should abort the action by default.');
    is(
        scalar(@{$etlwk->{dbbi_centstorage_con}->{queries}}),
        2,
        'the statements following a failure should not be executed.'
    );
}

sub test_a_failure_is_reported_when_the_action_is_opportunistic {
    my $etlwk = build_worker(failOn => $statements[1]->[1]);

    my $error;
    eval {
        gorgone::modules::centreon::mbi::etlworkers::import::main::sql(
            $etlwk,
            params => { db => 'centstorage', sql => \@statements, continue_on_error => 1 }
        );
        1;
    } or do { $error = $@; };

    is($error, undef, 'a failing statement should not abort an opportunistic action.');
    is(
        scalar(@{$etlwk->{dbbi_centstorage_con}->{queries}}),
        3,
        'the statements following a failure should still be executed.'
    );

    my @warnings = grep { $_->[1] =~ /failed: mocked database failure/ } @{$etlwk->{messages}->getLogs()};
    is(scalar(@warnings), 1, 'the failure should be reported.');
    like($warnings[0]->[1], qr/\[COLLATION\] second/, 'the report should name the statement that failed.');
}

sub main {
    test_every_statement_is_executed();
    test_a_failure_stops_the_action_by_default();
    test_a_failure_is_reported_when_the_action_is_opportunistic();
    done_testing();
}

main
