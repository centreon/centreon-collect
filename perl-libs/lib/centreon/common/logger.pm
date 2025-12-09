################################################################################
# Copyright 2005-2013 Centreon
# Centreon is developped by : Julien Mathis and Romain Le Merlus under
# GPL Licence 2.0.
# 
# This program is free software; you can redistribute it and/or modify it under 
# the terms of the GNU General Public License as published by the Free Software 
# Foundation ; either version 2 of the License.
# 
# This program is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
# PARTICULAR PURPOSE. See the GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License along with 
# this program; if not, see <http://www.gnu.org/licenses>.
# 
# Linking this program statically or dynamically with other modules is making a 
# combined work based on this program. Thus, the terms and conditions of the GNU 
# General Public License cover the whole combination.
# 
# As a special exception, the copyright holders of this program give Centreon 
# permission to link this program with independent modules to produce an executable, 
# regardless of the license terms of these independent modules, and to copy and 
# distribute the resulting executable under terms of Centreon choice, provided that 
# Centreon also meet, for each linked independent module, the terms  and conditions 
# of the license of that module. An independent module is a module which is not 
# derived from this program. If you modify this program, you may extend this 
# exception to your version of the program, but you are not obliged to do so. If you
# do not wish to do so, delete this exception statement from your version.
# 
#
####################################################################################
# updated by Evan ADAM on 10/2024

package centreon::common::logger;
=head1 logger

centreon::common::logger - Simple logging module

=head1 SYNOPSIS

    #!/usr/bin/perl
    use strict;
    use warnings;
    use centreon::common::logger;

    my $logger = new centreon::common::logger();
    $logger->file_mode("/var/log/mylog.log");
    $logger->withpid(1);
    $logger->withdate(1);
    $logger->redirect_output();
    $logger->severity("debug");

    $logger->writeLogInfo("information");
    $logger->writeLogError("error message");

=head1 DESCRIPTION

This module offers a simple interface to write log messages to various output:

=over

=item * standard output

=item * file

=item * syslog

=back

each log message is associated with a severity level:

=over

=item * FATAL

=item * ERROR

=item * WARNING

=item * NOTICE

=item * INFO

=item * DEBUG

=back

each log message is only written if its severity level is greater than or equal to the configured severity level.
You should use the writeLogXXX methods to write log messages with the appropriate severity level, writelog() is a low level method that should not be used directly.

Each log can be prefixed with the current process id and/or the current date, depending on the configuration.
Each log message is suffixed with a newline character and truncated to 20000 characters.

=head2 Methods

=cut

use strict;
use warnings;
use Sys::Syslog qw(:standard :macros);
use IO::Handle;

# Fixed the severity internal representation to be
my %human_severities = (
    2 => 'FATAL',
    3 => 'ERROR',
    4 => 'WARNING',
    5 => 'NOTICE',
    6 => 'INFO',
    7 => 'DEBUG'
);
=head3 new

Constructor, take no parameter. Initializes the logger with default values.
=cut
sub new {
    my $class = shift;

    my $self = bless
      {
       file => 0,
       filehandler => undef,
       # warning by default, see %human_severities for the available possibilty
       severity => 4,
       old_severity => 4,
       # 0 = stdout, 1 = file, 2 = syslog
       log_mode => 0,
       # Output pid of current process
        withpid => 0,
       # Output date of log
        withdate => 1,
       # syslog
       log_facility => undef,
       log_option => LOG_PID,
      }, $class;
    return $self;
}
=head3 $logger->file_mode("file_name")

close the last opened file (if any) and open the new file for logging in append mode.
=cut
sub file_mode($$) {
    my ($self, $file) = @_;

    if (defined($self->{filehandler})) {
        $self->{filehandler}->close();
    }
    if (open($self->{filehandler}, ">>", $file)){
        $self->{log_mode} = 1;
        $self->{filehandler}->autoflush(1);
        $self->{file_name} = $file;
        return 1;
    }
    $self->{filehandler} = undef;
    print STDERR "Cannot open file $file: $!\n";
    return 0;
}
=head3 $logger->is_file_mode()

Return 1 if the logger is in file mode, 0 if it log to stdout or to syslog.
=cut
sub is_file_mode {
    my $self = shift;
    
    if ($self->{log_mode} == 1) {
        return 1;
    }
    return 0;
}
=head3 $logger->is_debug()

Return 1 if the logger severity is set to debug, 0 otherwise.
=cut
sub is_debug {
    my $self = shift;

    if ($self->{severity} == 7) {
        return 1;
    }
    return 0;
}
=head3 $logger->syslog_mode(logopt, facility)

Configure the logger to use syslog with the given options and facility.
=cut
sub syslog_mode($$$) {
    my ($self, $logopt, $facility) = @_;

    $self->{log_mode} = 2;
    openlog($0, $logopt, $facility);
    return 1;
}
=head3 $logger->redirect_output()

