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

package gorgone::modules::centreon::broker::class;

use base qw(gorgone::class::module);

use strict;
use warnings;
use gorgone::standard::library;
use gorgone::class::sqlquery;
use ZMQ::LibZMQ4;
use ZMQ::Constants qw(:all);
use File::Path qw(make_path);
use JSON::XS;
use Time::HiRes;

my $result;
my %handlers = (TERM => {}, HUP => {});
my ($connector);

sub new {
    my ($class, %options) = @_;

    $connector  = {};
    $connector->{internal_socket} = undef;
    $connector->{module_id} = $options{module_id};
    $connector->{logger} = $options{logger};
    $connector->{config} = $options{config};
    $connector->{config_core} = $options{config_core};
    $connector->{config_db_centreon} = $options{config_db_centreon};
    $connector->{stop} = 0;

    bless $connector, $class;
    $connector->set_signal_handlers();
    return $connector;
}

sub set_signal_handlers {
    my $self = shift;

    $SIG{TERM} = \&class_handle_TERM;
    $handlers{TERM}->{$self} = sub { $self->handle_TERM() };
    $SIG{HUP} = \&class_handle_HUP;
    $handlers{HUP}->{$self} = sub { $self->handle_HUP() };
}

sub handle_HUP {
    my $self = shift;
    $self->{reload} = 0;
}

sub handle_TERM {
    my $self = shift;
    $self->{logger}->writeLogInfo("[broker] $$ Receiving order to stop...");
    $self->{stop} = 1;
}

sub class_handle_TERM {
    foreach (keys %{$handlers{TERM}}) {
        &{$handlers{TERM}->{$_}}();
    }
}

sub class_handle_HUP {
    foreach (keys %{$handlers{HUP}}) {
        &{$handlers{HUP}->{$_}}();
    }
}

sub get_broker_stats_collection_flag {
    my ($self, %options) = @_;

    my ($status, $datas) = $self->{class_object_centreon}->custom_execute(
        request => "SELECT `value` FROM options WHERE `key` = 'enable_broker_stats'",
        mode => 2
    );
    if ($status == -1 || !defined($datas->[0][0])) {
        $self->{logger}->writeLogError('[broker] Cannot get Broker statistics collection flag');
        return -1;
    }
    
    return $datas->[0][0];
}

sub action_brokerstats {
    my ($self, %options) = @_;

    $options{token} = $self->generate_token() if (!defined($options{token}));

    $self->send_log(
        code => gorgone::class::module::ACTION_BEGIN,
        token => $options{token},
        data => {
            message => 'action brokerstats starting'
        }
    );

    if ($self->get_broker_stats_collection_flag() < 1) {
        $self->send_log(
            code => gorgone::class::module::ACTION_FINISH_OK,
            token => $options{token},
            data => {
                message => 'no collection configured'
            }
        );
        $self->{logger}->writeLogInfo("[broker] No Broker statistics collection configured");
        return 0;
    }

    my $request = "SELECT id, cache_directory, config_name FROM cfg_centreonbroker " .
        "JOIN nagios_server " .
        "WHERE ns_activate = '1' AND stats_activate = '1' AND ns_nagios_server = id";

    if (defined($options{data}->{variables}[0]) && $options{data}->{variables}[0] =~ /\d+/) {
        $request .= " AND id = '" . $options{data}->{variables}[0] . "'";
    }
    
    if (!defined($options{data}->{content}->{collect_localhost}) ||
        $options{data}->{content}->{collect_localhost} eq 'false') {
        $request .= " AND localhost = '0'";
    }

    my ($status, $datas) = $self->{class_object_centreon}->custom_execute(request => $request, mode => 2);
    if ($status == -1) {
        $self->send_log(
            code => gorgone::class::module::ACTION_FINISH_KO,
            token => $options{token},
            data => {
                message => 'cannot find configuration'
            }
        );
        $self->{logger}->writeLogError("[broker] Cannot find configuration");
        return 1;
    }

    my %targets;
    foreach (@{$datas}) {
        my $target = $_->[0];
        my $statistics_file = $_->[1] . "/" . $_->[2] . "-stats.json";
        $self->{logger}->writeLogInfo(
            "[broker] Collecting file '" . $statistics_file . "' on target '" . $target . "'"
        );
        $self->send_internal_action(
            target => $target,
            action => 'COMMAND',
            token => $options{token},
            data => {
                content => [ 
                    {
                        command => 'cat ' . $statistics_file,
                        timeout => $options{data}->{content}->{timeout},
                        metadata => {
                            poller_id => $target,
                            config_name => $_->[2],
                        }
                    }
                ]
            }
        );
        $targets{$target} = 1;
    }
    
    my $wait = (defined($self->{config}->{command_wait})) ? $self->{config}->{command_wait} : 1_000_000;
    Time::HiRes::usleep($wait);
    
    foreach my $target (keys %targets) {
        $self->send_internal_action(
            target => $target,
            action => 'GETLOG',
        );
    }

    $wait = (defined($self->{config}->{sync_wait})) ? $self->{config}->{sync_wait} : 1_000_000;
    Time::HiRes::usleep($wait);

    $self->send_log(
        code => $self->ACTION_FINISH_OK,
        token => $options{token},
        data => {
            message => 'action brokerstats finished'
        }
    );
    
    $self->send_internal_action(
        action => 'GETLOG',
        data => {
            token => $options{token}
        }
    );
    return 0;
}

