
package centreon::common::misc;

use strict;
use warnings;
use POSIX ":sys_wait_h";

sub read_file {
    my (%options) = @_;
 
    open my $in, '<:encoding(UTF-8)', $options{file} or return (1, "Could not open '$options{file}' for reading $!");
    local $/ = undef;
    my $all = <$in>;
    close $in;
 
    return (0, $all);
}

sub write_file {
    my (%options) = @_;
 
    open my $out, '>:encoding(UTF-8)', $options{file} or return(1, "Could not open '$options{file}' for writing $!");
    print $out $options{content};
    close $out;
 
    return (0);
}

sub mymodule_load {
    my (%options) = @_;
    my $file;
    ($file = $options{module} . ".pm") =~ s{::}{/}g;
     
    eval {
        local $SIG{__DIE__} = 'IGNORE';
        require $file;
    };
    if ($@) {
        return 1 if (defined($options{no_quit}) && $options{no_quit} == 1);
        $options{logger}->writeLogError($@);
        $options{logger}->writeLogError($options{error_msg});
        exit(1);
    }
    
    return 0;
}

sub path_errors {
    my (%options) = @_;
    
    if (@{$options{err}}) {
        for my $diag (@{$options{err}}) {
            my ($file, $message) = %$diag;
            if ($file eq '') {
                $options{logger}->writeLogError($options{prefix} . "=> $message");
            } else {
                $options{logger}->writeLogError($options{prefix} . "=> $file: $message");
            }
        }
        return 1 if (defined($options{no_quit}) && $options{no_quit} == 1);
        exit(1);
    }
    
    return 0;
}

sub chdir {
    my (%options) = @_;
    
    if (!chdir($options{dir})) {
        $options{logger}->writeLogError("Cannot chdir => $options{dir} : $!");
        exit(1);
    }
}

sub backtick {
    my %arg = (
        command => undef,
        logger => undef,
        timeout => 30,
        wait_exit => 0,
        @_,
    );
    my @output;
    my $pid;
    my $return_code;
    
    my $sig_do;
    if ($arg{wait_exit} == 0) {
        $sig_do = 'IGNORE';
        $return_code = undef;
    } else {
        $sig_do = 'DEFAULT';
    }
    local $SIG{CHLD} = $sig_do;
    
    $arg{logger}->writeLogDebug("command execution: " . $arg{command});
    if (!defined($pid = open( KID, "-|" ))) {
        $arg{logger}->writeLogError("Cant fork: $!");
        return (-1001, "Cant fork: $!", -1);
    }
    
    if ($pid) {
        eval {
           local $SIG{ALRM} = sub { die "Timeout by signal ALARM\n"; };
           alarm( $arg{timeout} );
           while (<KID>) {
               chomp;
               push @output, $_;
               $arg{logger}->writeLogDebug($_);
           }

           alarm(0);
        };
        if ($@) {
            $arg{logger}->writeLogInfo($@);
            $arg{logger}->writeLogInfo("Killing child process [$pid] ...");
            if ($pid != -1) {
                kill -9, $pid;
            }
            $arg{logger}->writeLogInfo("Killed");

            alarm(0);
            close KID;
            $arg{logger}->writeLogDebug("command end execution");
            return (-1000, join("\n", @output), -1);
        } else {
            if ($arg{wait_exit} == 1) {
                # We're waiting the exit code                
                waitpid($pid, 0);
                $return_code = $?;
            }
            close KID;
        }
    } else {
        # child
        # set the child process to be a group leader, so that
        # kill -9 will kill it and all its descendents
        setpgrp( 0, 0 );

        exec($arg{command});
        exit(127);
    }

    $arg{logger}->writeLogDebug("command end execution");
    return (0, join("\n", @output), $return_code);
}
        
1;