Redirect STDOUT and STDERR to the log file if in file mode, otherwise do nothing.
=cut
# For daemons
sub redirect_output {
    my $self = shift;

    if ($self->is_file_mode()) {
        open my $lfh, '>>', $self->{file_name};
        open STDOUT, '>&', $lfh;
        open STDERR, '>&', $lfh;
    }
}
=head3 $logger->flush_output()

Bypass the buffers set up by the kernel/file system and always write the log as soon as it is sent.
See https://perldoc.perl.org/perlvar#$%7C
=cut
sub flush_output {
    my ($self, %options) = @_;

    $| = 1 if (defined($options{enabled}));
}

=head3 $logger->force_default_severity(severity => $severity)

set "default" severity (used by set_default_severity).
=cut
sub force_default_severity {
    my ($self, %options) = @_;

    $self->{old_severity} = defined($options{severity}) ? $options{severity} : $self->{severity};
}

sub set_default_severity {
    my $self = shift;

    $self->{severity} = $self->{old_severity};
}
=head3 $logger->severity([severity])

if severity is given, set the log severity to the given value. can be a number between 2 and 7 or one of the following strings documented at the top of the document (case insensitive):

if no severity is given, return the current log severity as a string.
=cut
# Getter/Setter Log severity
sub severity {
    my $self = shift;
    if (@_) {
        my $input_severity = lc($_[0]);
        my $save_severity = $self->{severity};
        if ($input_severity =~ /^[0234567]$/) {
            $self->{severity} = $input_severity;
        } elsif ($input_severity eq "none") {
            $self->{severity} = 0;
        } elsif ($input_severity eq "fatal") {
            $self->{severity} = 2;
        } elsif ($input_severity eq "error") {
            $self->{severity} = 3;
        } elsif ($input_severity eq "warning") {
            $self->{severity} = 4;
        } elsif ($input_severity eq "notice") {
            $self->{severity} = 5;
        } elsif ($input_severity eq "info") {
            $self->{severity} = 6;
        } elsif ($input_severity eq "debug") {
            $self->{severity} = 7;
        } else {
            $self->writeLogError("Wrong severity value set.");
            return -1;
        }
        $self->{old_severity} = $save_severity;
    }
    return $human_severities{$self->{severity}};
}
=head3 configuration of log message format

each following methods are used to configure the log message format.

=head4 $logger->withpid([1|0])

=head4 $logger->withdate([1|0])

Append the process id and/or the date at the start of each log message.
=cut
sub withpid {
    my $self = shift;
    if (@_) {
        if ($_[0]){
        $self->{withpid} = 1;
        }else{
        $self->{withpid} = 0;
        }

    }
    return $self->{withpid};
}

sub withdate {
    my $self = shift;
    if (@_) {
        $self->{withdate} = $_[0];
    }
    return $self->{withdate};
}

sub get_date {
    my $self = shift;
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time());
    return sprintf("%04d-%02d-%02d %02d:%02d:%02d",
                   $year+1900, $mon+1, $mday, $hour, $min, $sec);
}
=head2 $logger->writeLog(severity, "msg")

Internal method to write a log message with the given severity.
You should use the writeLogXxxx methods instead, there only difference is that the severity parameter is set for you.
=cut
sub writeLog($$$%) {
    my ($self, $severity, $msg, %options) = @_;

    # do nothing if the configured severity does not imply logging this message
    return if ($self->{severity} < $severity);
    if (length($msg) > 20000) {
        $msg = substr($msg, 0, 20000) . '...';
    }
    $msg = ($self->withpid()) ? "$$ - $msg" : $msg;

    my $datedmsg = $human_severities{$severity} . " - " . $msg . "\n";
    if ($self->withdate()) {
        $datedmsg = $self->get_date . " - " . $datedmsg;
    }
    if ($self->{log_mode} == 1 and defined($self->{filehandler})) {
        print {$self->{filehandler}} $datedmsg;
    } elsif ($self->{log_mode} == 0) {
        print $datedmsg;
    } elsif ($self->{log_mode} == 2) {
        syslog($severity, $msg);
    } else {
        print STDERR "Unknown log mode '$self->{log_mode}' or log file unavailable for the following log :\n $datedmsg\n";
    }
}
=head3 $logger->writeLogDebug("msg")
=cut
sub writeLogDebug {
    shift->writeLog(7, @_);
}
=head3 $logger->writeLogInfo("msg")
=cut
sub writeLogInfo {
    shift->writeLog(6, @_);
}
=head3 $logger->writeLogNotice("msg")
=cut
sub writeLogNotice {
    shift->writeLog(5, @_);
}
=head3 $logger->writeLogWarning("msg")
=cut
sub writeLogWarning {
    shift->writeLog(4, @_);
}
=head3 $logger->writeLogError("msg")
=cut
sub writeLogError {
    shift->writeLog(3, @_);
}
=head3 $logger->writeLogFatal("msg")
=cut
sub writeLogFatal {
    shift->writeLog(2, @_);
    die("FATAL: " . $_[0] . "\n");
}

sub DESTROY {
    my $self = shift;
    
    if (defined $self->{filehandler}) {
        $self->{filehandler}->close();
    }
}

1;
