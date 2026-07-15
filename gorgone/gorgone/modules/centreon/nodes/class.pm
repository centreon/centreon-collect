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

package gorgone::modules::centreon::nodes::class;

use base qw(gorgone::class::module);

use strict;
use warnings;
use gorgone::standard::library;
use gorgone::standard::constants qw(:all);
use gorgone::class::sqlquery;
use gorgone::class::http::http;
use MIME::Base64;
use JSON::XS;
use EV;

my %handlers = (TERM => {}, HUP => {});
my ($connector);

sub new {
    my ($class, %options) = @_;
    $connector = $class->SUPER::new(%options);
    bless $connector, $class;

    $connector->{register_nodes} = {}; 

    $connector->{default_resync_time} = (defined($options{config}->{resync_time}) && $options{config}->{resync_time} =~ /(\d+)/) ? $1 : 600;
    $connector->{resync_time} = $connector->{default_resync_time};
    $connector->{last_resync_time} = -1;

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
    $self->{logger}->writeLogDebug("[nodes] $$ Receiving order to stop...");
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

sub check_debug {
    my ($self, %options) = @_;

    my $request = "SELECT `value` FROM options WHERE `key` = 'debug_gorgone'";
    my ($status, $datas) = $self->{class_object}->custom_execute(request => $request, mode => 2);
    if ($status == -1) {
        $self->send_log(code => GORGONE_ACTION_FINISH_KO, token => $options{token}, data => { message => 'cannot find debug configuration' });
        $self->{logger}->writeLogError('[nodes] -class- cannot find debug configuration');
        return 1;
    }

    my $map_values = { 0 => 'default', 1 => 'debug' };
    my $debug_gorgone = 0;
    $debug_gorgone = $datas->[0]->[0] if (defined($datas->[0]->[0]));
    if (!defined($self->{debug_gorgone}) || $self->{debug_gorgone} != $debug_gorgone) {
        $self->send_internal_action({ action => 'BCASTLOGGER', data => { content => { severity => $map_values->{$debug_gorgone} } } });
    }

    $self->{debug_gorgone} = $debug_gorgone;
    return 0;
}

sub action_centreonnodessync {
    my ($self, %options) = @_;

    $options{token} = $self->generate_token() if (!defined($options{token}));

    $self->send_log(code => GORGONE_ACTION_BEGIN, token => $options{token}, data => { message => 'action nodesresync proceed' });

    # If we have a SQL issue: resync = 10 sec
    if ($self->check_debug()) {
        $self->{resync_time} = 10;
        return 1;
    }

    my $request = 'SELECT remote_server_id, poller_server_id FROM rs_poller_relation';
    my ($status, $datas) = $self->{class_object}->custom_execute(request => $request, mode => 2);
    if ($status == -1) {
        $self->{resync_time} = 10;
        $self->send_log(code => GORGONE_ACTION_FINISH_KO, token => $options{token}, data => { message => 'cannot find nodes remote configuration' });
        $self->{logger}->writeLogError('[nodes] Cannot find nodes remote configuration');
        return 1;
    }

    # we set a pathscore of 100 because it's "slave"
    my $register_subnodes = {};
    foreach (@$datas) {
        $register_subnodes->{$_->[0]} = [] if (!defined($register_subnodes->{$_->[0]}));
        unshift @{$register_subnodes->{$_->[0]}}, { id => $_->[1], pathscore => 100 };
    }

    $request = "
        SELECT id, name, localhost, ns_ip_address, gorgone_port, remote_id, remote_server_use_as_proxy, gorgone_communication_type, uid
        FROM nagios_server
        WHERE ns_activate = '1'
    ";
    ($status, $datas) = $self->{class_object}->custom_execute(request => $request, mode => 1, keys => 'id');
    if ($status == -1) {
        $self->{resync_time} = 10;
        $self->send_log(code => GORGONE_ACTION_FINISH_KO, token => $options{token}, data => { message => 'cannot find nodes configuration' });
        $self->{logger}->writeLogError('[nodes] Cannot find nodes configuration');
        return 1;
    }

    my ($core_id, $core_uid);
    my $register_temp = {};
    my $register_nodes = [];
    foreach my $node (values %$datas) {
        if ($node->{localhost} == 1) {
            $core_id  = $node->{id};
            $core_uid = $node->{uid};
            next;
        }

        # remote_server_use_as_proxy = 1 means: pass through the remote. otherwise directly.
        if (defined($node->{remote_id}) && $node->{remote_id} =~ /\d+/ && $node->{remote_server_use_as_proxy} == 1) {
            $register_subnodes->{$node->{remote_id}} = [] if (!defined($register_subnodes->{$node->{remote_id}}));
            unshift @{$register_subnodes->{$node->{remote_id}}}, { id => $node->{id}, pathscore => 1 };
            next;
        }
        $self->{register_nodes}->{$node->{id}} = 1;
        $register_temp->{$node->{id}} = 1;
        if ($node->{gorgone_communication_type} == 2) {
            push @$register_nodes, {
                id => $node->{id},
                type => 'push_ssh',
                address => $node->{ns_ip_address},
                ssh_port => $node->{gorgone_port},
                ssh_username => $self->{config}->{ssh_username},
                uid => $node->{uid},
            };
        } elsif($node->{gorgone_communication_type} == 3) {
            push @$register_nodes, {
                id => $node->{id},
                type => 'pull', # this is ZMQ where node is initiating connection.
                # Letting address and port for consistency and if in the future we want to validate source ip/port
                address => $node->{ns_ip_address},
                port => $node->{gorgone_port},
                uid => $node->{uid},
            };
        } elsif($node->{gorgone_communication_type} == 4) {
            push @$register_nodes, {
                id    => $node->{id},
                type  => 'pullwss',
                uid => $node->{uid},
                token => $node->{gorgone_auth_token} // "",
            };
        } else{ # value 1 and unknown is zmq push
            push @$register_nodes, {
                id => $node->{id},
                type => 'push_zmq',
                address => $node->{ns_ip_address},
                port => $node->{gorgone_port},
                uid => $node->{uid},
                token => $node->{gorgone_auth_token} // "",
            };
        }
    }

    my $unregister_nodes = [];    
    foreach (keys %{$self->{register_nodes}}) {
        if (!defined($register_temp->{$_})) {
            push @$unregister_nodes, { id => $_ };
            delete $self->{register_nodes}->{$_};
        }
    }

    # We add subnodes
    foreach (@$register_nodes) {
        if (defined($register_subnodes->{ $_->{id} })) {
            $_->{nodes} = $register_subnodes->{ $_->{id} };
        }
    }

    # as we can now specify communication type in the database, the register module become useless.
    # to avoid breaking existing config, the node module now read the register config file and apply it.
    # using a single module allows to simplify the overriding compute, and stop doing it in multiples interposed zmq messages.
    my $file_nodes = $self->get_register_module_config() // [];
    for my $file_node (@$file_nodes) {
        next unless $file_node->{prevail};
        my $db_node;
        for my $arr (@$register_nodes) {
            if ($arr->{id} == $file_node->{id}){
                $db_node = $arr;
                last
            }
        }
        $self->{logger}->writeLogInfo("[nodes] updating node " . $file_node->{id} . " info from database with register configuration");
        if (!defined($db_node)) {
            $self->{logger}->writeLogDebug("[nodes] node " . $file_node->{id} . " does not exist in database, using register file configuration.");

            $db_node = { "id" => $file_node->{id},
                "uid" => $file_node->{uid} // $file_node->{id},
                "type" => $file_node->{type} // "pullwss"};

            push @$register_nodes, $db_node;
        }
        for my $key ("type", "address", "port", "server_pubkey", "client_pubkey", "client_privkey", "cipher", "vector", "nodes") {
            $file_node->{$key} and $db_node->{$key} = $file_node->{$key};
        }
    }


    $self->{logger}->writeLogInfo(sprintf(
        "[nodes] retrieved %s nodes from DB and/or register configuration file, %s new and %s to delete. Sending to other gorgone modules",
        scalar( keys %$datas),
        scalar(@$register_nodes),
        scalar(@$unregister_nodes)));


    $self->send_internal_action({ action => 'SETCOREID', data => { id => $core_id, uid => $core_uid } }) if (defined($core_id));
    $self->send_internal_action({ action => 'REGISTERNODESFROMDB', data => { nodes => $register_nodes } });
    $self->send_internal_action({ action => 'UNREGISTERNODES', data => { nodes => $unregister_nodes } });

    $self->{logger}->writeLogDebug("[nodes] Finish resync");
    $self->send_log(code => GORGONE_ACTION_FINISH_OK, token => $options{token}, data => { message => 'action nodesresync finished' });

    $self->{resync_time} = $self->{default_resync_time};
    return 0;
}
sub get_register_module_config {
    my ($self, %options) = @_;
    # first we check if the register module is enabled, and exit if not.
    my $file_path;
    for my $module (@{$self->{config_core}->{modules}}){
        next if $module->{package} ne 'gorgone::modules::core::register::hooks';
        next if $module->{enable} !~ /true|1/;
        $file_path = $module->{config_file};
        last
    }
    return undef if !defined($file_path);

    my $file_conf = gorgone::standard::library::read_config(
        config_file => $file_path,
        logger => $self->{logger}
    );
    if (!$file_conf or !$file_conf->{nodes}){
        return undef;
    }
    return $file_conf->{nodes};

}
sub periodic_exec {
    my ($self, %options) = @_;

    if ($self->{stop} == 1) {
        $self->{logger}->writeLogInfo("[nodes] -class- $$ has quit");
        exit(0);
    }

    if (time() - $self->{resync_time} > $self->{last_resync_time}) {
        $self->{last_resync_time} = time();
        $self->action_centreonnodessync();
    }
}

sub run {
    my ($self, %options) = @_;

    $self->{db_centreon} = gorgone::class::db->new(
        dsn => $self->{config_db_centreon}->{dsn},
        user => $self->{config_db_centreon}->{username},
        password => $self->{config_db_centreon}->{password},
        force => 0,
        logger => $self->{logger}
    );
    $self->{class_object} = gorgone::class::sqlquery->new(logger => $self->{logger}, db_centreon => $self->{db_centreon});

    $self->{internal_socket} = gorgone::standard::library::connect_com(
        context => $self->{zmq_context},
        zmq_type => 'ZMQ_DEALER',
        name => 'gorgone-nodes',
        logger => $self->{logger},
        type => $self->get_core_config(name => 'internal_com_type'),
        path => $self->get_core_config(name => 'internal_com_path')
    );
    $self->send_internal_action({
        action => 'CENTREONNODESREADY',
        data => {}
    });

    $self->periodic_exec();

    my $watcher_timer = $self->{loop}->timer(5, 5, sub { $self->periodic_exec() } );
    my $watcher_io = $self->{loop}->io($self->{internal_socket}->get_fd(), EV::READ, sub { $connector->event() } );
    $self->{loop}->run();
}

1;
