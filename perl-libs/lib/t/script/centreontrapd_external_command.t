#!/usr/bin/perl
use strict;
use warnings;

use Test2::V0;
use FindBin;
use File::Temp qw(tempdir);
use POSIX ();
use lib "$FindBin::Bin/../../";
use centreon::common::logger;
use centreon::script::centreontrapd;

# Values a device can send in a trap varbind. Each one must end up in the
# external command exactly as it was received.
my @VALUES = (
    'plain text',
    'Link down (ifIndex 3)',
    'double "quoted" text',
    "single 'quoted' text",
    'dollar $HOME and ${HOME}',
    'backslash \\ and \\" sequence',
);

# A trap daemon with a single trap, linked to the service 'svc' of the host 'host'.
# The external commands are written to $options{cmd_file} or to $options{cmd_dir}.
sub new_trapd {
    my (%options) = @_;

    my $logger = centreon::common::logger->new();
    $logger->severity('fatal');

    my $self = centreon::script::centreontrapd->new();
    $self->{logger} = $logger;
    $self->{centreontrapd_config} = { %{$self->{centreontrapd_default_config}} };
    $self->{whoami} = $self->{centreontrapd_config}->{centreon_user};
    $self->{centreontrapd_config}->{mode} = 1;
    $self->{cmdFile} = $options{cmd_file};
    $self->{cmdDir} = $options{cmd_dir};
    # The engine owns and creates its command pipe, so the daemon only appends to
    # an existing cmdFile. Create it here when the test did not (a FIFO test does).
    if (defined($options{cmd_file}) && !-e $options{cmd_file}) {
        open(my $fh, '>', $options{cmd_file}) or die "cannot create $options{cmd_file}: $!";
        close($fh);
    }
    $self->{current_alarm_timeout} = 5;
    $self->{current_hostname} = 'host';
    $self->{current_service_desc} = 'svc';
    $self->{traps_global_status} = 2;
    $self->{trap_data} = {
        current_trap_id => 1,
        entvar => $options{entvar} // [],
        entvarname => [ map { '.1.3.6.1.4.1.99999.1.' . $_ } 1 .. scalar(@{$options{entvar} // []}) ],
        var => [ 'host', '127.0.0.1', '', '.1.3.6.1.4.1.99999.0.1', '127.0.0.1' ],
        agent_dns_name => 'host',
        ref_oids => { 1 => { traps_args => $options{traps_args} // '$1' } }
    };
    $self->{current_trap_id} = 1;
    return $self;
}

sub read_lines {
    my ($file) = @_;

    open(my $fh, '<', $file) or return ();
    my @lines = <$fh>;
    close($fh);
    return @lines;
}

# The output of the trap ($1) must be written verbatim in the passive check result.
sub test_submit_result_keeps_values {
    my $dir = tempdir(CLEANUP => 1);

    foreach my $value (@VALUES) {
        my $cmd_file = "$dir/centengine.cmd";
        unlink($cmd_file);
        my $trapd = new_trapd(cmd_file => $cmd_file, entvar => [ $value ]);
        $trapd->{traps_global_output} = $trapd->substitute_string($trapd->{trap_data}->{ref_oids}->{1}->{traps_args});
        $trapd->submitResult();

        my @lines = read_lines($cmd_file);
        is(scalar(@lines), 1, "one external command is written for <$value>");
        like(
            $lines[0] // '',
            qr/^\[\d+\] PROCESS_SERVICE_CHECK_RESULT;host;svc;2;\Q$value\E\n\z/,
            "the trap value <$value> is written as received"
        );
    }
}

# Shell syntax in a trap value is plain text in the passive check result.
sub test_submit_result_shell_syntax_is_text {
    my $dir = tempdir(CLEANUP => 1);
    my $marker = "$dir/marker";
    my $cmd_file = "$dir/centengine.cmd";

    my @values = (
        '$(touch ' . $marker . ')',
        '`touch ' . $marker . '`',
        '"; touch ' . $marker . '; echo "',
    );
    foreach my $value (@values) {
        unlink($cmd_file);
        my $trapd = new_trapd(cmd_file => $cmd_file, entvar => [ $value ]);
        $trapd->{traps_global_output} = $trapd->substitute_string('$1');
        $trapd->submitResult();

        ok(!-e $marker, "the trap value <$value> is not run");
        my @lines = read_lines($cmd_file);
        like($lines[0] // '', qr/;2;\Q$value\E\n\z/, "the trap value <$value> is written as received");
    }
}

# A line break in a trap value cannot add a second external command.
sub test_submit_result_single_line {
    my $dir = tempdir(CLEANUP => 1);
    my $cmd_file = "$dir/centengine.cmd";

    my $trapd = new_trapd(
        cmd_file => $cmd_file,
        entvar => [ "first line\n[1] SCHEDULE_FORCED_SVC_CHECK;other;svc;1\r\0end" ]
    );
    $trapd->{traps_global_output} = $trapd->substitute_string('$1');
    $trapd->submitResult();

    my @lines = read_lines($cmd_file);
    is(scalar(@lines), 1, 'a single external command is written');
    like(
        $lines[0] // '',
        qr/;2;first line \[1\] SCHEDULE_FORCED_SVC_CHECK;other;svc;1  end\n\z/,
        'the line breaks of the trap value are replaced by spaces'
    );
}

# Multiple varbinds ($*) are joined with the separator, as before.
sub test_submit_result_all_values {
    my $dir = tempdir(CLEANUP => 1);
    my $cmd_file = "$dir/centengine.cmd";

    my $trapd = new_trapd(cmd_file => $cmd_file, entvar => [ 'eth0', 'down', 'a "b" $c' ]);
    $trapd->{traps_global_output} = $trapd->substitute_string('Interface $*');
    $trapd->submitResult();

    my @lines = read_lines($cmd_file);
    like($lines[0] // '', qr/;2;Interface eth0 down a "b" \$c\n\z/, 'the values of $* are written as received');
}

# The severity is sent with two more external commands.
sub test_submit_result_severity {
    my $dir = tempdir(CLEANUP => 1);
    my $cmd_file = "$dir/centengine.cmd";

    my $trapd = new_trapd(cmd_file => $cmd_file, entvar => [ 'value' ]);
    $trapd->{traps_global_output} = 'value';
    $trapd->{traps_global_severity_id} = 4;
    $trapd->{traps_global_severity_level} = 2;
    $trapd->submitResult();

    my @lines = read_lines($cmd_file);
    is(scalar(@lines), 3, 'the result and the severity are written');
    like($lines[1] // '', qr/^\[\d+\] CHANGE_CUSTOM_SVC_VAR;host;svc;CRITICALITY_ID;4\n\z/, 'the severity id is written');
    like($lines[2] // '', qr/^\[\d+\] CHANGE_CUSTOM_SVC_VAR;host;svc;CRITICALITY_LEVEL;2\n\z/, 'the severity level is written');
}

# In central mode (0) the command is prefixed for the poller, and written to a
# new file of the command directory.
sub test_submit_result_command_dir {
    my $dir = tempdir(CLEANUP => 1);

    my $trapd = new_trapd(cmd_dir => $dir, entvar => [ 'a "b" $(c)' ]);
    $trapd->{centreontrapd_config}->{mode} = 0;
    $trapd->{current_server_id} = 3;
    $trapd->{traps_global_output} = $trapd->substitute_string('$1');
    $trapd->submitResult();

    my @files = glob("$dir/*-traps");
    is(scalar(@files), 1, 'a file is written in the command directory');
    my @lines = read_lines($files[0] // '');
    like(
        $lines[0] // '',
        qr/^EXTERNALCMD:3:\[\d+\] PROCESS_SERVICE_CHECK_RESULT;host;svc;2;a "b" \$\(c\)\n\z/,
        'the external command is written for the poller'
    );
}

# The command file is the engine pipe: when it is absent (engine stopped), the
# write fails and no regular file is created in its place.
sub test_submit_result_does_not_create_cmdfile {
    my $dir = tempdir(CLEANUP => 1);
    my $cmd_file = "$dir/engine.pipe";

    my $trapd = new_trapd(cmd_file => $cmd_file, entvar => [ 'value' ]);
    unlink($cmd_file);   # the engine removed its pipe when it stopped
    $trapd->{traps_global_output} = 'value';

    my $rc = $trapd->write_external_command(datetime => time(), command => 'TEST', log_prefix => 'TEST');
    is($rc, 1, 'the write fails when the command pipe is absent');
    ok(!-e $cmd_file, 'no regular file is created in place of the engine pipe');
}

# The service check is rescheduled with an external command.
sub test_force_check {
    my $dir = tempdir(CLEANUP => 1);
    my $cmd_file = "$dir/centengine.cmd";

    my $trapd = new_trapd(cmd_file => $cmd_file);
    $trapd->forceCheck();

    my @lines = read_lines($cmd_file);
    like($lines[0] // '', qr/^\[(\d+)\] SCHEDULE_FORCED_SVC_CHECK;host;svc;\1\n\z/, 'the check is rescheduled');
}

# The command file of the engine is a named pipe: the line is read as written.
sub test_submit_result_fifo {
    my $dir = tempdir(CLEANUP => 1);
    my $cmd_file = "$dir/centengine.cmd";
    POSIX::mkfifo($cmd_file, 0600) or die "cannot create fifo: $!";
    sysopen(my $reader, $cmd_file, POSIX::O_RDONLY() | POSIX::O_NONBLOCK()) or die "cannot open fifo: $!";

    my $trapd = new_trapd(cmd_file => $cmd_file, entvar => [ 'a "b" $c' ]);
    $trapd->{traps_global_output} = $trapd->substitute_string('$1');
    $trapd->submitResult();

    my $line = '';
    sysread($reader, $line, 4096);
    close($reader);
    like($line, qr/^\[\d+\] PROCESS_SERVICE_CHECK_RESULT;host;svc;2;a "b" \$c\n\z/, 'the line is written to the named pipe');
}

# Without a reader on the named pipe (engine stopped), the write gives up after
# the command timeout instead of blocking the trap process.
sub test_submit_result_fifo_timeout {
    my $dir = tempdir(CLEANUP => 1);
    my $cmd_file = "$dir/centengine.cmd";
    POSIX::mkfifo($cmd_file, 0600) or die "cannot create fifo: $!";

    my $start = time();
    my $pid = fork();
    die "cannot fork: $!" if (!defined($pid));
    if ($pid == 0) {
        my $trapd = new_trapd(cmd_file => $cmd_file);
        $trapd->{current_alarm_timeout} = 1;
        my $result = $trapd->write_external_command(datetime => time(), command => 'TEST', log_prefix => 'TEST');
        POSIX::_exit($result);
    }

    # The test itself must not block if the timeout does not work.
    my $done = 0;
    while (time() - $start < 15) {
        $done = waitpid($pid, POSIX::WNOHANG());
        last if ($done == $pid);
        select(undef, undef, undef, 0.1);
    }
    if ($done != $pid) {
        kill('KILL', $pid);
        waitpid($pid, 0);
    }
    is($done, $pid, 'the write gives up without a reader');
    is($? >> 8, 1, 'the write is reported as failed');
    ok(time() - $start <= 5, 'the write gives up after the command timeout');
}

# An output too long to fit in one engine read is cut, and the command is still
# written, as a single line that stays within one read.
sub test_submit_result_truncates_long_output {
    my $dir = tempdir(CLEANUP => 1);
    my $cmd_file = "$dir/centengine.cmd";

    my $trapd = new_trapd(cmd_file => $cmd_file, entvar => [ 'B' x 10000 ]);
    $trapd->{traps_global_output} = $trapd->substitute_string('$1');
    $trapd->submitResult();

    my @lines = read_lines($cmd_file);
    is(scalar(@lines), 1, 'the long output is written as a single line');
    ok(length($lines[0]) <= 4096, 'the record stays within one engine read (<= 4096 bytes)');
    like($lines[0], qr/^\[\d+\] PROCESS_SERVICE_CHECK_RESULT;host;svc;2;B+\n\z/, 'the command is still written, with the output cut');
}

test_submit_result_keeps_values();
test_submit_result_shell_syntax_is_text();
test_submit_result_single_line();
test_submit_result_all_values();
test_submit_result_severity();
test_submit_result_command_dir();
test_force_check();
test_submit_result_does_not_create_cmdfile();
test_submit_result_fifo();
test_submit_result_fifo_timeout();
test_submit_result_truncates_long_output();

done_testing();
