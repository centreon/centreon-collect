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

package gorgone::modules::core::pullwss::class;

use base qw(gorgone::class::module);

use strict;
use warnings;
use gorgone::standard::library;
use gorgone::standard::constants qw(:all);
use gorgone::standard::misc;
use Mojo::UserAgent;
use Mojo::IOLoop::Signal;
use IO::Socket::SSL;
use IO::Handle;
use JSON::XS;
use EV;
use HTML::Entities;

my %handlers = (TERM => {}, HUP => {});
my ($connector);

sub new {
    my ($class, %options) = @_;
    $connector = $class->SUPER::new(%options);
    bless $connector, $class;

    $connector->{ping_timer} = -1;
    $connector->{connected} = 0;
    $connector->{stop} = 0;

    $connector->set_signal_handlers();
    return $connector;
}

sub set_signal_handlers {
    my $self = shift;
    # see https://metacpan.org/pod/EV#PERL-SIGNALS
    # EV and Mojo::IOLoop don't seem to work in this module for setting a signal handler.
    Mojo::IOLoop::Signal->on(TERM => sub { $self->handle_TERM() });
    Mojo::IOLoop::Signal->on(HUP => sub { $self->handle_HUP() });

}

sub handle_HUP {
    my $self = shift;
    $self->{reload} = 0;
}

sub handle_TERM {
    my $self = shift;
    $self->{logger}->writeLogDebug("[pullwss] $$ Receiving order to stop...");
    $self->{stop} = 1;
    
    my $message = gorgone::standard::library::build_protocol(
        action => 'UNREGISTERNODES',
        data => {
            nodes => [
                {
                    id => $self->get_core_config(name => 'id'),
                    type => 'wss',
                    identity => $self->get_core_config(name => 'id')
                }
            ]
        },
        json_encode => 1
    );

    if ($self->{connected} == 1) {
        # if the websocket is still connected, we send a message to the other end so it know we are shutting down
        # And we say to mojo to stop when he don't have other message to process.
        $self->{logger}->writeLogDebug("[pullwss] sending UNREGISTERNODES message to central before quiting as we are still connected to them.");
        $self->{tx}->send( {text => $message });

        $self->{tx}->on(drain => sub {
            $self->{logger}->writeLogDebug("[pullwss] starting the stop_gracefully mojo sub");
            Mojo::IOLoop->stop_gracefully()
        });
    }
    else {
        # if the websocket is not connected, we simply remove zmq socket and shutdown
        # we need to shutdown the zmq socket ourself or there is a c++ stack trace error in the log.
        disconnect_zmq_socket_and_exit();
    }
}

sub disconnect_zmq_socket_and_exit {
    $connector->{logger}->writeLogDebug("[pullwss] removing zmq socket :  $connector->{internal_socket}");
    # Following my tests we need both close() and undef to correctly close the zmq socket
    # If we add only one of them the following error can arise after shutdown :
    # Bad file descriptor (src/epoll.cpp:73)
    $connector->{internal_socket}->close();
    undef $connector->{internal_socket};
    $connector->{logger}->writeLogInfo("[pullwss] exit now.");
    exit(0);
}

sub send_message {
    my ($self, %options) = @_;
    my $message = HTML::Entities::encode_entities($options{message});
    $self->{tx}->send({text => $message });
}

sub ping {
    my ($self, %options) = @_;

    return if ($self->{ping_timer} != -1 && (time() - $self->{ping_timer}) < 30);

    $self->{ping_timer} = time();

    my $message = gorgone::standard::library::build_protocol(
        action => 'REGISTERNODES',
        data => {
            nodes => [
                {
                    id => $self->get_core_config(name => 'id'),
                    type => 'wss',
                    identity => $self->get_core_config(name => 'id')
                }
            ]
        },
        json_encode => 1
    );

    $self->{tx}->send({text => $message }) if ($self->{connected} == 1);
}

