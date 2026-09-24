#!/usr/bin/perl
use strict;
use warnings;

use Test2::V0;
use FindBin;
use File::Temp qw(tempfile);
use lib "$FindBin::Bin/../";
use centreon::script;

# A path that is guaranteed not to exist, to exercise the "no configuration" case.
my $missing_config = "$FindBin::Bin/this-config-does-not-exist.pm";

# Run $code with STDOUT redirected to an in-memory buffer and return what it wrote.
# The logger prints to STDOUT, so this lets us assert on the emitted log messages
# without polluting the test harness output.
sub capture_stdout {
    my ($code) = @_;
    my $output = '';
    {
        local *STDOUT;
        open(STDOUT, '>', \$output) or die "cannot capture STDOUT: $!";
        $code->();
    }
    return $output;
}

# When a configuration file is required but missing, parse_options() must not die:
# it flags the run to be skipped so that cron jobs do nothing (instead of failing
# against the database and polluting the logs) until the configuration is available.
# It also logs the reason, which requires the logger severity to be applied even
# though init() is skipped.
sub test_requireconfig_missing {
    my $self = centreon::script->new(
        "test",
        requireconfig => 1,
        config_file => $missing_config
    );

    my $output = '';
    my $lived = lives {
        $output = capture_stdout(sub { local @ARGV = (); $self->parse_options(); });
    };
    ok($lived, "parse_options does not die when a required config is missing");
    is($self->{noconfig_skip}, 1, "the run is flagged to be skipped");
    is($self->{centreon_config}, undef, "no configuration is loaded");
    like($output, qr/does not exist or is empty/, "the reason the run is skipped is logged");
}

# An existing but empty configuration file must be treated like a missing one
# (the guard is "-e && -s"), so the "-s" (empty) branch is exercised here.
sub test_requireconfig_empty {
    my ($fh, $filename) = tempfile(SUFFIX => '.pm', UNLINK => 1);
    close($fh); # leave the file empty (0 bytes)

    my $self = centreon::script->new(
        "test",
        requireconfig => 1,
        config_file => $filename
    );

    my $lived = lives {
        capture_stdout(sub { local @ARGV = (); $self->parse_options(); });
    };
    ok($lived, "parse_options does not die when the required config is empty");
    is($self->{noconfig_skip}, 1, "an empty config is treated like a missing one");
    is($self->{centreon_config}, undef, "no configuration is loaded from an empty file");
}

# Without requireconfig (the default), a missing configuration keeps the previous
# behavior: parse_options() silently continues and does not flag a skip.
sub test_noconfig_default {
    my $self = centreon::script->new(
        "test",
        config_file => $missing_config
    );

    local @ARGV = ();
    ok(lives { $self->parse_options() }, "parse_options does not die without requireconfig");
    ok(!$self->{noconfig_skip}, "the run is not flagged to be skipped");
    is($self->{centreon_config}, undef, "no configuration is loaded");
}

# When noconfig is set, the configuration block is skipped entirely, so
# requireconfig must have no effect (no skip flag, no die).
sub test_noconfig_overrides_requireconfig {
    my $self = centreon::script->new(
        "test",
        noconfig => 1,
        requireconfig => 1,
        config_file => $missing_config
    );

    local @ARGV = ();
    ok(lives { $self->parse_options() }, "parse_options does not die when noconfig is set");
    ok(!$self->{noconfig_skip}, "noconfig short-circuits requireconfig");
}

# When the configuration file exists and is not empty, it is loaded and no skip is
# flagged, even with requireconfig enabled.
sub test_requireconfig_present {
    my ($fh, $filename) = tempfile(SUFFIX => '.pm', UNLINK => 1);
    print $fh '$centreon::script::centreon_config = { db_host => "localhost" };' . "\n1;\n";
    close($fh);

    my $self = centreon::script->new(
        "test",
        requireconfig => 1,
        config_file => $filename
    );

    local @ARGV = ();
    ok(lives { $self->parse_options() }, "parse_options does not die when the config is present");
    ok(!$self->{noconfig_skip}, "the run is not flagged to be skipped");
    is($self->{centreon_config}, { db_host => "localhost" }, "the configuration is loaded");
}

# run() must act on the skip flag by exiting 0 before init(), so that subclasses
# (which call SUPER::run() first) never reach the database. Exercised in a
# subprocess because exit() would otherwise terminate the test harness: the
# trailing exit(42) is only reached if run() failed to exit on its own.
sub test_run_exits_when_config_missing {
    my $lib = "$FindBin::Bin/../";
    my $code = join('',
        'use centreon::script;',
        'open(STDOUT, ">", "/dev/null");',
        'centreon::script->new("test", requireconfig => 1, config_file => "' . $missing_config . '")->run();',
        'exit(42);',
    );
    system($^X, "-I$lib", '-e', $code);
    is($? >> 8, 0, "run() exits 0 before init() when a required config is missing");
}

test_requireconfig_missing();
test_requireconfig_empty();
test_noconfig_default();
test_noconfig_overrides_requireconfig();
test_requireconfig_present();
test_run_exits_when_config_missing();

done_testing();
