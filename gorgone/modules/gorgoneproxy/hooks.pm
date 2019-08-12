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

package modules::gorgoneproxy::hooks;

use warnings;
use strict;
use centreon::script::gorgonecore;
use centreon::gorgone::common;
use modules::gorgoneproxy::class;

my $config_core;
my $config;
my $module_shortname = 'proxy';
my $module_id = 'gorgoneproxy';
my $events = [
    { event => 'PROXYREADY' },
    { event => 'SETLOGS' }, # internal. Shouldn't be used by third party clients
    { event => 'PONG' }, # internal. Shouldn't be used by third party clients
    { event => 'REGISTERNODE' }, # internal. Shouldn't be used by third party clients
    { event => 'UNREGISTERNODE' }, # internal. Shouldn't be used by third party clients
    { event => 'ADDPOLLER', uri => '/poller', method => 'POST' },
];

my $synctime_error = 0;
my $synctime_pollers = {}; # get last time retrieved
my $synctime_lasttime;
my $synctime_option;
my $synctimeout_option;
my $ping_option;
my $ping_time = 0;

my $last_pong = {}; 
my $register_pollers = {};
my $last_pollers = {}; # Last values from centreon database and the type
my $pools = {};
my $pools_pid = {};
my $poller_pool = {};
my $rr_current = 0;
my $stop = 0;
my ($external_socket, $internal_socket, $core_id);

sub register {
    my (%options) = @_;
    
    $config = $options{config};
    $config_core = $options{config_core};
    return ($events, $module_shortname , $module_id);
}

sub init {
    my (%options) = @_;

    $synctime_lasttime = time();
    $synctime_option = defined($config->{synchistory_time}) ? $config->{synchistory_time} : 300;
    $synctimeout_option = defined($config->{synchistory_timeout}) ? $config->{synchistory_timeout} : 120;
    $ping_option = defined($config->{ping}) ? $config->{ping} : 60;
    
    $core_id = $options{id};
    $external_socket = $options{external_socket};
    $internal_socket = $options{internal_socket};
    $last_pollers = get_pollers(dbh => $options{dbh});
    for my $pool_id (1..$config->{pool}) {
        create_child(pool_id => $pool_id, logger => $options{logger});
    }
}

