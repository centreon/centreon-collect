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
    use Time::Local qw(timegm);
    sub new { return bless({}, __PACKAGE__); }

    # date helpers, behaving like the real ones for whole days. Computed in UTC to
    # keep the tests independent from the timezone of the host running them
    sub _epoch {
        my ($date) = @_;
        my ($year, $month, $day) = split(/-/, $date);
        return timegm(0, 0, 0, $day, $month - 1, $year);
    }
    sub _date {
        my ($epoch) = @_;
        my @date = gmtime($epoch);
        return sprintf('%04d-%02d-%02d', $date[5] + 1900, $date[4] + 1, $date[3]);
    }
    sub getDayOfWeek {
        my ($self, $date) = @_;
        return (qw(sunday monday tuesday wednesday thursday friday saturday))[(gmtime(_epoch($date)))[6]];
    }
    sub subtractDateDays {
        my ($self, $date, $num) = @_;
        return _date(_epoch($date) - $num * 86400);
    }
    sub getRangePartitionDate {
        my ($self, $start, $end) = @_;
        my ($epoch, $epoch_end) = (_epoch($start), _epoch($end));
        my $partitions = [];
        while ($epoch < $epoch_end) {
            $epoch += 86400;
            my $date = _date($epoch);
            push @$partitions, { name => $date =~ s/-//gr, date => $date, epoch => $epoch };
        }
        return $partitions;
    }
    $INC{'gorgone/modules/centreon/mbi/libs/Utils.pm'} = 1;

    package gorgone::standard::constants;
    use Exporter 'import';
    use constant GORGONE_MODULE_CENTREON_MBIETL_PROGRESS => 1;
    our @EXPORT_OK = qw(GORGONE_MODULE_CENTREON_MBIETL_PROGRESS);
    our %EXPORT_TAGS = ( 'all' => [qw(GORGONE_MODULE_CENTREON_MBIETL_PROGRESS)] );
    $INC{'gorgone/standard/constants.pm'} = 1;
}

use gorgone::modules::centreon::mbi::etl::perfdata::main;

