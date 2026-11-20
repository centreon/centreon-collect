#!/usr/bin/perl
use strict;
use warnings;

use Test2::V0;
use FindBin;
use File::Temp qw(tempdir);
use lib "$FindBin::Bin/../../";
use centreon::common::logger;
use centreon::script::centreontrapd;

my $dir = tempdir(CLEANUP => 1);
my $marker = "$dir/marker";
my $out = "$dir/out";

# A helper the configured command calls: it writes each argument it receives on
# its own line to the file given as its first argument. It lets the test read
# exactly what the command was given, argument by argument.
my $writer = "$dir/writer.sh";
{
    open(my $fh, '>', $writer) or die "cannot write $writer: $!";
    print $fh "#!/bin/sh\n";
    print $fh "out=\$1\n";
    print $fh "shift\n";
    print $fh "for a in \"\$@\"; do printf '%s\\n' \"\$a\"; done > \"\$out\"\n";
    close($fh);
    chmod(0755, $writer);
}

# Trap values holding shell syntax (but no quote or backslash). Without a shell
# they must reach the command verbatim, as a single argument, and run nothing.
my @SHELL_SYNTAX_VALUES = (
    '$(touch ' . $marker . ')',
    '`touch ' . $marker . '`',
    '; touch ' . $marker,
    '| touch ' . $marker,
    '&& touch ' . $marker,
    '> ' . $marker,
    '$(a) `b` ; c | d && e > f',
);