sub write_stats {
    my ($self, %options) = @_;

    return if (!defined($options{data}->{data}->{action}) || $options{data}->{data}->{action} ne "getlog" &&
        defined($options{data}->{data}->{result}));

    foreach my $entry (@{$options{data}->{data}->{result}}) {
        my $data = JSON::XS->new->utf8->decode($entry->{data});
        next if (!defined($data->{result}->{exit_code}) || $data->{result}->{exit_code} != 0 ||
            !defined($data->{metadata}->{poller_id}) || !defined($data->{metadata}->{config_name}));

        my $dest_dir = $self->{config}->{cache_dir} . '/' . $data->{metadata}->{poller_id};
        make_path($dest_dir) if (! -d $dest_dir);
        my $dest_file = $dest_dir . '/' . $data->{metadata}->{config_name} . '.json';
        $self->{logger}->writeLogDebug("[broker] Writing file '" . $dest_file . "'");
        open(FH, '>', $dest_file);
        print FH $data->{result}->{stdout};
        close(FH);
    }
}

sub event {
    while (1) {
        my $message = gorgone::standard::library::zmq_dealer_read_message(socket => $connector->{internal_socket});
        
        $connector->{logger}->writeLogDebug("[broker] Event: $message");
        if ($message =~ /^\[ACK\]\s+\[(.*?)\]\s+(.*)$/m) {
            my $token = $1;
            my $data = JSON::XS->new->utf8->decode($2);
            my $method = $connector->can('write_stats');
            $method->($connector, data => $data);
        } else {
            $message =~ /^\[(.*?)\]\s+\[(.*?)\]\s+\[.*?\]\s+(.*)$/m;
            if ((my $method = $connector->can('action_' . lc($1)))) {
                $message =~ /^\[(.*?)\]\s+\[(.*?)\]\s+\[.*?\]\s+(.*)$/m;
                my ($action, $token) = ($1, $2);
                my $data = JSON::XS->new->utf8->decode($3);
                $method->($connector, token => $token, data => $data);
            }
        }

        last unless (gorgone::standard::library::zmq_still_read(socket => $connector->{internal_socket}));
    }
}

sub run {
    my ($self, %options) = @_;

    # Connect internal
    $connector->{internal_socket} = gorgone::standard::library::connect_com(
        zmq_type => 'ZMQ_DEALER',
        name => 'gorgonebroker',
        logger => $self->{logger},
        type => $self->{config_core}->{internal_com_type},
        path => $self->{config_core}->{internal_com_path}
    );
    $connector->send_internal_action(
        action => 'BROKERREADY',
        data => {}
    );

    $self->{db_centreon} = gorgone::class::db->new(
        dsn => $self->{config_db_centreon}->{dsn},
        user => $self->{config_db_centreon}->{username},
        password => $self->{config_db_centreon}->{password},
        force => 2,
        logger => $self->{logger}
    );
    $self->{class_object_centreon} = gorgone::class::sqlquery->new(
        logger => $self->{logger},
        db_centreon => $self->{db_centreon}
    );

    $self->{poll} = [
        {
            socket  => $connector->{internal_socket},
            events  => ZMQ_POLLIN,
            callback => \&event,
        }
    ];

    if (defined($self->{config}->{cron})) {
        $self->send_internal_action(
            action => 'ADDCRON', 
            data => {
                content => $self->{config}->{cron},
            }
        );
    }

    while (1) {
        # we try to do all we can
        my $rev = zmq_poll($self->{poll}, 5000);
        if (defined($rev) && $rev == 0 && $self->{stop} == 1) {
            $self->{logger}->writeLogInfo("[broker] $$ has quit");
            zmq_close($connector->{internal_socket});
            exit(0);
        }
    }
}

1;
