#!/usr/bin/perl

use strict;
use warnings;

use Test2::V0;
use Test2::Plugin::NoWarnings echo => 1;
use Test2::Tools::Mock;

use FindBin;
use lib "$FindBin::Bin/../../../../../../../";

# Mock required modules
BEGIN {
    package gorgone::modules::centreon::mbi::libs::bi::Time;
    sub new { return bless({}, __PACKAGE__); }
    $INC{'gorgone/modules/centreon/mbi/libs/bi/Time.pm'} = 1;

    package gorgone::modules::centreon::mbi::libs::bi::LiveService;
    sub new { return bless({}, __PACKAGE__); }
    $INC{'gorgone/modules/centreon/mbi/libs/bi/LiveService.pm'} = 1;

    package gorgone::modules::centreon::mbi::libs::bi::MySQLTables;
    sub new { return bless({}, __PACKAGE__); }
    $INC{'gorgone/modules/centreon/mbi/libs/bi/MySQLTables.pm'} = 1;

    package gorgone::modules::centreon::mbi::libs::Utils;
    sub new { return bless({}, __PACKAGE__); }
    $INC{'gorgone/modules/centreon/mbi/libs/Utils.pm'} = 1;

    package gorgone::standard::constants;
    use Exporter 'import';
    use constant GORGONE_MODULE_CENTREON_MBIETL_PROGRESS => 1;
    our @EXPORT_OK = qw(GORGONE_MODULE_CENTREON_MBIETL_PROGRESS);
    our %EXPORT_TAGS = ( 'all' => [qw(GORGONE_MODULE_CENTREON_MBIETL_PROGRESS)] );
    $INC{'gorgone/standard/constants.pm'} = 1;
}

use gorgone::modules::centreon::mbi::etl::perfdata::main;

my @calls = ();
my $mock = mock 'gorgone::modules::centreon::mbi::etl::perfdata::main' => (
    override => [
        'deleteEntriesForRebuild' => sub { my ($etl, %p) = @_; push @calls, { method => 'deleteEntriesForRebuild', %p }; },
        'emptyTableForRebuild'    => sub { my ($etl, %p) = @_; push @calls, { method => 'emptyTableForRebuild', %p }; }
    ],
);

