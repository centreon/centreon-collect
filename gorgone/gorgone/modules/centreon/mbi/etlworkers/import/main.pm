# 
# Copyright 2019 Centreon (http://www.centreon.com/)
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

package gorgone::modules::centreon::mbi::etlworkers::import::main;

use strict;
use warnings;
use gorgone::standard::misc;
use File::Basename;
use Try::Tiny;

sub sql {
    my ($etlwk, %options) = @_;

    return if (!defined($options{params}->{sql}));

    # An action may declare its statements as opportunistic: one of them failing is then reported
    # and the next one is still attempted, instead of stopping the whole ETL run.
    my $continueOnError = defined($options{params}->{continue_on_error}) &&
        $options{params}->{continue_on_error} == 1 ? 1 : 0;

    foreach my $statement (@{$options{params}->{sql}}) {
        $etlwk->{messages}->writeLog('INFO', $statement->[0]);

        my $connection;
        if ($options{params}->{db} eq 'centstorage') {
            $connection = $etlwk->{dbbi_centstorage_con};
        } elsif ($options{params}->{db} eq 'centreon') {
            $connection = $etlwk->{dbbi_centreon_con};
        }
        next if (!defined($connection));

        if ($continueOnError == 0) {
            $connection->query({ query => $statement->[1] });
            next;
        }

        my $error;
        try {
            $connection->query({ query => $statement->[1] });
        } catch {
            $error = $_;
        };
        $etlwk->{messages}->writeLog('WARNING', $statement->[0] . ' failed: ' . $error) if (defined($error));
    }
}

sub command {
    my ($etlwk, %options) = @_;

    return if (!defined($options{params}->{command}) || $options{params}->{command} eq '');

    my ($error, $stdout, $return_code) = gorgone::standard::misc::backtick(
        command => $options{params}->{command},
        timeout => 7200,
        wait_exit => 1,
        redirect_stderr => 1,
        logger => $options{logger}
    );

    if ($error != 0) {
        die $options{params}->{message} . ": execution failed: $stdout";
    }

    $etlwk->{messages}->writeLog('INFO', $options{params}->{message});
    $etlwk->{logger}->writeLogDebug("[mbi-etlworkers] succeeded command (code: $return_code): $stdout");
}

sub load {
    my ($etlwk, %options) = @_;

    return if (!defined($options{params}->{file}));

    my ($file, $dir) = File::Basename::fileparse($options{params}->{file});

    if (! -d "$dir" && ! -w "$dir") {
        $etlwk->{messages}->writeLog('ERROR', "Cannot write into directory " . $dir);
    }

    command($etlwk, params => { command => $options{params}->{dump}, message => $options{params}->{message} });

    if ($options{params}->{db} eq 'centstorage') {
        $etlwk->{dbbi_centstorage_con}->query({ query => $options{params}->{load} });
    } elsif ($options{params}->{db} eq 'centreon') {
        $etlwk->{dbbi_centreon_con}->query({ query => $options{params}->{load} });
    }

    unlink($options{params}->{file});
}

1;