sub wss_connect {
    my ($self, %options) = @_;

    return if ($self->{stop} == 1 or $connector->{connected} == 1);

    $self->{ua} = Mojo::UserAgent->new();
    $self->{ua}->transactor->name('gorgone mojo');

    if (defined($self->{config}->{proxy}) && $self->{config}->{proxy} ne '') {
        $self->{ua}->proxy->http($self->{config}->{proxy})->https($self->{config}->{proxy});
    }

    my $proto = 'ws';
    if (defined($self->{config}->{ssl}) && $self->{config}->{ssl} eq 'true') {
        $proto = 'wss';
        $self->{ua}->insecure(1);
    }

    $self->{ua}->websocket(
        $proto . '://' . $self->{config}->{address} . ':' . $self->{config}->{port} . '/' => { Authorization => 'Bearer ' . $self->{config}->{token} } => sub {
            my ($ua, $tx) = @_;

            $connector->{tx} = $tx;
            $connector->{logger}->writeLogError('[pullwss] ' . $tx->res->error->{message}) if $tx->res->error;
            $connector->{logger}->writeLogError('[pullwss] webSocket handshake failed') and return unless $tx->is_websocket;

            $connector->{tx}->on(
                finish => sub {
                    my ($tx, $code, $reason) = @_;

                    $connector->{connected} = 0;
                    $connector->{logger}->writeLogError('[pullwss] websocket closed with status ' . $code);
                }
            );
            $connector->{tx}->on(
                message => sub {
                    my ($tx, $msg) = @_;

                    # We skip. Dont need to send it in gorgone-core
                    return undef if ($msg =~ /^\[ACK\]/);

                    if ($msg =~ /^\[.*\]/) {
                        $connector->{logger}->writeLogDebug('[pullwss] websocket message: ' . $msg);
                        $connector->send_internal_action({message => $msg});
                        $self->read_zmq_events();
                    } else {
                        $connector->{logger}->writeLogInfo('[pullwss] websocket message: ' . $msg);
                    }
                }
            );

            $connector->{logger}->writeLogInfo('[pullwss] websocket connected');
            $connector->{connected} = 1;
            $connector->{ping_timer} = -1;
            $connector->ping();
        }
    );
    $self->{ua}->inactivity_timeout(120);
}

sub run {
    my ($self, %options) = @_;

    $self->{internal_socket} = gorgone::standard::library::connect_com(
        context => $self->{zmq_context},
        zmq_type => 'ZMQ_DEALER',
        name => 'gorgone-pullwss',
        logger => $self->{logger},
        type => $self->get_core_config(name => 'internal_com_type'),
        path => $self->get_core_config(name => 'internal_com_path')
    );
    $self->send_internal_action({
        action => 'PULLWSSREADY',
        data => {}
    });
    $self->read_zmq_events();

    $self->wss_connect();

    my $socket_fd = gorgone::standard::library::zmq_getfd(socket => $self->{internal_socket});
    my $socket = IO::Handle->new_from_fd($socket_fd, 'r');
    Mojo::IOLoop->singleton->reactor->io($socket => sub {
        $connector->read_zmq_events();
    });
    Mojo::IOLoop->singleton->reactor->watch($socket, 1, 0);

    Mojo::IOLoop->singleton->recurring(60 => sub {
        if (!$connector->{stop}){
            $connector->{logger}->writeLogDebug('[pullwss] recurring timeout loop');
            $connector->wss_connect();
            $connector->ping();
        }
    });
    Mojo::IOLoop->start() unless (Mojo::IOLoop->is_running);

    disconnect_zmq_socket_and_exit();

}

sub transmit_back {
    my (%options) = @_;

    return undef if (!defined($options{message}));

    if ($options{message} =~ /^\[ACK\]\s+\[(.*?)\]\s+(.*)/m) {
        my $data;
        eval {
            $data = JSON::XS->new->decode($2);
        };
        if ($@) {
            return $options{message};
        }
        
        if (defined($data->{data}->{action}) && $data->{data}->{action} eq 'getlog') {
            return '[SETLOGS] [' . $1 . '] [] ' . $2;
        }
        return undef;
    } elsif ($options{message} =~ /^\[BCASTCOREKEY\]\s+\[.*?\]\s+\[.*?\]\s+(.*)/m) {
        my $data;
        eval {
            $data = JSON::XS->new->decode($1);
        };
        if ($@) {
            $connector->{logger}->writeLogDebug("[pull] cannot decode BCASTCOREKEY: $@");
            return undef;
        }

        $connector->action_bcastcorekey(data => $data);
        return undef;
    } elsif ($options{message} =~ /^\[(PONG|SYNCLOGS)\]/) {
        return $options{message};
    }
    return undef;
}

sub read_zmq_events {
    my ($self, %options) = @_;

    while (!$self->{stop} and $self->{internal_socket}->has_pollin()) {
        my ($message) = $connector->read_message();
        $message = transmit_back(message => $message);
        next if (!defined($message));

        # Only send back SETLOGS and PONG
        $connector->{logger}->writeLogDebug("[pullwss] read message from internal: $message");
        $connector->send_message(message => $message);
    }
}

1;