sub routing {
    my (%options) = @_;

    my $data;
    eval {
        $data = JSON->new->utf8->decode($options{data});
    };
    if ($@) {
        $options{logger}->writeLogError("Cannot decode json data: $@");
        centreon::gorgone::common::add_history(
            dbh => $options{dbh},
            code => 20, token => $options{token},
            data => { message => 'gorgoneproxy: cannot decode json' },
            json_encode => 1
        );
        return undef;
    }
    
    if ($options{action} eq 'PONG') {
        return undef if (!defined($data->{data}->{id}) || $data->{data}->{id} eq '');
        $last_pong->{$data->{data}->{id}} = time();
        $options{logger}->writeLogInfo("gorgone-proxy: pong received from '" . $data->{data}->{id} . "'");
        return undef;
    }
    
    if ($options{action} eq 'UNREGISTERNODE') {
        $options{logger}->writeLogInfo("gorgone-proxy: poller '" . $data->{id} . "' is unregistered");
        if (defined($register_pollers->{$data->{id}})) {
            delete $register_pollers->{$data->{id}};
            delete $synctime_pollers->{$data->{id}};
        }
        return undef;
    }
    
    if ($options{action} eq 'REGISTERNODE') {
        $options{logger}->writeLogInfo("gorgone-proxy: poller '" . $data->{id} . "' is registered");
        $register_pollers->{$data->{id}} = 1;
        $last_pong->{$data->{id}} = 0 if (!defined($last_pong->{$data->{id}}));
        if ($synctime_error == 0 && !defined($synctime_pollers->{$options{target}}) &&
            !defined($synctime_pollers->{$data->{id}})) {
            $synctime_pollers->{$data->{id}} = { ctime => 0, in_progress => 0, in_progress_time => -1, last_id => 0 }; 
        }
        return undef;
    }
    
    if ($options{action} eq 'PROXYREADY') {
        $pools->{$data->{pool_id}}->{ready} = 1;
        return undef;
    }
    
    if ($options{action} eq 'SETLOGS') {
        setlogs(dbh => $options{dbh}, data => $data, token => $options{token}, logger => $options{logger});
        return undef;
    }
    
    if (!defined($options{target}) || 
        (!defined($last_pollers->{$options{target}}) && !defined($register_pollers->{$options{target}}))) {
        centreon::gorgone::common::add_history(
            dbh => $options{dbh},
            code => 20, token => $options{token},
            data => { message => 'gorgoneproxy: need a valid poller id' },
            json_encode => 1
        );
        return undef;
    }
    
    if ($options{action} eq 'GETLOG') {
        if ($synctime_error == -1 || get_sync_time(dbh => $options{dbh}) == -1) {
            centreon::gorgone::common::add_history(
                dbh => $options{dbh},
                code => 20, token => $options{token},
                data => { message => 'gorgoneproxy: problem to getlog' },
                json_encode => 1
            );
            return undef;
        }
               
        if ($synctime_pollers->{$options{target}}->{in_progress} == 1) {
            centreon::gorgone::common::add_history(
                dbh => $options{dbh},
                code => 20, token => $options{token},
                data => { message => 'gorgoneproxy: getlog already in progress' },
                json_encode => 1
            );
            return undef;
        }
        if (defined($last_pollers->{$options{target}}) && $last_pollers->{$options{target}}->{type} == 2) {
            centreon::gorgone::common::add_history(
                dbh => $options{dbh},
                code => 20, token => $options{token},
                data => { message => "gorgoneproxy: can't get log a ssh target" },
                json_encode => 1
            );
            return undef;
        }
        
        # We put the good time to get        
        my $ctime = $synctime_pollers->{$options{target}}->{ctime};
        my $last_id = $synctime_pollers->{$options{target}}->{last_id};
        $options{data} = centreon::gorgone::common::json_encode(data => { ctime => $ctime, id => $last_id });
        $synctime_pollers->{$options{target}}->{in_progress} = 1;
        $synctime_pollers->{$options{target}}->{in_progress_time} = time();
    }
    
    # Mode zmq pull
    if (defined($register_pollers->{$options{target}})) {
        pull_request(%options, data_decoded => $data);
        return undef;
    }
    
    if (centreon::script::gorgonecore::waiting_ready_pool(pool => $pools) == 0) {
        centreon::gorgone::common::add_history(
            dbh => $options{dbh},
            code => 20, token => $options{token},
            data => { message => 'gorgoneproxy: still none ready' },
            json_encode => 1
        );
        return undef;
    }
    
    my $identity;
    if (defined($poller_pool->{$options{target}})) {
        $identity = $poller_pool->{$options{target}};
    } else {
        $identity = rr_pool();
        $poller_pool->{$options{target}} = $identity;
    }
    
    centreon::gorgone::common::zmq_send_message(
        socket => $options{socket}, identity => 'gorgoneproxy-' . $identity,
        action => $options{action}, data => $options{data}, token => $options{token},
        target => $options{target}
    );
}

sub gently {
    my (%options) = @_;

    $stop = 1;
    foreach my $pool_id (keys %{$pools}) {
        $options{logger}->writeLogInfo("gorgone-proxy: Send TERM signal for pool '" . $pool_id . "'");
        if ($pools->{$pool_id}->{running} == 1) {
            CORE::kill('TERM', $pools->{$pool_id}->{pid});
        }
    }
}

sub kill {
    my (%options) = @_;

    foreach (keys %{$pools}) {
        if ($pools->{$_}->{running} == 1) {
            $options{logger}->writeLogInfo("gorgone-proxy: Send KILL signal for pool '" . $_ . "'");
            CORE::kill('KILL', $pools->{$_}->{pid});
        }
    }
}

sub kill_internal {
    my (%options) = @_;

}