sub new_trapd {
    my (%options) = @_;

    my $logger = centreon::common::logger->new();
    $logger->severity('fatal');

    my $self = centreon::script::centreontrapd->new();
    $self->{logger} = $logger;
    $self->{centreontrapd_config} = { %{$self->{centreontrapd_default_config}} };
    $self->{htmlentities} = $options{htmlentities} // 0;
    $self->{current_alarm_timeout} = 5;
    $self->{current_hostname} = 'host';
    $self->{current_service_desc} = 'svc';
    $self->{current_ip} = $options{ip} // '127.0.0.1';
    $self->{traps_global_output} = $options{output} // '';
    $self->{trap_data} = {
        current_trap_id => 1,
        entvar => $options{entvar} // [],
        entvarname => [ map { '.1.3.6.1.4.1.99999.1.' . $_ } 1 .. scalar(@{$options{entvar} // []}) ],
        var => [ 'host', '127.0.0.1', '', '.1.3.6.1.4.1.99999.0.1', '127.0.0.1' ],
        agent_dns_name => $options{agent_dns_name} // 'host',
        ref_oids => {
            1 => {
                traps_execution_command => $options{command} // '',
                traps_preexec => [ map { { tpe_string => $_ } } @{$options{preexec} // []} ]
            }
        }
    };
    $self->{current_trap_id} = 1;
    return $self;
}

sub read_out {
    open(my $fh, '<', $out) or return undef;
    local $/;
    my $content = <$fh>;
    close($fh);
    return $content;
}

sub reset_files {
    unlink($marker, $out);
}

# Shell syntax in a varbind ($1) is passed to the special command as a single
# verbatim argument, and nothing else runs.
sub test_special_command_varbind_is_one_verbatim_argument {
    foreach my $value (@SHELL_SYNTAX_VALUES) {
        reset_files();
        my $trapd = new_trapd(command => "$writer $out \"\$1\"", entvar => [ $value ]);
        $trapd->executeCommand();

        ok(!-e $marker, "no shell runs for <$value>");
        is(read_out(), "$value\n", "the varbind <$value> reaches the command verbatim, as one argument");
    }
}

# The same for @{OID}, $* and the Centreon output macro.
sub test_special_command_other_macros {
    reset_files();
    my $t = new_trapd(command => "$writer $out \"\@{.1.3.6.1.4.1.99999.1.1}\"", entvar => [ '$(touch ' . $marker . ')' ]);
    $t->executeCommand();
    ok(!-e $marker, '@{OID}: no shell runs');
    is(read_out(), '$(touch ' . $marker . ')' . "\n", '@{OID} reaches the command verbatim');

    reset_files();
    $t = new_trapd(command => "$writer $out \"\@OUTPUT\@\"", output => '`touch ' . $marker . '`');
    $t->executeCommand();
    ok(!-e $marker, '@OUTPUT@: no shell runs');
    is(read_out(), '`touch ' . $marker . '`' . "\n", '@OUTPUT@ reaches the command verbatim');
}

# Known effect of the word splitter (same as gorgone): without a shell the command
# is split with quotewords, which treats quotes and backslash as syntax. So quote
# and backslash characters in a varbind are consumed by the split. This alters the
# value but is not a shell injection: nothing extra runs.
sub test_quotes_and_backslash_consumed_by_word_splitter {
    reset_files();
    my $t = new_trapd(command => "$writer $out \"\$1\"", entvar => [ 'back\\slash' ]);
    $t->executeCommand();
    is(read_out(), "backslash\n", 'the word splitter consumes the backslash of the value');

    reset_files();
    $t = new_trapd(command => "$writer $out \"\$1\"", entvar => [ 'a "b" c' ]);
    $t->executeCommand();
    ok(!-e $marker, 'quotes in the value run nothing');
    is(read_out(), "a b c\n", 'the word splitter consumes the double quotes of the value');
}

# $* keeps each varbind as a separate word, joined by the separator.
sub test_special_command_all_values {
    reset_files();
    my $t = new_trapd(command => "$writer $out \$*", entvar => [ 'eth0', 'down' ]);
    $t->executeCommand();
    is(read_out(), "eth0\ndown\n", '$* is split on the separator into separate arguments');
}

# Usual values reach the command unchanged, and @HOSTNAME@/@SERVICEDESC@ (from the
# database) are substituted as before.
sub test_special_command_usual_values {
    foreach my $value ('Link down (ifIndex 3)', 'summer: 10:42 #3 ~ok*', 'AT&T; can\'t') {
        reset_files();
        my $t = new_trapd(command => "$writer $out \"\$1\"", entvar => [ $value ]);
        $t->executeCommand();
        is(read_out(), "$value\n", "the value <$value> is passed unchanged");
    }

    reset_files();
    my $t = new_trapd(command => "$writer $out \@HOSTNAME\@ \@SERVICEDESC\@ \"\$1\"", entvar => [ 'v' ]);
    $t->executeCommand();
    is(read_out(), "host\nsvc\nv\n", 'the database macros are substituted');
}

# PREEXEC commands get the same treatment, and their output ($p1) too.
sub test_preexec {
    foreach my $value ('$(touch ' . $marker . ')', '; touch ' . $marker) {
        reset_files();
        my $t = new_trapd(preexec => [ "$writer $out \"\$1\"" ], entvar => [ $value ]);
        $t->execute_preexec();
        ok(!-e $marker, "preexec: no shell runs for <$value>");
        is(read_out(), "$value\n", "preexec: the varbind <$value> is passed verbatim");
    }

    # The preexec output feeds $p1 of the special command, also as one argument.
    reset_files();
    my $t = new_trapd(
        preexec => [ "/bin/echo injected-preexec-output" ],
        command => "$writer $out \"\$p1\"",
        entvar => []
    );
    $t->execute_preexec();
    $t->executeCommand();
    is(read_out(), "injected-preexec-output\n", 'the preexec output reaches the special command as one argument');
}

# The no_shell_interpretation parameter is honoured: set to 0, the command is run
# through a shell (the admin keeps redirections/pipes), at the cost of interpreting
# the values. The parameter exists on the function and is not wired to the config.
sub test_parameter_toggles_shell {
    reset_files();
    my $t = new_trapd(command => "/bin/echo shelled > $out", entvar => []);
    $t->executeCommand(no_shell_interpretation => 0);
    is(read_out(), "shelled\n", 'with no_shell_interpretation => 0 the shell runs (redirection works)');

    # Default (parameter omitted) is no shell: the redirection is a literal argument.
    reset_files();
    $t = new_trapd(command => "$writer $out redir-is-literal > $marker", entvar => []);
    $t->executeCommand();
    ok(!-e $marker, 'by default no shell runs, the redirection does not create a file');
}

test_special_command_varbind_is_one_verbatim_argument();
test_special_command_other_macros();
test_quotes_and_backslash_consumed_by_word_splitter();
test_special_command_all_values();
test_special_command_usual_values();
test_preexec();
test_parameter_toggles_shell();

done_testing();
