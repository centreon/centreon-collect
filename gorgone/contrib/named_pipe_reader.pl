#!/usr/bin/perl
# Copyright 2024 Centreon (http://www.centreon.com/)
#
# Centreon is a full-fledged industry-strength solution that meets
# the needs in IT infrastructure and application monitoring for
# service performance.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
use strict;
use warnings;
use Getopt::Long;
use POSIX qw(mkfifo);
use DateTime;
use File::Basename;
use File::Path qw(make_path);
# global variable for easier verb() usage, don't need to pass the verbose argument.
my $args = {};
sub main {

    GetOptions("pipename=s" => \$args->{pipename},
               "logfile=s"   => \$args->{logfile},    # string
               "verbose"  => \$args->{verbose}) # flag
    or die("Error in command line arguments\n");
    make_path(dirname($args->{pipename}));

    verb("pipe to create is : " . $args->{pipename});
    unlink($args->{pipename});
    mkfifo($args->{pipename}, 0777) || die "can't mkfifo $args->{pipename} : $!";
    open(my $fh_log, '>>', $args->{logfile}) or die "can't open log file $args->{logfile} : $!";
    {
        my $ofh = select $fh_log;
        $|      = 1; # work only on current filehandle, so we select our log fh and make it hot, and continue after.
        select $ofh
    }
    while (1) {
        open(my $fh_pipe, '<', $args->{pipename}) or die "can't open pipe $args->{pipename}";
        my $val = <$fh_pipe>;
        verb("pipe gave value : " . $val);
        my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time());
        my $date =  sprintf(
            '%04d-%02d-%02d %02d:%02d:%02d',
            $year+1900, $mon+1, $mday, $hour, $min, $sec
        );
        print $fh_log $date . " - " . $val;

        close $fh_pipe;
        sleep(0.1);
    }

}
sub verb {
    return if !$args->{verbose};
    print DateTime->now() . " - ";
    print shift ;
    print "\n";
}
main;