sub check {
    my (%options) = @_;

    my $count = 0;
    foreach my $pid (keys %{$options{dead_childs}}) {
        # Not me
        next if (!defined($pools_pid->{$pid}));
        
        # If someone dead, we recreate
        my $pool_id = $pools_pid->{$pid};
        delete $pools->{$pools_pid->{$pid}};
        delete $pools_pid->{$pid};
        delete $options{dead_childs}->{$pid};
        if ($stop == 0) {
            create_child(pool_id => $pool_id, logger => $options{logger});
        }
    }
    
    foreach (keys %{$pools}) {
        $count++  if ($pools->{$_}->{running} == 1);
    }
    
    # We put synclog request in timeout
    foreach (keys %{$synctime_pollers}) {
        if ($synctime_pollers->{$_}->{in_progress} == 1 && 
            time() - $synctime_pollers->{$_}->{in_progress_time} > $synctimeout_option) {
            centreon::gorgone::common::add_history(
                dbh => $options{dbh},
                code => 20,
                data => { message => "gorgoneproxy: getlog in timeout for '$_'" },
                json_encode => 1
            );
            $synctime_pollers->{$_}->{in_progress} = 0;
        }
    }
    
    # We check if we need synclogs
    if ($stop == 0 && 
        ($synctime_error == 0 || get_sync_time(dbh => $options{dbh}) == 0) &&
        time() - $synctime_lasttime > $synctime_option) {
        $synctime_lasttime = time();
        full_sync_history(dbh => $options{dbh});
    }
    
    if ($stop == 0 &&
        time() - $ping_time > $ping_option) {
        $options{logger}->writeLogInfo("gorgone-proxy: send pings");
        $ping_time = time();
        ping_send(dbh => $options{dbh});
    }
    
    return $count;
}

# Specific functions
sub setlogs {
    my (%options) = @_;
    
    if (!defined($options{data}->{data}->{id}) || $options{data}->{data}->{id} eq '') {
        centreon::gorgone::common::add_history(
            dbh => $options{dbh},
            code => 20, token => $options{token},
            data => { message => 'gorgoneproxy: need a id to setlogs' },
            json_encode => 1
        );
        return undef;
    }
    if ($synctime_pollers->{$options{data}->{data}->{id}}->{in_progress} == 0) {
        centreon::gorgone::common::add_history(
            dbh => $options{dbh},
            code => 20, token => $options{token},
            data => { message => 'gorgoneproxy: skip setlogs response. Maybe too much time to get response. Retry' },
            json_encode => 1
        );
        return undef;
    }
    
    $options{logger}->writeLogInfo("gorgoneproxy: hooks: received setlogs for '$options{data}->{data}->{id}'");
    
    $synctime_pollers->{$options{data}->{data}->{id}}->{in_progress} = 0;
    
    my $ctime_recent = 0;
    my $last_id = 0;
    # Transaction
    $options{dbh}->transaction_mode(1);
    my $status = 0;
    foreach (keys %{$options{data}->{data}->{result}}) {
        $status = centreon::gorgone::common::add_history(
            dbh => $options{dbh},
            etime => $options{data}->{data}->{result}->{$_}->{etime}, 
            code => $options{data}->{data}->{result}->{$_}->{code}, 
            token => $options{data}->{data}->{result}->{$_}->{token},
            data => $options{data}->{data}->{result}->{$_}->{data}
        );
        last if ($status == -1);
        $ctime_recent = $options{data}->{data}->{result}->{$_}->{ctime} if ($ctime_recent < $options{data}->{data}->{result}->{$_}->{ctime});
        $last_id = $options{data}->{data}->{result}->{$_}->{id} if ($last_id < $options{data}->{data}->{result}->{$_}->{id});
    }
    if ($status == 0 && update_sync_time(dbh => $options{dbh}, id => $options{data}->{data}->{id}, last_id => $last_id, ctime => $ctime_recent) == 0) {
        $options{dbh}->commit();
        $synctime_pollers->{$options{data}->{data}->{id}}->{last_id} = $last_id if ($last_id != 0);
        $synctime_pollers->{$options{data}->{data}->{id}}->{ctime} = $ctime_recent if ($ctime_recent != 0);
    } else {
        $options{dbh}->rollback();
    }
    $options{dbh}->transaction_mode(0);    
}

sub ping_send {
    my (%options) = @_;
    
    foreach my $id (keys %{$last_pollers}) {
        if ($last_pollers->{$id}->{type} == 1) {
            routing(socket => $internal_socket, action => 'PING', target => $id, data => '{}', dbh => $options{dbh});
        }
    }
    
    foreach my $id (keys %{$register_pollers}) {
        routing(action => 'PING', target => $id, data => '{}', dbh => $options{dbh});
    }
}

sub full_sync_history {
    my (%options) = @_;
    
    foreach my $id (keys %{$last_pollers}) {
        if ($last_pollers->{$id}->{type} == 1) {
            routing(socket => $internal_socket, action => 'GETLOG', target => $id, data => '{}', dbh => $options{dbh});
        }
    }
    
    foreach my $id (keys %{$register_pollers}) {
        routing(action => 'GETLOG', target => $id, data => '{}', dbh => $options{dbh});
    }
}

