use strict;
use warnings;
use Test2::V0;

use FindBin;
use File::Temp qw(tempdir);

my $script = "$FindBin::Bin/../../../contrib/gorgone_install_plugins.pl";

# Runs the script with a fake apt-get first in PATH. The fake apt-get records
# each call on its own line and exits with $update_exit on "update".
sub run_script {
    my (%options) = @_;

    my $dir = tempdir(CLEANUP => 1);
    my $log = "$dir/calls.log";
    my $update_exit = $options{update_exit} // 0;

    open(my $fh, '>', "$dir/apt-get") or die "cannot write fake apt-get: $!";
    print $fh <<"EOF";
#!/bin/sh
echo "\$*" >> '$log'
if [ "\$1" = "update" ]; then
    exit $update_exit
fi
exit 0
EOF
    close($fh);
    chmod(0755, "$dir/apt-get") or die "cannot chmod fake apt-get: $!";

    local $ENV{PATH} = "$dir:$ENV{PATH}";
    my $output = `$^X $script --type=deb @{$options{plugins}} 2>&1`;
    my $exit = $? >> 8;

    my @calls;
    if (open(my $in, '<', $log)) {
        chomp(@calls = <$in>);
        close($in);
    }

    return ($exit, $output, \@calls);
}

subtest 'deb: index updated before install' => sub {
    my ($exit, $output, $calls) = run_script(plugins => ['centreon-plugin-Operatingsystems-Linux-Snmp']);

    is($exit, 0, 'script succeeds');
    is(
        $calls,
        ['update', '-y install centreon-plugin-Operatingsystems-Linux-Snmp-*'],
        'apt-get update runs, then apt-get install, as two separate commands'
    );
};

subtest 'deb: install still runs when update fails' => sub {
    my ($exit, $output, $calls) = run_script(
        plugins     => ['centreon-plugin-Applications-Databases-Mysql'],
        update_exit => 100
    );

    is($exit, 0, 'script succeeds');
    is(
        $calls,
        ['update', '-y install centreon-plugin-Applications-Databases-Mysql-*'],
        'a failed update does not prevent the install'
    );
    like($output, qr/code: 100/, 'the update exit code is reported');
};

subtest 'deb: nothing to install' => sub {
    my ($exit, $output, $calls) = run_script(plugins => ['not-a-plugin']);

    is($exit, 0, 'script succeeds');
    is($calls, [], 'apt-get is never called');
    like($output, qr/nothing to install/, 'reports nothing to install');
};

done_testing();
