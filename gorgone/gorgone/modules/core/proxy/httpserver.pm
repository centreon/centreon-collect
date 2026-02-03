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

package gorgone::modules::core::proxy::httpserver;

use base qw(gorgone::class::module);

use strict;
use warnings;
use gorgone::standard::library;
use gorgone::standard::constants qw(:all);
use gorgone::standard::misc;
use centreon::common::centreonvault;
use Mojolicious::Lite;
use Mojo::Server::Daemon;
use IO::Socket::SSL;
use IO::Handle;
use JSON::XS;
use IO::Poll qw(POLLIN POLLPRI);
use EV;
use HTML::Entities;

my %handlers = (TERM => {}, HUP => {});
my ($connector);

websocket '/' => sub {
    my $mojo = shift;

    $connector->{logger}->writeLogDebug('[proxy] httpserver websocket client connected: ' . $mojo->tx->connection);

    $connector->{ws_clients}->{ $mojo->tx->connection } = {
        tx => $mojo->tx,
        logged => 0,
        last_update => time(),
        authorization => $mojo->tx->req->headers->header('authorization')
    };

    $mojo->on(message => sub {
        my ($mojo, $msg) = @_;

        $msg =  HTML::Entities::decode_entities($msg);

        $connector->{ws_clients}->{ $mojo->tx->connection }->{last_update} = time();

        $connector->{logger}->writeLogDebug("[proxy] httpserver receiving message: " . $msg);

        my $rv = $connector->is_logged_websocket(ws_id => $mojo->tx->connection, data => $msg);
        return if ($rv == 0);

        read_message_client(data => $msg);
    });

    $mojo->on(finish => sub {
        my ($mojo, $code, $reason) = @_;

        $connector->{logger}->writeLogDebug('[proxy] httpserver websocket client disconnected: ' . $mojo->tx->connection);
        $connector->clean_websocket(ws_id => $mojo->tx->connection, finish => 1);
    });
};