sub update_sync_time {
    my (%options) = @_;
    
    # Nothing to update (no insert before)
    return 0 if ($options{ctime} == 0);
    
    my $status;
    if ($synctime_pollers->{$options{id}}->{last_id} == 0) {
        ($status) = $options{dbh}->query("INSERT INTO gorgone_synchistory (`id`, `ctime`, `last_id`) VALUES (" . $options{dbh}->quote($options{id}) . ", " . $options{dbh}->quote($options{ctime}) . ", " . $options{dbh}->quote($options{last_id}) . ")");
    } else {
        ($status) = $options{dbh}->query("UPDATE gorgone_synchistory SET `ctime` = " . $options{dbh}->quote($options{ctime}) . ", `last_id` = " . $options{dbh}->quote($options{last_id}) . " WHERE `id` = " . $options{dbh}->quote($options{id}));
    }
    return $status;
}

sub get_sync_time {
    my (%options) = @_;
    
    my ($status, $sth) = $options{dbh}->query("SELECT * FROM gorgone_synchistory");
    if ($status == -1) {
        $synctime_error = -1;
        return -1;
    }
    $synctime_error = 0;

    while (my $row = $sth->fetchrow_hashref()) {
        $synctime_pollers->{$row->{id}} = { ctime => $row->{ctime}, in_progress => 0, in_progress_time => -1, last_id => $row->{last_id} }; 
    }
    
    return 0;
}

sub get_pollers {
    my (%options) = @_;
    # TODO method
    # type 1 = 'zmq', type 2 = 'ssh'
    
    my $pollers = {};
    foreach (([1, 1], [2, 1], [10, 1], [166, 2], [140, 1])) {
        $pollers->{${$_}[0]} = { type => ${$_}[1] };
        $synctime_pollers->{${$_}[0]} = { ctime => 0, in_progress => 0, in_progress_time => -1, last_id => 0 }; 
        $last_pong->{${$_}[0]} = 0 if (!defined($last_pong->{${$_}[0]}));
    }

    get_sync_time(dbh => $options{dbh});
    
    return $pollers;
}

sub rr_pool {
    my (%options) = @_;
    
    while (1) {
        $rr_current = $rr_current % $config->{pool};
        if ($pools->{$rr_current + 1}->{ready} == 1) {
            $rr_current++;
            return $rr_current;
        }
        $rr_current++;
    }
}

sub create_child {
    my (%options) = @_;
    
    $options{logger}->writeLogInfo("Create gorgoneproxy for pool id '" . $options{pool_id} . "'");
    my $child_pid = fork();
    if ($child_pid == 0) {
        $0 = 'gorgone-proxy';
        my $module = modules::gorgone::class->new(
            logger => $options{logger},
            config_core => $config_core,
            config => $config,
            pool_id => $options{pool_id},
            core_id => $core_id
        );
        $module->run();
        exit(0);
    }
    $options{logger}->writeLogInfo("PID $child_pid gorgoneproxy for pool id '" . $options{pool_id} . "'");
    $pools->{$options{pool_id}} = { pid => $child_pid, ready => 0, running => 1 };
    $pools_pid->{$child_pid} = $options{pool_id};
}

sub pull_request {
    my (%options) = @_;

    # No target anymore. We remove it.
    my $message = centreon::gorgone::common::build_protocol(
        action => $options{action}, data => $options{data}, token => $options{token},
        target => ''
    );
    my ($status, $key) = centreon::gorgone::common::is_handshake_done(dbh => $options{dbh}, identity => unpack('H*', $options{target}));
    if ($status == 0) {
        centreon::gorgone::common::add_history(
            dbh => $options{dbh},
            code => 20, token => $options{token},
            data => { message => "gorgoneproxy: node '" . $options{target} . "' had never been connected" },
            json_encode => 1
        );
        return undef;
    }
    
    # Should call here the function to transform data and do some put logs. A loop (because it will also be used in sub proxy process)
    # Catch some actions call and do some transformation (on file copy)
    # TODO
    
    centreon::gorgone::common::zmq_send_message(
        socket => $external_socket,
        cipher => $config_core->{cipher},
        vector => $config_core->{vector},
        symkey => $key,
        identity => $options{target},
        message => $message
    );
}

sub get_constatus_result {
    my (%options) = @_;

    my $result = { last_ping => $ping_time, entries => $last_pong };
    return $result;
}

1;
