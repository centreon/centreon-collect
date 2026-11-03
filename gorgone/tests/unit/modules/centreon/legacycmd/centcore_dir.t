#!/usr/bin/perl

use strict;
use warnings;
use Test2::V0;
use Test2::Plugin::NoWarnings echo => 1;
use FindBin;
use lib "$FindBin::Bin/../../../../../";
use lib "$FindBin::Bin/../../../../../../perl-libs/lib/";
use File::Temp qw(tempdir);
use Time::HiRes;
use tests::unit::lib::mockLogger;
use gorgone::modules::centreon::legacycmd::class;

my $second = 1700000000;
# Eight names whose modification times follow an order unrelated to their
# alphabetical order, so that neither a name sort nor the directory listing
# order is likely to match it.
my @scrambled = map { "external-cmd-$_.cmd" } qw(h c f a g b e d);

sub main {
    test_files_handled_in_write_order();
    test_same_time_files_handled_in_name_order();
    test_leftover_read_file_handled_first();
    test_unreadable_entry_skipped_and_logged_once();
    done_testing();
}

# Writes each file with the given modification time, adds the given broken
# symlinks, and runs the given number of passes. Returns the names, without
# their _read suffix, that each pass hands to handle_file, the errors logged
# and the directory.
sub run_passes {
    my (%options) = @_;

    my $dir = tempdir(CLEANUP => 1);
    foreach my $name (keys %{$options{files}}) {
        my $time = $options{files}->{$name};
        open(my $fh, '>', "$dir/$name") or die "Cannot write $dir/$name: $!";
        print $fh "COMMAND\n";
        close($fh);
        Time::HiRes::utime($time, $time, "$dir/$name") or die "Cannot set time of $dir/$name: $!";
    }
    foreach my $name (@{$options{broken_links} // []}) {
        symlink("$dir/missing-target", "$dir/$name") or die "Cannot create link $dir/$name: $!";
    }

    my (@handled, @errors);
    my $mock_logger = mock('centreon::common::logger' => (override => [
        writeLogError => sub { push @errors, $_[1] }
    ]));
    my $mock_legacycmd = mock('gorgone::modules::centreon::legacycmd::class' => (override => [
        handle_file => sub {
            my ($self, %options) = @_;
            close($options{handle});
            push @{$handled[-1]}, $options{file} =~ s{^.*/}{}r =~ s{_read$}{}r;
            return 0;
        }
    ]));
    my $legacycmd = bless(
        { logger => centreon::common::logger->new(), config => { cmd_dir => $dir, dirty_mode => 0 } },
        'gorgone::modules::centreon::legacycmd::class'
    );
    foreach (1 .. ($options{passes} // 1)) {
        push @handled, [];
        $legacycmd->handle_centcore_dir();
    }

    return { handled => \@handled, errors => \@errors, dir => $dir };
}

sub test_files_handled_in_write_order {
    my $result = run_passes(files => { map { ($scrambled[$_] => $second + $_ / 10) } 0 .. $#scrambled });
    is($result->{handled}->[0], \@scrambled, 'files written in the same second are handled in write order');
    is($result->{errors}, [], 'no error is logged');
}

sub test_same_time_files_handled_in_name_order {
    # The web API names its files after microtime(true), which drops trailing
    # zeros: within the same modification time, the name order is the write order.
    my @written = map { "external-cmd-$_.cmd" }
        qw(1700000000.0003 1700000000.05 1700000000.12 1700000000.1234 1700000000.2 1700000000.31 1700000000.5001 1700000000.9);
    my $result = run_passes(files => { map { ($_ => $second) } @written });
    is($result->{handled}->[0], \@written, 'files with the same modification time are handled in the order of their microtime names');
    is($result->{errors}, [], 'no error is logged');
}

sub test_leftover_read_file_handled_first {
    my $result = run_passes(files => {
        'external-cmd-new.cmd'      => $second + 0.2,
        'external-cmd-old.cmd_read' => $second + 0.5,
    });
    is(
        $result->{handled}->[0],
        ['external-cmd-old.cmd', 'external-cmd-new.cmd'],
        'a *_read file left by an interrupted pass is handled before the files still waiting'
    );
    is($result->{errors}, [], 'no error is logged');
}

sub test_unreadable_entry_skipped_and_logged_once {
    my $result = run_passes(
        files        => { 'external-cmd-b.cmd' => $second + 0.1, 'external-cmd-c.cmd' => $second + 0.2 },
        broken_links => ['external-cmd-a.cmd'],
        passes       => 2,
    );
    is(
        $result->{handled}->[0],
        ['external-cmd-b.cmd', 'external-cmd-c.cmd'],
        'an entry that cannot be stat\'ed does not block the others'
    );
    is(
        $result->{errors},
        [match(qr{Skipping '.*/external-cmd-a\.cmd' \(broken symbolic link\)})],
        'the skipped entry is logged once over two passes'
    );
    ok(-l "$result->{dir}/external-cmd-a.cmd" && !-e "$result->{dir}/external-cmd-a.cmd_read", 'the skipped entry is left in place');
}

main();