sub construct {
    my ($class, %options) = @_;
    $connector = $class->SUPER::new(%options);
    bless $connector, $class;

    $connector->{ws_clients} = {};
    $connector->{identities} = {};
    $connector->{nodes} = {}; # store nodes info from node module which take it from centreon DB.

    # didn't sent the vault object to avoid duplicating connection and any unforeseen concurrency issue.
    # should probably be passed as an object already constructed
    $connector->{vault} = centreon::common::centreonvault->new(
        logger => $options{logger},
        config_file => $options{config_core}->{vault_file});
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
    $self->{logger}->writeLogDebug("[proxy] $$ Receiving order to stop...");
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

sub run {
    my ($self, %options) = @_;

    my $listen = 'reuse=1';
    if ($self->{config}->{httpserver}->{ssl} eq 'true') {
        if (!defined($self->{config}->{httpserver}->{ssl_cert_file}) || $self->{config}->{httpserver}->{ssl_cert_file} eq '' ||
            ! -r "$self->{config}->{httpserver}->{ssl_cert_file}") {
            $connector->{logger}->writeLogError("[proxy] httpserver cannot read/find ssl-cert-file");
            exit(1);
        }
        if (!defined($self->{config}->{httpserver}->{ssl_key_file}) || $self->{config}->{httpserver}->{ssl_key_file} eq '' ||
            ! -r "$self->{config}->{httpserver}->{ssl_key_file}") {
            $connector->{logger}->writeLogError("[proxy] httpserver cannot read/find ssl-key-file");
            exit(1);
        }
        $listen .= '&cert=' . $self->{config}->{httpserver}->{ssl_cert_file} . '&key=' . $self->{config}->{httpserver}->{ssl_key_file};
    }
    my $proto = 'http';
    if ($self->{config}->{httpserver}->{ssl} eq 'true') {
        $proto = 'https';
        if (defined($self->{config}->{httpserver}->{passphrase}) && $self->{config}->{httpserver}->{passphrase} ne '') {
            IO::Socket::SSL::set_defaults(SSL_passwd_cb => sub { return $connector->{config}->{httpserver}->{passphrase} } );
        }
    }

    $self->{internal_socket} = gorgone::standard::library::connect_com(
        context => $self->{zmq_context},
        zmq_type => 'ZMQ_DEALER',
        name => 'gorgone-proxy-httpserver',
        logger => $self->{logger},
        type => $self->get_core_config(name => 'internal_com_type'),
        path => $self->get_core_config(name => 'internal_com_path')
    );
    $self->send_internal_action({
        action => 'PROXYREADY',
        data => {
            httpserver => 1
        }
    });
    $self->read_zmq_events();

    my $type = ref(Mojo::IOLoop->singleton->reactor);
    my $watcher_io;
    if ($type eq 'Mojo::Reactor::Poll') {
        Mojo::IOLoop->singleton->reactor->{io}{ $self->{internal_socket}->get_fd()} = {
            cb => sub { $connector->read_zmq_events(); },
            mode => POLLIN | POLLPRI
        };
    }  else {
        # need EV version 4.32
        $watcher_io = EV::io(
            $self->{internal_socket}->get_fd(),
            EV::READ,
            sub {
                $connector->read_zmq_events();
            }
        );
    }

    #my $socket_fd = $self->{internal_socket}->get_fd();
    #my $socket = IO::Handle->new_from_fd($socket_fd, 'r');
    #Mojo::IOLoop->singleton->reactor->io($socket => sub {
    #    $connector->read_zmq_events();
    #});
    #Mojo::IOLoop->singleton->reactor->watch($socket, 1, 0);

    Mojo::IOLoop->singleton->recurring(60 => sub {
        $connector->{logger}->writeLogDebug('[proxy] httpserver recurring timeout loop');
        my $ctime = time();
        foreach my $ws_id (keys %{$connector->{ws_clients}}) {
            if (($ctime - $connector->{ws_clients}->{$ws_id}->{last_update}) > 300) {
                $connector->{logger}->writeLogDebug('[proxy] httpserver websocket client timeout reached: ' . $ws_id);
                $connector->close_websocket(
                    code => 500,
                    message  => 'timeout reached',
                    ws_id => $ws_id
                );
            }
        }
    });

    app->mode('production');
    my $daemon = Mojo::Server::Daemon->new(
        app    => app,
        listen => [$proto . '://' . $self->{config}->{httpserver}->{address} . ':' . $self->{config}->{httpserver}->{port} . '?' . $listen]
    );
    $daemon->inactivity_timeout(180);

    # now that httpserver is ready, we need to tell nodes module to send us the data of every nodes configured.
    # there is now way for nodes module to wait on this module as it is optional, and if httpserver start after nodes, messages will be lost
    # and httpserver will refuse connexion until nodes next retrival (30min)
    $self->send_internal_action({
        action => 'CENTREONNODESSYNC',
        data => {}}
    );
    $daemon->run();

    exit(0);
}

sub read_message_client {
    my (%options) = @_;

    if ($options{data} =~ /^\[PONG\]/) {
        return undef if ($options{data} !~ /^\[(.+?)\]\s+\[(.*?)\]\s+\[.*?\]\s+(.*)/m);

        my ($action, $token) = ($1, $2);
        my ($rv, $data) = $connector->json_decode(argument => $3);
        return undef if ($rv == 1);

        $connector->send_internal_action({
            action => 'PONG',
            data => $data,
            token => $token,
            target => ''
        });
        $connector->read_zmq_events();
    } elsif ($options{data} =~ /^\[(?:REGISTERNODES|UNREGISTERNODES|SYNCLOGS|SETLOGS)\]/) {
        return undef if ($options{data} !~ /^\[(.+?)\]\s+\[(.*?)\]\s+\[.*?\]\s+(.*)/ms);

        my ($action, $token, $data)  = ($1, $2, $3);

        $connector->send_internal_action({
            action => $action,
            data => $data,
            data_noencode => 1,
            token => $token,
            target => ''
        });
        $connector->read_zmq_events();
    }
}
=head3 action_proxyaddnode($zmq_message)

zmq_message : PROXYADDNODE message type, received from internal zmq socket to decode and process.
    expect an array of nodes with id, token, type (connection type as in 'pullwss')

If the message is valid, update the internal state to allow new nodes connect, and disconnect any node not existing anymore

Return : 1 in case of failure, 0 in case of success
=cut
sub action_proxyaddnode {
    my $self = shift;
    my $data = shift;
    my ($status, $nodes) = $self->json_decode(argument => $data);
        if ($status == 1) {
            $self->{logger}->writeLogError("Can't decode a proxyaddnode message data : " . $data);
            return 1;
    }
    # let's loop on the nodes and delete any non wss. if token is undef it mean message don't come from the nodes module, so we keep the old token.
    # if you want to remove the token to use the default one from the conf use "" instead of undef.
    my $temp_nodes = {};
    for my $node (@{$nodes}){
        next if $node->{type} !~ /wss/;
        $node->{token} = $self->{vault}->get_secret($node->{token});
        my $ws_id = $self->{identities}->{ $node->{id} };

        if (!defined($node->{token})) {
            if (!is_empty($self->{nodes}->{ $node->{id} })) {
                $node->{token} = $self->{nodes}->{ $node->{id} }->{token};
            }
        } elsif ($ws_id and (
            defined($self->{nodes}->{ $node->{id} }->{token}) and $self->{nodes}->{ $node->{id} }->{token} ne $node->{token}
              or !defined($self->{nodes}->{ $node->{id} }->{token}) and defined($node->{token}) )){
            $self->{ws_clients}->{ $ws_id }->{tx}->finish();
            $self->{ws_clients}->{$ws_id}->{logged} = 0;
            $self->{logger}->writeLogInfo("[proxy-httpserver] node already connected but token changed, disconnecting client " . $ws_id );
            # now distant node should get a disconnect, and try to reconnect by itself, sending a registernode message to authenticate properly.
        }
        $self->{logger}->writeLogInfo("[proxy-httpserver] adding node " . $node->{id} . " as pullwss." );
        $temp_nodes->{$node->{id}} = $node;
    }
    # disconnect every node that don't exist anymore.
    for my $delete_node (keys %{$self->{nodes}}){
        next if $temp_nodes->{$delete_node};

        my $ws_id = $self->{identities}->{ $delete_node };
        $self->{ws_clients}->{ $ws_id }->{tx}->finish();
        $self->{logger}->writeLogInfo("[proxy-httpserver] node " . $delete_node . " don't exist anymore, disconnecting client " . $ws_id );
    }

    $self->{nodes} = $temp_nodes;
    return 0;
}
=head3 action_proxydelnode($zmq_message)

zmq_message : PROXYDELNODE message type, received from internal zmq socket to decode and process.

If the message is valid, update the internal state to remove this node, or log an error if the message is not valid.

Return :
* 0 if node was successfully deleted from local state.
* 1 if message can't be decoded
* 2 if message can be decoded but node don't exist in local state
=cut
sub action_proxydelnode {
    my $self = shift;
    my $data = shift;
    my ($status, $node) = $self->json_decode(argument => $data);
        if ($status == 1) {
            $self->{logger}->writeLogError("Can't decode a proxydelnode message data : " . $data);
            return 1;
        }
        if ($self->{nodes}->{ $node->{id} }){
            $self->{logger}->writeLogDebug("[proxy-httpserver] deleting node ". $node->{id} . " from pullwss." );
            delete($self->{nodes}->{ $node->{id} });
            return 0;
        }else {
            $self->{logger}->writeLogInfo("[proxy-httpserver] tried to delete node " . $node->{id} . " which don't exist, ignoring it." );
            return 2;
        }

}
=head3 proxy(message => $message)

message : message received from internal zmq socket.

process the internal messages(BCASTLOGGER, BCASTCOREKEY, PROXYADDNODE) or forward it to the distant node by searching in the message the target.

=cut
sub proxy {
    my (%options) = @_;
    
    return undef if ($options{message} !~ /^\[(.+?)\]\s+\[(.*?)\]\s+\[(.*?)\]\s+(.*)$/m);

    my ($action, $token, $target_complete, $data) = ($1, $2, $3, $4);
    $connector->{logger}->writeLogDebug(
        "[proxy] httpserver send message: [action = $action] [token = $token] [target = $target_complete] [data = $data]"
    );

    if ($action eq 'BCASTLOGGER' && $target_complete eq '') {
        (undef, $data) = $connector->json_decode(argument => $data);
        $connector->action_bcastlogger(data => $data);
        return ;
    } elsif ($action eq 'BCASTCOREKEY' && $target_complete eq '') {
        (undef, $data) = $connector->json_decode(argument => $data);
        $connector->action_bcastcorekey(data => $data);
        return ;
    } elsif ($action eq 'PROXYADDNODE' && $target_complete eq '') {
        return $connector->action_proxyaddnode($data);
    } elsif ($action eq 'PROXYDELNODE' && $target_complete eq '') {
        return $connector->action_proxydelnode($data);
    }

    if ($target_complete !~ /^(.+)~~(.+)$/) {
        $connector->send_log(
            code => GORGONE_ACTION_FINISH_KO,
            token => $token,
            data => {
                message => "unknown target format '$target_complete'"
            }
        );
        $connector->read_zmq_events();
        return ;
    }

    my ($target_client, $target, $target_direct) = ($1, $2, 1);
    if ($target_client ne $target) {
        $target_direct = 0;
    }

    if (!defined($connector->{identities}->{$target_client})) {
        $connector->send_log(
            code => GORGONE_ACTION_FINISH_KO,
            token => $token,
            data => {
                message => "cannot get connection for target node '$target_client'"
            }
        );
        $connector->read_zmq_events();
        return ;
    }

    my $message = gorgone::standard::library::build_protocol(
        action => $action,
        token => $token,
        target => $target_direct == 0 ? $target : undef,
        data => $data
    );

    $connector->{ws_clients}->{ $connector->{identities}->{$target_client} }->{tx}->send({text => $message});
}

sub read_zmq_events {
    my ($self, %options) = @_;

    while ($self->{internal_socket}->has_pollin()) {

        my ($message) = $connector->read_message();
        proxy(message => $message);
    }
}
sub is_empty {
    my $value = shift;
    if (!defined($value) or $value eq '') {
        return 1;
    }
    return 0;
}
=head3 $self->is_token_ok(ws_id => $ws_id, data => $data)

validate a client sent the correct token/node Id couple to authenticate.

Token validation logic :
check in the local state if node id exist and get token from there.
If token don't exist, use default one from yaml config file
if db token is set, don't allow default one for this particular node.

The "local state" is filled by PROXYADDNODE messages sent by nodes modules, and processed by the proxy function.
=cut
sub is_token_ok {
    my ($self, %options) = @_;

    return 0 if (!defined($self->{ws_clients}->{ $options{ws_id} }->{authorization})
        or $self->{ws_clients}->{ $options{ws_id} }->{authorization} !~ /^\s*Bearer\s+(.*?)\s*$/);
    my $client_token = $1;
    return 0 if (!defined($client_token) || $client_token eq '');

    if ($options{data} !~ /^\[REGISTERNODES\]\s+\[(?:.*?)\]\s+\[.*?\]\s+(.*)/ms) {
        return 0;
    }
    my ($status,$json) = $self->json_decode(argument => $1);
    return 0 if ($status == 1);
    if (is_empty($json->{nodes}->[0]->{id})){
        $self->{logger}->writeLogDebug("[proxy-httpserver] cannot find node info for client " . $options{ws_id});
    }
    my $client_id = $json->{nodes}->[0]->{id};
    $client_id or return 0;
    if (!defined($self->{nodes}->{ $client_id })) {
        $self->{logger}->writeLogDebug("[proxy-httpserver] cannot find node info for id " . $client_id);
        return 0;
    }
    my $db_token = $self->{nodes}->{ $client_id }->{token};
    my $correct_token = $db_token ? $db_token : $self->{config}->{httpserver}->{token};

    if (!$client_token or $client_token ne $correct_token){
        $self->{logger}->writeLogDebug(sprintf("[proxy-httpserver] WS client '%s' as node '%s' could not be authenticated." , $options{ws_id}, $client_id ));
        return 0;
    }

    $self->{ws_clients}->{ $options{ws_id} }->{identity} = $client_id;
    $self->{identities}->{ $client_id } = $options{ws_id};
    $self->{ws_clients}->{ $options{ws_id} }->{logged} = 1;
    $self->{logger}->writeLogDebug("[proxy-httpserver] WS client '" . $options{ws_id} . "' authenticated successfully as node " . $client_id);
    return 1;
}
sub is_logged_websocket {
    my ($self, %options) = @_;

    return 1 if ($self->{ws_clients}->{ $options{ws_id} }->{logged} == 1);
    if (!$self->is_token_ok(%options)) {
        $self->close_websocket(
            code => 500,
            message  => 'authentication failed',
            ws_id => $options{ws_id}
        );
        return 0;
    }
    return 1;
}

sub clean_websocket {
    my ($self, %options) = @_;

    return if (!defined($self->{ws_clients}->{ $options{ws_id} }));

    $self->{ws_clients}->{ $options{ws_id} }->{tx}->finish() if (!defined($options{finish}));
    delete $self->{identities}->{ $self->{ws_clients}->{ $options{ws_id} }->{identity} } 
        if (defined($self->{ws_clients}->{ $options{ws_id} }->{identity}));
    delete $self->{ws_clients}->{ $options{ws_id} };
}

sub close_websocket {
    my ($self, %options) = @_;

    $self->{ws_clients}->{ $options{ws_id} }->{tx}->send({json => {
        code => $options{code},
        message  => $options{message}
    }});
    $self->clean_websocket(ws_id => $options{ws_id});
}

1;
