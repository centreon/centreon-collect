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

package gorgone::modules::core::pullwss::hooks;

use warnings;
use strict;
use gorgone::class::core;
use gorgone::modules::core::pullwss::class;
use gorgone::standard::constants qw(:all);

use constant NAMESPACE => 'core';
use constant NAME => 'pullwss';
use constant EVENTS => [
    { event => 'PULLWSSREADY' }
];

my $config_core;
my $config;
my $pullwss = {};
my $stop = 0;

sub register {
    my (%options) = @_;

    my $loaded = 1;
    $config = $options{config};
    $config_core = $options{config_core};

    if (!defined($config->{address}) || $config->{address} =~ /^\s*$/) {
        $options{logger}->writeLogError('[pullwss] address option mandatory');
        $loaded = 0;
    }
    if (!defined($config->{port}) || $config->{port} !~ /^\d+$/) {
        $options{logger}->writeLogError('[pullwss] port option mandatory');
        $loaded = 0;
    }
    if (!defined($config->{token}) || $config->{token} =~ /^\s*$/) {
        $options{logger}->writeLogError('[pullwss] token option mandatory');
        $loaded = 0;
    }
    # max size from the socket doc is 262144, but for some reason it doesn't work for some message.
    # making it 130 000 make it work every time I tested. Maybe each character is 2 bytes in perl on some specific conditions.
    $config->{max_msg_size} = 130_000
        if (!defined($config->{max_msg_size}) || $config->{max_msg_size} !~ /\d+/);

    return ($loaded, NAMESPACE, NAME, EVENTS);
}

sub init {
    my (%options) = @_;

    create_child(logger => $options{logger});
}

sub routing {
    my (%options) = @_;
    
    if ($options{action} eq 'PULLWSSREADY') {
        $pullwss->{ready} = 1;
        return undef;
    }
    
    if (gorgone::class::core::waiting_ready(ready => \$pullwss->{ready}) == 0) {
        gorgone::standard::library::add_history({
            dbh => $options{dbh},
            code => GORGONE_ACTION_FINISH_KO,
            token => $options{token},
            data => { message => 'gorgone-pullwss: still no ready' },
            json_encode => 1
        });
        return undef;
    }

    $options{gorgone}->send_internal_message(
        identity => 'gorgone-pullwss',
        action => $options{action},
        raw_data_ref => $options{frame}->getRawData(),
        token => $options{token}
    );
}

sub gently {
    my (%options) = @_;

    $stop = 1;
    if (defined($pullwss->{running}) && $pullwss->{running} == 1) {
        $options{logger}->writeLogDebug("[pullwss] Send TERM signal $pullwss->{pid}");
        CORE::kill('TERM', $pullwss->{pid});
    }
}

sub kill {
    my (%options) = @_;

    if ($pullwss->{running} == 1) {
        $options{logger}->writeLogDebug("[pullwss] Send KILL signal for $pullwss->{pid}");
        CORE::kill('KILL', $pullwss->{pid});
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
        next if (!defined($pullwss->{pid}) || $pullwss->{pid} != $pid);

        $pullwss = {};
        delete $options{dead_childs}->{$pid};
        if ($stop == 0) {
            create_child(logger => $options{logger});
        }

        last;
    }

    $count++  if (defined($pullwss->{running}) && $pullwss->{running} == 1);

    return $count;
}

sub broadcast {
    my (%options) = @_;

    routing(%options);
}