subtest 'Focus: mod_bi_metricdailyvalue' => sub {
    my $daily_start = '2026-01-01';
    my $daily_end   = '2026-01-31';
    my $periods = {
        'perfdata.daily'  => { start => $daily_start, end => $daily_end },
        'perfdata.hourly' => { start => $daily_start, end => $daily_end }
    };

    subtest 'Purge mode (noPurge = 0)' => sub {
        @calls = ();
        my $etl = {
            run => {
                options => { nopurge => 0, month_only => 0, centile_only => 0 },
                etlProperties => { 'perfdata.granularity' => 'day' }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods);

        my @daily_calls = grep { ($_->{name} // '') eq 'mod_bi_metricdailyvalue' } @calls;

        is(scalar @daily_calls, 1, 'There must be exactly 1 call for this table');
        my $call = $daily_calls[0];

        is($call->{method}, 'emptyTableForRebuild', 'The method must be emptyTableForRebuild');
        is($call->{column}, 'time_id', 'The argument column must be time_id');
        is($call->{start}, $daily_start, 'The argument start must be correct');
        is($call->{end}, $daily_end, 'The argument end must be correct');
    };

    subtest 'No-Purge mode (noPurge = 1)' => sub {
        @calls = ();
        my $etl = {
            run => {
                options => { nopurge => 1, month_only => 0, centile_only => 0 },
                etlProperties => { 'perfdata.granularity' => 'day' }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods);

        my @daily_calls = grep { ($_->{name} // '') eq 'mod_bi_metricdailyvalue' } @calls;

        is(scalar @daily_calls, 1, 'There must be exactly 1 call for this table');

        my $call = $daily_calls[0];

        is($call->{method}, 'deleteEntriesForRebuild', 'The method must be deleteEntriesForRebuild');
        is($call->{start}, $daily_start, 'The argument start must be correct');
        is($call->{end}, $daily_end, 'The argument end must be correct');
        ok(!exists $call->{column}, 'The argument column must not be present for deleteEntriesForRebuild');
    };
};

subtest 'Focus: mod_bi_metrichourlyvalue' => sub {
    my $hourly_start = '2026-01-01 00:00:00';
    my $hourly_end   = '2026-01-01 23:59:59';
    my $periods = {
        'perfdata.daily'  => { start => '2026-01-01', end => '2026-01-01' },
        'perfdata.hourly' => { start => $hourly_start, end => $hourly_end }
    };

    subtest 'Purge mode (noPurge = 0)' => sub {
        @calls = ();

        my $etl = {
            run => {
                options => { nopurge => 0, month_only => 0, centile_only => 0 },
                etlProperties => { 'perfdata.granularity' => 'hour' }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods);

        my @hourly_calls = grep { ($_->{name} // '') eq 'mod_bi_metrichourlyvalue' } @calls;
        is(scalar @hourly_calls, 1, 'There must be exactly 1 call in Purge mode');

        my $call = $hourly_calls[0];
        is($call->{method}, 'emptyTableForRebuild', 'Expected method: emptyTableForRebuild');
        is($call->{start}, $hourly_start, 'Start date is correct');
        is($call->{end}, $hourly_end, 'End date is correct');
        is($call->{column}, 'time_id', 'The argument column must be time_id');

    };

    # In noPurge mode it has a double condition: not 'hour' AND not 'day'
    subtest 'No-Purge mode (noPurge = 1)' => sub {
        @calls = ();
        my $etl = {
            run => {
                options => { nopurge => 1, month_only => 0, centile_only => 0 },
                etlProperties => { 'perfdata.granularity' => 'all' } # set to 'all' so it is active
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods);

        my @hourly_calls = grep { ($_->{name} // '') eq 'mod_bi_metrichourlyvalue' } @calls;
        is(scalar @hourly_calls, 1, 'There must be exactly 1 call in No-Purge mode');

        my $call = $hourly_calls[0];
        is($call->{method}, 'deleteEntriesForRebuild', 'Expected method: deleteEntriesForRebuild');
        is($call->{start}, $hourly_start, 'Start date is correct');
        is($call->{end}, $hourly_end, 'End date is correct');
        ok(!exists $call->{column}, 'The argument column must not be present for deleteEntriesForRebuild');
    };
};

subtest 'Focus: mod_bi_metricmonthcapacity' => sub {
    my $daily_start = '2026-01-15';
    my $daily_end   = '2026-01-20'; # Same month as start
    my $first_day  = '2026-01-01';

    my $periods = {
        'perfdata.daily'  => { start => $daily_start, end => $daily_end },
        'perfdata.hourly' => { start => $daily_start, end => $daily_end }
    };

    # Should perform a FULL empty (without start/end)
    subtest 'Purge mode (noPurge = 0)' => sub {
        @calls = ();
        my $etl = {
            run => {
                options => { nopurge => 0, month_only => 0, centile_only => 0 },
                etlProperties => { 'perfdata.granularity' => 'day' }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods);

        my @cap_calls = grep { ($_->{name} // '') eq 'mod_bi_metricmonthcapacity' } @calls;
        is(scalar @cap_calls, 1, 'There must be exactly 1 call in Purge mode');

        my $call = $cap_calls[0];
        is($call->{method}, 'emptyTableForRebuild', 'Expected method: emptyTableForRebuild');
        is($call->{column}, 'time_id', 'column argument present');

        # CRUCIAL CHECK: no dates for this table in Purge mode
        ok(!exists $call->{start}, 'The argument start must NOT exist (Full Empty)');
        ok(!exists $call->{end},   'The argument end must NOT exist (Full Empty)');
    };

    # Should do NOTHING because start and end are in January
    subtest 'No-Purge mode (noPurge = 1) - Same month' => sub {
        @calls = ();
        my $etl = {
            run => {
                options => { nopurge => 1, month_only => 0, centile_only => 0 },
                etlProperties => { 'perfdata.granularity' => 'day' }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods);

        my @cap_calls = grep { ($_->{name} // '') eq 'mod_bi_metricmonthcapacity' } @calls;
        is(scalar @cap_calls, 0, 'In No-Purge, if start/end are in the same month, capacity must not be purged');
    };

    # Should perform a delete using the first day of the month
    subtest 'No-Purge mode (noPurge = 1) - Different months' => sub {
        @calls = ();
        my $periods_diff = {
            'perfdata.daily'  => { start => '2026-01-15', end => '2026-02-05' }, # January to February
            'perfdata.hourly' => { start => '2026-01-15', end => '2026-02-05' }
        };
        my $etl = {
            run => {
                options => { nopurge => 1, month_only => 0, centile_only => 0 },
                etlProperties => { 'perfdata.granularity' => 'day' }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods_diff);

        my @cap_calls = grep { ($_->{name} // '') eq 'mod_bi_metricmonthcapacity' } @calls;
        is(scalar @cap_calls, 1, 'In No-Purge, if months differ, purge must be called');

        my $call = $cap_calls[0];
        is($call->{method}, 'deleteEntriesForRebuild', 'Expected method: deleteEntriesForRebuild');
        is($call->{start}, '2026-01-01', 'Should target the start of the month (2026-01-01)');
        is($call->{end},   '2026-02-05', 'Should target the actual end of the period');
    };
};

subtest 'Focus: mod_bi_metriccentiledailyvalue' => sub {
    my $daily_start = '2026-01-01';
    my $daily_end   = '2026-01-31';
    my $periods = {
        'perfdata.daily'  => { start => $daily_start, end => $daily_end },
        'perfdata.hourly' => { start => $daily_start, end => $daily_end }
    };

    subtest 'Activation via etlProperties' => sub {
        @calls = ();
        # Case where the property centile.day is '0' or absent
        my $etl = {
            run => {
                options => { nopurge => 0, month_only => 0, centile_only => 0, no_centile => 0 },
                etlProperties => {
                    'perfdata.granularity' => 'day',
                    'centile.day' => '0' # Disabled here
                }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods);

        my @centile_calls = grep { ($_->{name} // '') eq 'mod_bi_metriccentiledailyvalue' } @calls;
        is(scalar @centile_calls, 0, 'The table must not be processed if centile.day=0');
    };

    subtest 'Purge mode (noPurge = 0)' => sub {
        @calls = ();
        my $etl = {
            run => {
                options => { nopurge => 0, month_only => 0, centile_only => 0, no_centile => 0 },
                etlProperties => {
                    'perfdata.granularity' => 'day',
                    'centile.day' => '1'
                }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods);

        my @centile_calls = grep { ($_->{name} // '') eq 'mod_bi_metriccentiledailyvalue' } @calls;
        is(scalar @centile_calls, 1, 'There must be exactly 1 call in Purge mode');

        my $call = $centile_calls[0];
        is($call->{method}, 'emptyTableForRebuild', 'Expected method: emptyTableForRebuild');
        is($call->{start}, $daily_start, 'Start date is correct');
        is($call->{end}, $daily_end, 'End date is correct');
    };

    subtest 'No-Purge mode (noPurge = 1)' => sub {
        @calls = ();
        my $etl = {
            run => {
                options => { nopurge => 1, month_only => 0, centile_only => 0, no_centile => 0 },
                etlProperties => {
                    'perfdata.granularity' => 'day',
                    'centile.day' => '1'
                }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods);

        my @centile_calls = grep { ($_->{name} // '') eq 'mod_bi_metriccentiledailyvalue' } @calls;
        is(scalar @centile_calls, 1, 'There must be exactly 1 call in No-Purge mode');

        my $call = $centile_calls[0];
        is($call->{method}, 'deleteEntriesForRebuild', 'Expected method: deleteEntriesForRebuild');
        is($call->{start}, $daily_start, 'Start date is correct');
    };
};

subtest 'Focus: mod_bi_metriccentileweeklyvalue' => sub {
    my $daily_start = '2026-01-01';
    my $daily_end   = '2026-01-31';
    my $periods = {
        'perfdata.daily'  => { start => $daily_start, end => $daily_end },
        'perfdata.hourly' => { start => $daily_start, end => $daily_end }
    };

    subtest 'Activation via etlProperties' => sub {
        @calls = ();
        my $etl = {
            run => {
                options => { nopurge => 0, month_only => 0, centile_only => 0, no_centile => 0 },
                etlProperties => {
                    'perfdata.granularity' => 'day',
                    'centile.week' => '0' # Disable week here
                }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods);

        my @weekly_calls = grep { ($_->{name} // '') eq 'mod_bi_metriccentileweeklyvalue' } @calls;
        is(scalar @weekly_calls, 0, 'The table must not be processed if centile.week=0');
    };

    subtest 'Purge mode (noPurge = 0)' => sub {
        @calls = ();
        my $etl = {
            run => {
                options => { nopurge => 0, month_only => 0, centile_only => 0, no_centile => 0 },
                etlProperties => {
                    'perfdata.granularity' => 'day',
                    'centile.week' => '1'
                }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods);

        my @weekly_calls = grep { ($_->{name} // '') eq 'mod_bi_metriccentileweeklyvalue' } @calls;
        is(scalar @weekly_calls, 1, 'There must be exactly 1 call in Purge mode');

        my $call = $weekly_calls[0];
        is($call->{method}, 'emptyTableForRebuild', 'Should use emptyTableForRebuild');
        is($call->{start}, $daily_start, 'Uses the daily_start date');
    };

    subtest 'No-Purge mode (noPurge = 1)' => sub {
        @calls = ();
        my $etl = {
            run => {
                options => { nopurge => 1, month_only => 0, centile_only => 0, no_centile => 0 },
                etlProperties => {
                    'perfdata.granularity' => 'day',
                    'centile.week' => '1'
                }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods);

        my @weekly_calls = grep { ($_->{name} // '') eq 'mod_bi_metriccentileweeklyvalue' } @calls;
        is(scalar @weekly_calls, 1, 'There must be exactly 1 call in No-Purge mode');

        my $call = $weekly_calls[0];
        is($call->{method}, 'deleteEntriesForRebuild', 'Should use deleteEntriesForRebuild');
        is($call->{start}, $daily_start, 'Uses the daily_start date');
    };
};

subtest 'Focus: mod_bi_metriccentilemonthlyvalue' => sub {
    my $daily_start = '2026-01-15';
    my $daily_end   = '2026-02-05'; # Month change to enable purge in No-Purge
    my $first_day   = '2026-01-01';

    my $periods = {
        'perfdata.daily'  => { start => $daily_start, end => $daily_end },
        'perfdata.hourly' => { start => $daily_start, end => $daily_end }
    };

    # SPECIAL CASE (MonthOnly + CentileOnly)
    # In this case, the original ignores everything else and makes a direct call
    subtest 'Special case: MonthOnly + CentileOnly' => sub {
        @calls = ();
        my $etl = {
            run => {
                options => { month_only => 1, centile_only => 1, nopurge => 0 },
                etlProperties => { 'centile.month' => '1' }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods);

        my @monthly_calls = grep { ($_->{name} // '') eq 'mod_bi_metriccentilemonthlyvalue' } @calls;
        is(scalar @monthly_calls, 1, 'Single call via the initial shortcut');

        my $call = $monthly_calls[0];
        is($call->{start}, $daily_start, 'In this special case, use daily_start (not firstDayOfMonth)');
    };

    subtest 'Purge mode (noPurge = 0)' => sub {
        @calls = ();
        my $etl = {
            run => {
                options => { nopurge => 0, month_only => 0, centile_only => 0, no_centile => 0 },
                etlProperties => { 'perfdata.granularity' => 'day', 'centile.month' => '1' }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods);

        my $call = (grep { ($_->{name} // '') eq 'mod_bi_metriccentilemonthlyvalue' } @calls)[0];
        ok($call, 'Call present in Purge mode');
        is($call->{method}, 'emptyTableForRebuild', 'Method: emptyTableForRebuild');
        is($call->{start}, $daily_start, 'Standard purge uses daily_start');
    };

    subtest 'No-Purge mode (noPurge = 1)' => sub {
        @calls = ();
        my $etl = {
            run => {
                options => { nopurge => 1, month_only => 0, centile_only => 0, no_centile => 0 },
                etlProperties => { 'perfdata.granularity' => 'day', 'centile.month' => '1' }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods);

        my $call = (grep { ($_->{name} // '') eq 'mod_bi_metriccentilemonthlyvalue' } @calls)[0];
        ok($call, 'Call present in No-Purge mode');
        is($call->{method}, 'deleteEntriesForRebuild', 'Method: deleteEntriesForRebuild');
        is($call->{start}, $first_day, 'No-Purge uses firstDayOfMonth to protect the month');
    };

    subtest 'No-Purge : same month' => sub {
        @calls = ();
        my $periods_same = {
            'perfdata.daily' => { start => '2026-01-10', end => '2026-01-20' },
            'perfdata.hourly' => { start => '2026-01-10', end => '2026-01-20' }
        };
        my $etl = {
            run => {
                options => { nopurge => 1, month_only => 0, centile_only => 0 },
                etlProperties => { 'perfdata.granularity' => 'day', 'centile.month' => '1' }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods_same);

        my @monthly_calls = grep { ($_->{name} // '') eq 'mod_bi_metriccentilemonthlyvalue' } @calls;
        is(scalar @monthly_calls, 0, 'In No-Purge, if same month, the table is ignored');
    };
};

done_testing();
