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
    $connector->{logger}->writeLogDebug("[pullwss] read message from internal, sending to remote node : $options{message}");
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
# take a ref string as parameter representing a zmq message, and send it to the parent node through the websocket if needed.
# Only send back GETLOGS/SETLOGS and PONG, other message are not transmitted.
# GETLOG are transformed into SETLOGS message, and the result is split into smaller messages if needed.
sub transmit_back {
    my ($self, %options) = @_;

    return undef if (!defined($options{message}));

    if (${$options{message}} =~ /^\[ACK\]\s+\[(.*?)\]\s+(.*)/m) {
        # message received by pullwss from the core. If it's a getlog message, we may need to split it into smaller messages
        my $msg_recv;
        eval {
            $msg_recv = JSON::XS->new->decode($2);
        };
        if ($@) {
            # sending it as is if decoding fail, so it will be logged on central and on poller.
            $connector->send_message(message => ${$options{message}});
        }

        if (defined($msg_recv->{data}->{action}) && $msg_recv->{data}->{action} eq 'getlog') {
            # websocket have a size limit on each message, so we need to split the response if it's too big
            # For now we split only for a getlog response (which become a setlog message here for an unknown reason).
            # max_msg_size is now a parameter with a default value set in pullwss::hooks::register()
            my $token = $1;
            my $max_msg_size = $self->{config}->{max_msg_size};
            my $msg_header   = "[SETLOGS] [$token] [] ";
            my $size         = length($msg_header);  # Size of current message to be sent.
            my @msg_to_send  = (); # keep all messages to send to add the total number of message to each one.
            my $logs = $msg_recv->{data}->{result};
            # don't want to change the data of the message except the number of logs per message.
            # So we need to keep the original message and use it to construct each new smaller message to send.
            # result is an arrayref, it will be filled just before encoding to json the object, and the ref is deleted just after.
            # When size of logs is over the limit, use a new message.
            $msg_recv->{data}->{result} = undef;
            my $nb_msg = 0; # This will keep track of the number of message we have to send.
            # Note that the msg 0 is the first message, so to get the number of message you should add 1.

            for my $log (@{$logs}) {
                 if (get_len_msg($log) > $max_msg_size) {
                    $self->{logger}->writeLogError("[pullwss] [$token] cannot send log message created at " .
                        $log->{ctime} . ', too big : ' . get_len_msg($log) . ' > ' . $max_msg_size);
                    next;
                }
                if ($size + get_len_msg($log) > $max_msg_size) {
                    $nb_msg++;
                    $size = length($msg_header);
                }
                push(@{$msg_to_send[$nb_msg]}, $log);
                $size += get_len_msg($log);
            }

            if (scalar(@{$logs}) <= 0) {
                $self->{logger}->writeLogDebug("[pullwss] [$token] getlog message included no logs, sending an empty response");
                $msg_recv->{data}->{nb_total_msg} = $nb_msg + 1;
                $msg_recv->{data}->{result} = [];
                my $message = $msg_header . encode_json($msg_recv);
                $self->send_message(message => $message);
            } else {
                $self->{logger}->writeLogDebug("[pullwss]  [$token] getlog message included "
                    . scalar(@{$logs}) . " logs, splitting into $nb_msg messages, last log is " . $size . " character long");

                # let's send each message. @msg_to_send contains a list of payload (logs) to send.
                # $payload is a reference to the array of logs to send for this particular message.
                foreach my $payload (@msg_to_send) {
                    $msg_recv->{data}->{nb_total_msg} = $nb_msg + 1;
                    $msg_recv->{data}->{result} = $payload;
                    my $message = $msg_header . encode_json($msg_recv);
                    $self->send_message(message => $message);
                }
            }
        }
    } elsif (${$options{message}} =~ /^\[BCASTCOREKEY\]\s+\[.*?\]\s+\[.*?\]\s+(.*)/m) {
        my $data;
        eval {
            $data = JSON::XS->new->decode($1);
        };
        if ($@) {
            $connector->{logger}->writeLogDebug("[pullwss] cannot decode BCASTCOREKEY: $@");
            return undef;
        }
        $connector->action_bcastcorekey(data => $data);

    } elsif (${$options{message}} =~ /^\[(PONG|SYNCLOGS)\]/) {
        $self->send_message(message => ${$options{message}});
    }
}
sub get_len_msg {
    my $msg = shift;
    my $len = 104; # a message with empty token and data take around this size, then we add up the data and token field size.
    if (defined($msg) and ref($msg) eq "HASH") {
        if (defined($msg->{data})) {
            $len = $len + length($msg->{data});
        }
        if (defined($msg->{token})){
            $len = $len + length($msg->{token});
        }
    }
    return $len;
}
sub read_zmq_events {
    my ($self, %options) = @_;

    while (!$self->{stop} and $self->{internal_socket}->has_pollin()) {
        my ($message) = $connector->read_message();

        # on the worst case $message could be huge (all of gorgone_history data from the sqlite db for example).
        # So this is a passby reference to avoid copying the data.
        # the format of the getlog message stop us from using only reference inside the transmit_back function.
        $self->transmit_back(message => \$message);


    }
}

1;
