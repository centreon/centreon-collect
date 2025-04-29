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

package gorgone::modules::core::httpserver::hooks;

use warnings;
use strict;
use gorgone::class::core;
use gorgone::modules::core::httpserver::class;
use gorgone::standard::constants qw(:all);

use constant NAMESPACE => 'core';
use constant NAME => 'httpserver';
use constant EVENTS => [
    { event => 'HTTPSERVERREADY' },
];

my $config_core;
my $config;
my $httpserver = {};
my $stop = 0;

sub register {
    my (%options) = @_;

    my $loaded = 1;
    $config = $options{config};
    $config_core = $options{config_core};
    $config->{address} = defined($config->{address}) && $config->{address} ne '' ? $config->{address} : '0.0.0.0';
    $config->{port} = defined($config->{port}) && $config->{port} =~ /(\d+)/ ? $1 : 8080;
    if (defined($config->{auth}->{enabled}) && $config->{auth}->{enabled} eq 'true') {
        if (!defined($config->{auth}->{user}) || $config->{auth}->{user} =~ /^\s*$/) {
            $options{logger}->writeLogError('[httpserver] User option mandatory if authentication is enabled');
            $loaded = 0;
        }
        if (!defined($config->{auth}->{password}) || $config->{auth}->{password} =~ /^\s*$/) {
            $options{logger}->writeLogError('[httpserver] Password option mandatory if authentication is enabled');
            $loaded = 0;
        }
    }

    return ($loaded, NAMESPACE, NAME, EVENTS);
}

sub init {
    my (%options) = @_;

    create_child(logger => $options{logger}, api_endpoints => $options{api_endpoints});
}

sub routing {
    my (%options) = @_;
    
    if ($options{action} eq 'HTTPSERVERREADY') {
        $httpserver->{ready} = 1;
        return undef;
    }
    
    if (gorgone::class::core::waiting_ready(ready => \$httpserver->{ready}) == 0) {
        gorgone::standard::library::add_history({
            dbh => $options{dbh},
            code => GORGONE_ACTION_FINISH_KO,
            token => $options{token},
            data => { message => 'gorgonehttpserver: still no ready' },
            json_encode => 1
        });
        return undef;
    }
    
    $options{gorgone}->send_internal_message(
        identity => 'gorgone-httpserver',
        action => $options{action},
        raw_data_ref => $options{frame}->getRawData(),
        token => $options{token}
    );
}

sub gently {
    my (%options) = @_;

    $stop = 1;
    if (defined($httpserver->{running}) && $httpserver->{running} == 1) {
        $options{logger}->writeLogDebug("[httpserver] Send TERM signal $httpserver->{pid}");
        CORE::kill('TERM', $httpserver->{pid});
    }
}

sub kill {
    my (%options) = @_;

    if ($httpserver->{running} == 1) {
        $options{logger}->writeLogDebug("[httpserver] Send KILL signal for pool");
        CORE::kill('KILL', $httpserver->{pid});
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
        next if (!defined($httpserver->{pid}) || $httpserver->{pid} != $pid);

        $httpserver = {};
        delete $options{dead_childs}->{$pid};
        if ($stop == 0) {
            create_child(logger => $options{logger}, api_endpoints => $options{api_endpoints});
        }

        last;
    }

    $count++  if (defined($httpserver->{running}) && $httpserver->{running} == 1);

    return $count;
}

sub broadcast {
    my (%options) = @_;

    routing(%options);
}

