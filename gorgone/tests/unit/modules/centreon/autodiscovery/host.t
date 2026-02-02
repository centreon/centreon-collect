#!/usr/bin/perl

use strict;
use warnings;
use Test2::V0;
use Test2::Plugin::NoWarnings echo => 1;
use Test2::Tools::Compare qw{is like match};
use FindBin;
use lib "$FindBin::Bin/../../../../../";
use lib "$FindBin::Bin/../../../../../../perl-libs/lib/";
use Data::Dumper;
use gorgone::modules::centreon::autodiscovery::class;
use gorgone::modules::core::action::class;
use tests::unit::lib::mockLogger;

sub main {
    test_hdisco_can_start_job();
    done_testing();
}
sub test_hdisco_can_start_job {
    my $mock_logger = mock 'centreon::common::logger';

    my $gorgone = bless
        { logger => $mock_logger->class,
          global_timeout => 60 },
        "gorgone::modules::centreon::autodiscovery::class";

    my @tests = ({
        name => "first",
        arg    => {
            status   => 4,
            timezone => 'Europe/Paris',
            date     => '2026-01-29 15:35:12.000000',
            timeout  => 60 },
        result => 1,

    }, {
        name   => "undef mean take default timeout",
        arg    => {
            status   => 4,
            timezone => 'Europe/Paris',
            date     => '2026-01-29 15:35:12.000000',
            timeout  => undef },
        result => 1
    },
    );
    for my $test (@tests) {
        my $mock_gorgone = mock('gorgone::modules::centreon::autodiscovery::class' => (override => [
            _get_duration_since_last_exec => sub {
                return $test->{args}->{duration} // 300; # limit is at 130 secs
            }, update_job_information => sub {
                return 1;
        }]));
        my $res = $gorgone->hdisco_can_start_job(
            "timeout" => $test->{arg}->{timeout},
            "job"     => {
                job_id => 2,
                execution => {mode => 1},
                status         => $test->{arg}->{status},
                last_execution => {
                    "timezone_type" => 3,
                    'timezone'      => $test->{arg}->{timezone},
                    'date'          => $test->{arg}->{date},
                    'timezone_type' => 3
                }
            });
        is($res, $test->{result}, $test->{name});
    }

}

main();

