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
use gorgone::class::tpapi::centreonv2;
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

    $connector->{logger}->writeLogDebug('[proxy-httpserver] httpserver websocket client connected: ' . $mojo->tx->connection);

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

        $connector->{logger}->writeLogDebug("[proxy-httpserver] receiving message: " . $msg);

        my $rv = $connector->is_logged_websocket(ws_id => $mojo->tx->connection, data => $msg);
        return if ($rv == 0);

        read_message_client(data => $msg);
    });

    $mojo->on(finish => sub {
        my ($mojo, $code, $reason) = @_;

        $connector->{logger}->writeLogDebug('[proxy-httpserver] websocket client disconnected: ' . $mojo->tx->connection);
        $connector->clean_websocket(ws_id => $mojo->tx->connection, finish => 1);
    });
};

sub construct {
    my ($class, %options) = @_;
    $connector = $class->SUPER::new(%options);
    bless $connector, $class;

    $connector->{ws_clients} = {};
    $connector->{identities} = {};
    $connector->{nodes} = {}; # store nodes info from module centreon/nodes which take it from centreon DB.

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

    # Initialize Centreon API connection
    $connector->{tpapi_centreonv2_name} = defined($options{config}->{tpapi_centreonv2}) && $options{config}->{tpapi_centreonv2} ne '' ?
        $options{config}->{tpapi_centreonv2} : 'centreonv2';
    $self->{tpapi_centreonv2} = gorgone::class::tpapi::centreonv2->new();
    my ($status) = $self->{tpapi_centreonv2}->set_configuration(
        config => $self->{tpapi}->get_configuration(name => $self->{tpapi_centreonv2_name}),
        logger => $self->{logger}
    );
    if ($status) {
        $self->{logger}->writeLogError('[PROXY] -is_logged_websocket - configure api centreonv2 - ' . $self->{tpapi_centreonv2}->error());
    }

    if ($self->{config}->{httpserver}->{ssl} eq 'true') {
        if (!defined($self->{config}->{httpserver}->{ssl_cert_file}) || $self->{config}->{httpserver}->{ssl_cert_file} eq '' ||
            ! -r "$self->{config}->{httpserver}->{ssl_cert_file}") {
            $connector->{logger}->writeLogError("[proxy-httpserver] cannot read/find ssl-cert-file");
            exit(1);
        }
        if (!defined($self->{config}->{httpserver}->{ssl_key_file}) || $self->{config}->{httpserver}->{ssl_key_file} eq '' ||
            ! -r "$self->{config}->{httpserver}->{ssl_key_file}") {
            $connector->{logger}->writeLogError("[proxy-httpserver] cannot read/find ssl-key-file");
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
        $connector->{logger}->writeLogDebug('[proxy-httpserver] recurring timeout loop');
        my $ctime = time();
        foreach my $ws_id (keys %{$connector->{ws_clients}}) {
            if (($ctime - $connector->{ws_clients}->{$ws_id}->{last_update}) > 300) {
                $connector->{logger}->writeLogDebug('[proxy-httpserver] websocket client timeout reached: ' . $ws_id);
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
    # there is no way for nodes module to wait on this module as it is optional, and if httpserver start after nodes, messages will be lost
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
            data   => $data,
            token  => $token,
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
=head3 action_proxyaddnode(token => $t, data => $decoded_json)

data : PROXYADDNODE message type, already decoded.
    expect an array of nodes with id, token, type (connection type as in 'pullwss')

If the message is valid, update the internal state to allow new nodes connect, and disconnect any node not existing anymore

Return : 1 in case of failure, 0 in case of success
=cut
sub action_proxyaddnode {
    my ($self, %options) = @_;
    my $nodes = $options{data};
    if ( is_empty($nodes) ) {
          $self->{logger}->writeLogError("[proxy-httpserver] Can't decode a proxyaddnode message data: no data");
          return 1;
    }
    # let's loop on the nodes and delete any non wss. if uid is undef it mean message don't come from the nodes module but from the register module.
    my $temp_nodes = {};
    for my $node (@{$nodes}){
        next if $node->{type} !~ /wss/;
        if (!$node->{uid}){
            $self->{logger}->writeLogInfo("[proxy-httpserver] No uid for the node $node->{id}, so this message might be the poller message, throwing it away.");
            return 1;
        }

        $self->{logger}->writeLogInfo("[proxy-httpserver] adding node " . $node->{id} . " as pullwss." );
        # we add the node with it's id as key
        $temp_nodes->{$node->{id}} = $node;
        # then we make a reference, using the uid as key and the same hashmap as a value.
        # this allow to transparently use both id and uid as key, without worrying about duplicate element.
        $temp_nodes->{$node->{uid}} = $temp_nodes->{$node->{id}};

    }
    # disconnect every node that don't exist anymore.
    for my $delete_node (keys %{$self->{nodes}}){
        next if $temp_nodes->{$delete_node};

        my $ws_id = $self->{identities}->{ $delete_node };
        next if !defined($ws_id);
        $self->{logger}->writeLogInfo("[proxy-httpserver] node " . $delete_node . " don't exist anymore, disconnecting client " . $ws_id );
        $self->close_websocket(
            code    => 500,
            message => 'authentication failed',
            ws_id   => $ws_id,
            finish  => 1
        );
    }

    $self->{nodes} = $temp_nodes;
    return 0;
}
=head3 action_proxydelnode(token => $t, data => $decoded_json)

decoded_json : PROXYDELNODE message type already decoded with json_decode.

If the message is valid, update the internal state to remove this node, or log an error if the message is not valid.

Return :
* 0 if node was successfully deleted from local state.
* 1 if message can't be decoded
* 2 if message can be decoded but node don't exist in local state
=cut
sub action_proxydelnode {
    my ($self, %options) = @_;
    my $node = $options{data};
    if (is_empty($node)) {
          $self->{logger}->writeLogError("Can't decode a proxydelnode message data: no data");
          return 1;
    }

    if ($self->{nodes}->{ $node->{id} }){
        $self->{logger}->writeLogDebug("[proxy-httpserver] deleting node ". $node->{id} . " from pullwss." );
        if ($node->{uid} and $self->{nodes}->{ $node->{uid}}){
           delete($self->{nodes}->{ $node->{uid} });
        }
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
        my (undef, $decoded) = $connector->json_decode(argument => $data);
        $connector->action_bcastlogger(data => $decoded);
        return ;
    } elsif ($action eq 'BCASTCOREKEY' && $target_complete eq '') {
        my (undef, $decoded) = $connector->json_decode(argument => $data);
        $connector->action_bcastcorekey(data => $decoded);
        return ;
    } elsif ($action eq 'PROXYADDNODE' && $target_complete eq '') {
        my (undef, $decoded) = $connector->json_decode(argument => $data);
        return $connector->action_proxyaddnode(data => $decoded, token => $token);
    } elsif ($action eq 'PROXYDELNODE' && $target_complete eq '') {
        my (undef, $decoded) = $connector->json_decode(argument => $data);
        return $connector->action_proxydelnode(data => $decoded, token => $token);
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
Authentication id done only once when websocket client send the first message, then the websocket session is considered authenticated.
The first message of any poller must be registernodes containing the poller id (or uid).

The "local state" is filled by PROXYADDNODE messages sent by nodes modules, and processed by the proxy function.

Return :
1 if websocket is logged.
0 if websocket is not logged.

=cut
sub is_logged_websocket {
    my ($self, %options) = @_;

    return 1 if ($self->{ws_clients}->{ $options{ws_id} }->{logged} == 1);

    my $token = $self->{ws_clients}->{ $options{ws_id} }->{authorization};
    if ($token =~ /^\s*Bearer\s+(\S*)\s*$/) {
        $token = $1;
    }

    my $check_conf_token = 1;
    my ($token_name, $token_value) = split(/:/, $token, 2);
    if (defined $token_name && defined $token_value) {
        my ($status, $results) = $self->{tpapi_centreonv2}->get_api_token(
            token_name => $token_name
        );
        if ($status == 0 && defined($results->{token})) {
            $check_conf_token = 0;
            if ($results->{token} ne $token_value
                || $results->{type} ne "poller"
                || $results->{is_revoked}) {
                $self->close_websocket(
                    code    => 500,
                    message => 'token authorization unallowed',
                    ws_id   => $options{ws_id}
                );
                return 0;
            }
        } else {
            $self->{logger}->writeLogInfo('[proxy-httpserver] cannot get token - ' . $self->{tpapi_centreonv2}->error());
        }
    }

    if ($check_conf_token == 1
        && ($self->{config}->{httpserver}->{token} eq ""
        || $self->{config}->{httpserver}->{token} ne $token)) {
        $self->close_websocket(
            code    => 500,
            message => 'token authorization unallowed',
            ws_id   => $options{ws_id}
        );
        return 0;
    }

    if ($options{data} !~ /^\[REGISTERNODES\]\s+\[(?:.*?)\]\s+\[.*?\]\s+(.*)/ms) {
        $self->close_websocket(
            code    => 500,
            message => 'please registernodes',
            ws_id   => $options{ws_id}
        );
        return 0;
    }

    my $content;
    eval {
        $content = JSON::XS->new->decode($1);
    };
    if ($@) {
        $self->close_websocket(
            code    => 500,
            message => 'decode error: unsupported format',
            ws_id   => $options{ws_id}
        );
        return 0;
    }
    if (!defined($content->{nodes}->[0]->{id}) || !defined($self->{nodes}->{$content->{nodes}->[0]->{id}})){
        $self->{logger}->writeLogDebug("[proxy-httpserver] client connection for unknown poller id/uid : " . $content->{nodes}->[0]->{id});
       $self->close_websocket(
            code    => 500,
            message => 'please registernodes',
            ws_id   => $options{ws_id}
        );
        return 0;
    }
    my $poller_id = $self->{nodes}->{$content->{nodes}->[0]->{id}}->{id};
    my $poller_uid = $self->{nodes}->{$content->{nodes}->[0]->{id}}->{uid};

    $self->{logger}->writeLogDebug("[proxy] httpserver client " . $content->{nodes}->[0]->{id} . " is logged");
    $self->{ws_clients}->{ $options{ws_id} }->{identity} = $content->{nodes}->[0]->{id};
    $self->{identities}->{ $poller_id } = $options{ws_id};
    $self->{identities}->{ $poller_uid } = $options{ws_id};
    $self->{ws_clients}->{ $options{ws_id} }->{logged} = 1;
    return 2;
}

sub clean_websocket {
    my ($self, %options) = @_;


    return if (!defined($self->{ws_clients}->{ $options{ws_id} }));

    $self->{ws_clients}->{ $options{ws_id} }->{tx}->finish() if (!defined($options{finish}));

    if (defined($self->{ws_clients}->{ $options{ws_id} }->{identity})){
        my $poller_id = $self->get_poller_id($self->{ws_clients}->{ $options{ws_id} }->{identity});
        delete $self->{identities}->{ $poller_id };

        my $poller_uid = $self->get_poller_uid($self->{ws_clients}->{ $options{ws_id} }->{identity});
        delete $self->{identities}->{ $poller_uid };
    }

    delete $self->{ws_clients}->{ $options{ws_id} };
}

sub close_websocket {
    my ($self, %options) = @_;

    $self->{ws_clients}->{ $options{ws_id} }->{tx}->send({json => {
        code => $options{code},
        message  => $options{message}
    }});
    $self->clean_websocket(ws_id => $options{ws_id}, finish => $options{finish});
}
# helper function to clean websocket
sub get_poller_id{
    my ($self, $value) = @_;
    if ($self->{nodes}->{$value} and $self->{nodes}->{$value}->{id} ) {
        return $self->{nodes}->{$value}->{id};
    }
    return $value;
}
sub get_poller_uid{
    my ($self, $value) = @_;
    if ($self->{nodes}->{$value} and $self->{nodes}->{$value}->{uid} ) {
        return $self->{nodes}->{$value}->{uid};
    }
    return $value;
}

1;