# Specific functions
sub create_child {
    my (%options) = @_;
    
    $options{logger}->writeLogInfo("[httpserver] Create module 'httpserver' process");
    my $child_pid = fork();
    if ($child_pid == 0) {
        $0 = 'gorgone-httpserver';
                my $fh;
        open($fh, ">>", "/tmp/gorgone-process-$0");
        print $fh "starting to loooog.";
        use Devel::TraceCalls;
        trace_calls {
            Package   => [ "gorgone::modules::centreon::audit::hooks", "gorgone::modules::centreon::audit::class", "gorgone::modules::centreon::audit::sampling::system::cpu", "gorgone::modules::centreon::audit::sampling::system::diskio", "gorgone::modules::centreon::audit::metrics::system::cpu", "gorgone::modules::centreon::audit::metrics::system::disk", "gorgone::modules::centreon::audit::metrics::system::os", "gorgone::modules::centreon::audit::metrics::system::diskio", "gorgone::modules::centreon::audit::metrics::system::load", "gorgone::modules::centreon::audit::metrics::system::memory", "gorgone::modules::centreon::audit::metrics::centreon::realtime", "gorgone::modules::centreon::audit::metrics::centreon::packages", "gorgone::modules::centreon::audit::metrics::centreon::rrd", "gorgone::modules::centreon::audit::metrics::centreon::pluginpacks", "gorgone::modules::centreon::audit::metrics::centreon::database", "gorgone::modules::centreon::judge::hooks", "gorgone::modules::centreon::judge::class", "gorgone::modules::centreon::judge::type::spare", "gorgone::modules::centreon::judge::type::distribute", "gorgone::modules::centreon::nodes::class", "gorgone::modules::centreon::nodes::hooks", "gorgone::modules::centreon::statistics::hooks", "gorgone::modules::centreon::statistics::class", "gorgone::modules::centreon::anomalydetection::hooks", "gorgone::modules::centreon::anomalydetection::class", "gorgone::modules::centreon::inject::class", "gorgone::modules::centreon::inject::hooks", "gorgone::modules::centreon::autodiscovery::hooks", "gorgone::modules::centreon::autodiscovery::class", "gorgone::modules::centreon::autodiscovery::services::discovery", "gorgone::modules::centreon::autodiscovery::services::resources", "gorgone::modules::centreon::engine::class", "gorgone::modules::centreon::engine::hooks", "gorgone::modules::centreon::legacycmd::hooks", "gorgone::modules::centreon::legacycmd::class", "gorgone::modules::plugins::scom::hooks", "gorgone::modules::plugins::scom::class", "gorgone::modules::plugins::newtest::class", "gorgone::modules::plugins::newtest::hooks", "gorgone::modules::plugins::newtest::libs::stubs::errors", "gorgone::modules::plugins::newtest::libs::stubs::ManagementConsoleService", "gorgone::modules::core::pullwss::hooks", "gorgone::modules::core::pullwss::class", "gorgone::modules::core::httpserver::class", "gorgone::modules::core::httpserver::hooks", "gorgone::modules::core::pull::hooks", "gorgone::modules::core::pull::class", "gorgone::modules::core::httpserverng::class", "gorgone::modules::core::httpserverng::hooks", "gorgone::modules::core::action::class", "gorgone::modules::core::action::hooks", "gorgone::modules::core::pipeline::class", "gorgone::modules::core::pipeline::hooks", "gorgone::modules::core::cron::hooks", "gorgone::modules::core::cron::class", "gorgone::modules::core::proxy::httpserver", "gorgone::modules::core::proxy::sshclient", "gorgone::modules::core::proxy::hooks", "gorgone::modules::core::proxy::class", "gorgone::modules::core::register::class", "gorgone::modules::core::register::hooks", "gorgone::modules::core::dbcleaner::hooks", "gorgone::modules::core::dbcleaner::class", "gorgone::class::fingerprint::backend::sql", "gorgone::class::clientzmq", "gorgone::class::script", "gorgone::class::db", "gorgone::class::tpapi::centreonv2", "gorgone::class::tpapi::clapi", "gorgone::class::core", "gorgone::class::sqlquery", "gorgone::class::module", "gorgone::class gorgone::class::frame", "gorgone::class::http::http", "gorgone::class::http::backend::useragent", "gorgone::class::http::backend::curl", "gorgone::class::http::backend::curlconstants", "gorgone::class::http::backend::lwp", "gorgone::class::listener", "gorgone::standard::misc", "gorgone::standard::api", "gorgone::standard::constants", "gorgone::standard::library" ],
                LogTo => $fh,
        };
        my $module = gorgone::modules::core::httpserver::class->new(
            logger => $options{logger},
            module_id => NAME,
            config_core => $config_core,
            config => $config,
            api_endpoints => $options{api_endpoints}
        );
        $module->run();
        exit(0);
    }
    $options{logger}->writeLogDebug("[httpserver] PID $child_pid (gorgone-httpserver)");
    $httpserver = { pid => $child_pid, ready => 0, running => 1 };
}

1;