# instantiate the module level helpers: purgeTables uses $utils to align the weekly
# centile period on the weeks it recomputes
gorgone::modules::centreon::mbi::etl::perfdata::main::initVars({ run => {} });

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

    # The table is dropped and recreated, so the period is not realigned on the weeks
    subtest 'Purge mode (noPurge = 0)' => sub {
        @calls = ();
        my $etl = {
            run => {
                options => { nopurge => 0, month_only => 0, centile_only => 0, no_centile => 0 },
                etlProperties => {
                    'perfdata.granularity' => 'day',
                    'centile.week' => '1',
                    'centile.weekFirstDay' => 'monday'
                }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods);

        my @weekly_calls = grep { ($_->{name} // '') eq 'mod_bi_metriccentileweeklyvalue' } @calls;
        is(scalar @weekly_calls, 1, 'There must be exactly 1 call in Purge mode');

        my $call = $weekly_calls[0];
        is($call->{method}, 'emptyTableForRebuild', 'Should use emptyTableForRebuild');
        is($call->{start}, $daily_start, 'Uses the daily_start date');
        is($call->{end}, $daily_end, 'Uses the daily_end date');
    };

    # Weekly values are stamped with the first day of the week they aggregate, so
    # in No-Purge mode the purged period is realigned on the recomputed weeks
    subtest 'No-Purge mode (noPurge = 1)' => sub {
        @calls = ();
        my $etl = {
            run => {
                options => { nopurge => 1, month_only => 0, centile_only => 0, no_centile => 0 },
                etlProperties => {
                    'perfdata.granularity' => 'day',
                    'centile.week' => '1',
                    'centile.weekFirstDay' => 'monday'
                }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods);

        my @weekly_calls = grep { ($_->{name} // '') eq 'mod_bi_metriccentileweeklyvalue' } @calls;
        is(scalar @weekly_calls, 1, 'There must be exactly 1 call in No-Purge mode');

        my $call = $weekly_calls[0];
        is($call->{method}, 'deleteEntriesForRebuild', 'Should use deleteEntriesForRebuild');
        # mondays of the rebuilt period are 2026-01-05, 12, 19 and 26
        is($call->{start}, '2025-12-29', 'Starts on the first day of the oldest recomputed week');
        is($call->{end}, '2026-01-20', 'Ends right after the first day of the newest recomputed week');
    };

    # Reproduces the rebuild of a couple of days inside a week: the recomputed week
    # starts before the rebuilt period, its rows were not purged and the insert
    # failed on a duplicate primary key
    subtest 'No-Purge mode (noPurge = 1) - Rebuild shorter than a week' => sub {
        @calls = ();
        my $periods_partial = {
            'perfdata.daily'  => { start => '2026-07-12', end => '2026-07-14' },
            'perfdata.hourly' => { start => '2026-07-12', end => '2026-07-14' }
        };
        my $etl = {
            run => {
                options => { nopurge => 1, month_only => 0, centile_only => 0, no_centile => 0 },
                etlProperties => {
                    'perfdata.granularity' => 'day',
                    'centile.week' => '1',
                    'centile.weekFirstDay' => 'monday'
                }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods_partial);

        my @weekly_calls = grep { ($_->{name} // '') eq 'mod_bi_metriccentileweeklyvalue' } @calls;
        is(scalar @weekly_calls, 1, 'There must be exactly 1 call in No-Purge mode');

        my $call = $weekly_calls[0];
        # the rebuild recomputes the week of monday 2026-07-06 to sunday 2026-07-12
        is($call->{start}, '2026-07-06', 'Purges the week recomputed by the rebuild');
        is($call->{end}, '2026-07-07', 'Does not purge the following weeks');

        # the realigned period must not leak to the other tables of the same rebuild
        my $daily_call = (grep { ($_->{name} // '') eq 'mod_bi_metricdailyvalue' } @calls)[0];
        ok($daily_call, 'The daily table is purged by the same rebuild');
        is($daily_call->{start}, '2026-07-12', 'The daily table keeps the rebuilt period as start');
        is($daily_call->{end}, '2026-07-14', 'The daily table keeps the rebuilt period as end');
    };

    # The weekly table is skipped when no week is recomputed, but the tables listed
    # after it must still be purged
    subtest 'No-Purge mode (noPurge = 1) - Skipping the week leaves the other tables' => sub {
        @calls = ();
        my $periods_month_change = {
            'perfdata.daily'  => { start => '2026-07-30', end => '2026-08-02' },
            'perfdata.hourly' => { start => '2026-07-30', end => '2026-08-02' }
        };
        my $etl = {
            run => {
                options => { nopurge => 1, month_only => 0, centile_only => 0, no_centile => 0 },
                etlProperties => {
                    'perfdata.granularity' => 'day',
                    'centile.week' => '1',
                    'centile.month' => '1',
                    'centile.weekFirstDay' => 'monday'
                }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods_month_change);

        # 2026-07-31, 08-01 and 08-02 are a friday, a saturday and a sunday
        my @weekly_calls = grep { ($_->{name} // '') eq 'mod_bi_metriccentileweeklyvalue' } @calls;
        is(scalar @weekly_calls, 0, 'No week aggregated by the rebuild, so nothing to purge');

        my @monthly_calls = grep { ($_->{name} // '') eq 'mod_bi_metriccentilemonthlyvalue' } @calls;
        is(scalar @monthly_calls, 1, 'The monthly table, listed after the weekly one, is still purged');
        is($monthly_calls[0]->{start}, '2026-07-01', 'The monthly purge starts on the first day of the month');
    };

    subtest 'No-Purge mode (noPurge = 1) - No week to recompute' => sub {
        @calls = ();
        my $periods_no_week = {
            'perfdata.daily'  => { start => '2026-07-14', end => '2026-07-17' },
            'perfdata.hourly' => { start => '2026-07-14', end => '2026-07-17' }
        };
        my $etl = {
            run => {
                options => { nopurge => 1, month_only => 0, centile_only => 0, no_centile => 0 },
                etlProperties => {
                    'perfdata.granularity' => 'day',
                    'centile.week' => '1',
                    'centile.weekFirstDay' => 'monday'
                }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods_no_week);

        my @weekly_calls = grep { ($_->{name} // '') eq 'mod_bi_metriccentileweeklyvalue' } @calls;
        is(scalar @weekly_calls, 0, 'No week aggregated by the rebuild, so nothing to purge');
    };

    # The first day of the week is configurable, it is not always a monday
    subtest 'No-Purge mode (noPurge = 1) - Week starting on sunday' => sub {
        @calls = ();
        my $periods_sunday = {
            'perfdata.daily'  => { start => '2026-07-17', end => '2026-07-20' },
            'perfdata.hourly' => { start => '2026-07-17', end => '2026-07-20' }
        };
        my $etl = {
            run => {
                options => { nopurge => 1, month_only => 0, centile_only => 0, no_centile => 0 },
                etlProperties => {
                    'perfdata.granularity' => 'day',
                    'centile.week' => '1',
                    'centile.weekFirstDay' => 'sunday'
                }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods_sunday);

        my @weekly_calls = grep { ($_->{name} // '') eq 'mod_bi_metriccentileweeklyvalue' } @calls;
        is(scalar @weekly_calls, 1, 'There must be exactly 1 call in No-Purge mode');

        my $call = $weekly_calls[0];
        # the rebuild crosses sunday 2026-07-19, recomputing the week of sunday 2026-07-12
        is($call->{start}, '2026-07-12', 'Purges the week recomputed by the rebuild');
        is($call->{end}, '2026-07-13', 'Does not purge the following weeks');
    };

    # rebuildProcessing aggregates a week when it processes a day ending on the week
    # first day, so the very first day of the rebuild never triggers an aggregation
    subtest 'No-Purge mode (noPurge = 1) - Rebuild starting on the week first day' => sub {
        @calls = ();
        my $periods_start_monday = {
            'perfdata.daily'  => { start => '2026-07-13', end => '2026-07-15' },
            'perfdata.hourly' => { start => '2026-07-13', end => '2026-07-15' }
        };
        my $etl = {
            run => {
                options => { nopurge => 1, month_only => 0, centile_only => 0, no_centile => 0 },
                etlProperties => {
                    'perfdata.granularity' => 'day',
                    'centile.week' => '1',
                    'centile.weekFirstDay' => 'monday'
                }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods_start_monday);

        my @weekly_calls = grep { ($_->{name} // '') eq 'mod_bi_metriccentileweeklyvalue' } @calls;
        is(scalar @weekly_calls, 0, 'The monday the rebuild starts on is not aggregated');
    };

    subtest 'No-Purge mode (noPurge = 1) - Week first day not configured' => sub {
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
        is(scalar @weekly_calls, 0, 'No week can be aggregated, so nothing to purge');
    };

    # --centile-only --no-purge, as reported: the weekly table is still purged
    subtest 'No-Purge mode (noPurge = 1) - Centile only' => sub {
        @calls = ();
        my $periods_partial = {
            'perfdata.daily'  => { start => '2026-07-12', end => '2026-07-14' },
            'perfdata.hourly' => { start => '2026-07-12', end => '2026-07-14' }
        };
        my $etl = {
            run => {
                options => { nopurge => 1, month_only => 0, centile_only => 1, no_centile => 0 },
                etlProperties => {
                    'perfdata.granularity' => 'day',
                    'centile.week' => '1',
                    'centile.weekFirstDay' => 'monday'
                }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods_partial);

        my @weekly_calls = grep { ($_->{name} // '') eq 'mod_bi_metriccentileweeklyvalue' } @calls;
        is(scalar @weekly_calls, 1, 'There must be exactly 1 call with --centile-only');

        my $call = $weekly_calls[0];
        is($call->{start}, '2026-07-06', 'Purges the week recomputed by the rebuild');
        is($call->{end}, '2026-07-07', 'Does not purge the following weeks');
    };

    subtest 'No-Purge mode (noPurge = 1) - Month only' => sub {
        @calls = ();
        my $etl = {
            run => {
                options => { nopurge => 1, month_only => 1, centile_only => 0, no_centile => 0 },
                etlProperties => {
                    'perfdata.granularity' => 'day',
                    'centile.week' => '1',
                    'centile.weekFirstDay' => 'monday'
                }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods);

        my @weekly_calls = grep { ($_->{name} // '') eq 'mod_bi_metriccentileweeklyvalue' } @calls;
        is(scalar @weekly_calls, 0, 'The table must not be processed with --month-only');
    };

    subtest 'No-Purge mode (noPurge = 1) - Hourly granularity' => sub {
        @calls = ();
        my $etl = {
            run => {
                options => { nopurge => 1, month_only => 0, centile_only => 0, no_centile => 0 },
                etlProperties => {
                    'perfdata.granularity' => 'hour',
                    'centile.week' => '1',
                    'centile.weekFirstDay' => 'monday'
                }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods);

        my @weekly_calls = grep { ($_->{name} // '') eq 'mod_bi_metriccentileweeklyvalue' } @calls;
        is(scalar @weekly_calls, 0, 'The table must not be processed when only hours are aggregated');
    };

    subtest 'No-Purge mode (noPurge = 1) - No centile' => sub {
        @calls = ();
        my $etl = {
            run => {
                options => { nopurge => 1, month_only => 0, centile_only => 0, no_centile => 1 },
                etlProperties => {
                    'perfdata.granularity' => 'day',
                    'centile.week' => '1',
                    'centile.weekFirstDay' => 'monday'
                }
            }
        };

        gorgone::modules::centreon::mbi::etl::perfdata::main::purgeTables($etl, $periods);

        my @weekly_calls = grep { ($_->{name} // '') eq 'mod_bi_metriccentileweeklyvalue' } @calls;
        is(scalar @weekly_calls, 0, 'The table must not be processed with --no-centile');
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