# Specific functions
sub create_child {
    my (%options) = @_;
    
    $options{logger}->writeLogInfo("[pullwss] Create module 'pullwss' process");
    my $child_pid = fork();
    if ($child_pid == 0) {
        $0 = 'gorgone-pullwss';
        my $module = gorgone::modules::core::pullwss::class->new(
            logger => $options{logger},
            module_id => NAME,
            config_core => $config_core,
            config => $config
        );
        my $fh;
        open($fh, ">>", "/tmp/gorgone-process-$0");
        use Devel::TraceCalls;
        trace_calls {
            Package => ["gorgone::modules::centreon::audit::hooks", "gorgone::modules::centreon::audit::class", "gorgone::modules::centreon::audit::sampling::system::cpu", "gorgone::modules::centreon::audit::sampling::system::diskio", "gorgone::modules::centreon::audit::metrics::system::cpu", "gorgone::modules::centreon::audit::metrics::system::disk", "gorgone::modules::centreon::audit::metrics::system::os", "gorgone::modules::centreon::audit::metrics::system::diskio", "gorgone::modules::centreon::audit::metrics::system::load", "gorgone::modules::centreon::audit::metrics::system::memory", "gorgone::modules::centreon::audit::metrics::centreon::realtime", "gorgone::modules::centreon::audit::metrics::centreon::packages", "gorgone::modules::centreon::audit::metrics::centreon::rrd", "gorgone::modules::centreon::audit::metrics::centreon::pluginpacks", "gorgone::modules::centreon::audit::metrics::centreon::database", "gorgone::modules::centreon::judge::hooks", "gorgone::modules::centreon::judge::class", "gorgone::modules::centreon::judge::type::spare", "gorgone::modules::centreon::judge::type::distribute", "gorgone::modules::centreon::nodes::class", "gorgone::modules::centreon::nodes::hooks", "gorgone::modules::centreon::statistics::hooks", "gorgone::modules::centreon::statistics::class", "gorgone::modules::centreon::anomalydetection::hooks", "gorgone::modules::centreon::anomalydetection::class", "gorgone::modules::centreon::inject::class", "gorgone::modules::centreon::inject::hooks", "gorgone::modules::centreon::autodiscovery::hooks", "gorgone::modules::centreon::autodiscovery::class", "gorgone::modules::centreon::autodiscovery::services::discovery", "gorgone::modules::centreon::autodiscovery::services::resources", "gorgone::modules::centreon::engine::class", "gorgone::modules::centreon::engine::hooks", "gorgone::modules::centreon::legacycmd::hooks", "gorgone::modules::centreon::legacycmd::class", "gorgone::modules::plugins::scom::hooks", "gorgone::modules::plugins::scom::class", "gorgone::modules::plugins::newtest::class", "gorgone::modules::plugins::newtest::hooks", "gorgone::modules::plugins::newtest::libs::stubs::errors", "gorgone::modules::plugins::newtest::libs::stubs::ManagementConsoleService", "gorgone::modules::core::pullwss::hooks", "gorgone::modules::core::pullwss::class", "gorgone::modules::core::httpserver::class", "gorgone::modules::core::httpserver::hooks", "gorgone::modules::core::pull::hooks", "gorgone::modules::core::pull::class", "gorgone::modules::core::httpserverng::class", "gorgone::modules::core::httpserverng::hooks", "gorgone::modules::core::action::class", "gorgone::modules::core::action::hooks", "gorgone::modules::core::pipeline::class", "gorgone::modules::core::pipeline::hooks", "gorgone::modules::core::cron::hooks", "gorgone::modules::core::cron::class", "gorgone::modules::core::proxy::httpserver", "gorgone::modules::core::proxy::sshclient", "gorgone::modules::core::proxy::hooks", "gorgone::modules::core::proxy::class", "gorgone::modules::core::register::class", "gorgone::modules::core::register::hooks", "gorgone::modules::core::dbcleaner::hooks", "gorgone::modules::core::dbcleaner::class", "gorgone::class::fingerprint::backend::sql", "gorgone::class::clientzmq", "gorgone::class::script", "gorgone::class::db", "gorgone::class::tpapi::centreonv2", "gorgone::class::tpapi::clapi", "gorgone::class::core", "gorgone::class::sqlquery", "gorgone::class::module", "gorgone::class gorgone::class::frame", "gorgone::class::http::http", "gorgone::class::http::backend::useragent", "gorgone::class::http::backend::curl", "gorgone::class::http::backend::curlconstants", "gorgone::class::http::backend::lwp", "gorgone::class::listener", "gorgone::standard::misc", "gorgone::standard::api", "gorgone::standard::constants", "gorgone::standard::library"],
            LogTo => $fh,
        };

        $module->run();
        exit(0);
    }
    $options{logger}->writeLogDebug("[pullwss] PID $child_pid (gorgone-pullwss)");
    $pullwss = { pid => $child_pid, ready => 0, running => 1 };
}

1;
