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

package gorgone::class::tpapi::centreonv2;

use strict;
use warnings;
use gorgone::class::http::http;
use JSON::XS;
use URI::Escape;

sub new {
    my ($class, %options) = @_;
    my $self  = {};
    bless $self, $class;

    $self->{is_error} = 1;
    $self->{error} = 'configuration missing';
    $self->{is_logged} = 0;

    return $self;
}

sub json_decode {
    my ($self, %options) = @_;

    my $decoded;
    eval {
        $decoded = JSON::XS->new->decode($options{content});
    };
    if ($@) {
        $self->{is_error} = 1;
        $self->{error} = "cannot decode json response: $@";
        return undef;
    }

    return $decoded;
}

sub error {
    my ($self, %options) = @_;

    return $self->{error};
}

sub set_configuration {
    my ($self, %options) = @_;

    if (!defined($options{config})) {
        return 1;
    }

    foreach (('base_url', 'username', 'password')) {
        if (!defined($options{config}->{$_}) ||
            $options{config}->{$_} eq '') {
            $self->{error} = $_ . ' configuration missing';
            return 1;
        }

        $self->{$_} = $options{config}->{$_};
    }

    $self->{base_url} =~ s/\/$//;

    $self->{http_backend} = defined($options{config}->{backend}) ? $options{config}->{backend} : 'curl';

    $self->{curl_opts} = ['CURLOPT_SSL_VERIFYPEER => 0', 'CURLOPT_POSTREDIR => CURL_REDIR_POST_ALL'];
    my $curl_opts = [];
    if (defined($options{config}->{curlopts})) {
        foreach (keys %{$options{config}->{curlopts}}) {
            push @{$curl_opts}, $_ . ' => ' . $options{config}->{curlopts}->{$_};
        }
    }
    if (scalar(@$curl_opts) > 0) {
        $self->{curl_opts} = $curl_opts;
    }

    $self->{http} = gorgone::class::http::http->new(logger => $options{logger});
    $self->{is_error} = 0;
    return 0;
}

sub authenticate {
    my ($self, %options) = @_;

    my $json_request = {
        security => {
            credentials => {
                login => $self->{username},
                password => $self->{password}
            }
        }
    };
    my $encoded;
    eval {
        $encoded = encode_json($json_request);
    };
    if ($@) {
        $self->{is_error} = 1;
        $self->{error} = "cannot encode json request: $@";
        return undef;
    }

    my ($code, $content) = $self->{http}->request(
        http_backend => $self->{http_backend},
        method => 'POST',
        hostname => '',
        full_url => $self->{base_url} . '/login',
        query_form_post => $encoded,
        header => [
            'Accept-Type: application/json; charset=utf-8',
            'Content-Type: application/json; charset=utf-8',
        ],
        curl_opt => $self->{curl_opts},
        warning_status => '',
        unknown_status => '',
        critical_status => ''
    );
    if ($code) {
        $self->{is_error} = 1;
        $self->{error} = 'http request error';
        return undef;
    }
    if ($self->{http}->get_code() < 200 || $self->{http}->get_code() >= 300) {
        $self->{is_error} = 1;
        $self->{error} =  "Login error [code: '" . $self->{http}->get_code() . "'] [message: '" . $self->{http}->get_message() . "']";
        return undef;
    }

    my $decoded = $self->json_decode(content => $content);
    return if (!defined($decoded));

    my $token = defined($decoded->{security}->{token}) ? $decoded->{security}->{token} : undef;
    if (!defined($token)) {
        $self->{is_error} = 1;
        $self->{error} = 'authenticate issue - cannot get token';
        return undef;
    }

    $self->{token} = $token;
    $self->{is_logged} = 1;
}

=head3 $self->request(method => $method, endpoint => $endpoint, get_param => $getParamRef, query_form_post => $body, no_replay => $bool)

Send a request to the Centreon API v2, after logging in if no token is cached.

If the API refuses a cached token (401 or 403, e.g. the session expired and was purged),
log in again and replay the request once.

Parameters:

=over 4

=item * method: HTTP method ('GET', 'POST', 'PUT', 'PATCH', 'DELETE'). Required.

=item * endpoint: path of the endpoint, appended to the configured base_url (ex '/platform/versions'). Required.

=item * get_param: array ref of query parameters as 'name=value' strings, not url-encoded (ex ['key=value']).
They are added after the default 'page=1' and 'limit=10000', which they cannot override:
a parameter given twice is sent twice. Optional.

=item * query_form_post: request body, already encoded as a json string. Optional.

=item * no_replay: if true, do not log in again and replay the request when the token is refused.
Optional, default false.

=back

Output : (Int, HashRef)

(0, $decoded) on success, $decoded being the json response already decoded.

1 on failure (configuration missing, login failed, http error or response not decodable),
the reason is available with $self->error().

=cut
sub request {
    my ($self, %options) = @_;

    if (!defined($self->{base_url})) {
        $self->{is_error} = 1;
        $self->{error} = 'configuration missing';
        return 1;
    }

    # the authentication token used has an inactivity timeout of 2 hours by default
    # if there is no request in this window, the token is deleted by php, and Gorgone need to authenticate again.
    #
    my $is_token_from_cache = $self->{is_logged};
    my ($status, $decoded) = $self->_send_request(%options);
    if ($status != 0 && $self->{is_logged} == 0 && $is_token_from_cache == 1 && !$options{no_replay}) {
        return $self->_send_request(%options);
    }

    return 1 if ($status != 0);
    return (0, $decoded);
}

# log in if needed and send one request. The token is dropped if the API refuses it.
sub _send_request {
    my ($self, %options) = @_;

    $self->{is_error} = 0;
    if ($self->{is_logged} == 0) {
        $self->authenticate();
    }

    return 1 if ($self->{is_logged} == 0);

    # TODO: manage it properly
    my $get_param = ['page=1', 'limit=10000'];
    if (defined($options{get_param})) {
        push @$get_param, @{$options{get_param}};
    }

    my ($code, $content) = $self->{http}->request(
        http_backend => $self->{http_backend},
        method => $options{method},
        hostname => '',
        full_url => $self->{base_url} . $options{endpoint},
        query_form_post => $options{query_form_post},
        get_param => $get_param,
        header => [
            'Accept-Type: application/json; charset=utf-8',
            'Content-Type: application/json; charset=utf-8',
            'X-AUTH-TOKEN: ' . $self->{token}
        ],
        curl_opt => $self->{curl_opts},
        warning_status => '',
        unknown_status => '',
        critical_status => ''
    );

    my $decoded = $self->json_decode(content => $content);

    # code 401 means the token is unknown (expired session), 403 means forbidden (token not good maybe)
    if ($self->{http}->get_code() == 401 || $self->{http}->get_code() == 403) {
        $self->{token} = undef;
        $self->{is_logged} = 0;
        $self->{is_error} = 1;
        $self->{error} = 'token forbidden';
        $self->{error} = $decoded->{message} if (defined($decoded) && defined($decoded->{message}));
        return 1;
    }

    if ($self->{http}->get_code() < 200 || $self->{http}->get_code() >= 300) {
        $self->{is_error} = 1;
        my $message = $self->{http}->get_message();
        $message = $decoded->{message} if (defined($decoded) && defined($decoded->{message}));
        $self->{error} =  "request error [code: '" . $self->{http}->get_code() . "'] [message: '" . $message . "']";
        return 1;
    }

    return 1 if (!defined($decoded));

    return (0, $decoded);
}

sub get_token {
    my ($self, %options) = @_;

    return $self->{token};
}

sub get_monitoring_hosts {
    my ($self, %options) = @_;

    my $endpoint = '/monitoring/hosts';
    $endpoint .= '/' . $options{host_id} if (defined($options{host_id}));

    my $get_param;
    if (defined($options{search})) {
        $get_param = ['search=' . $options{search}];
    }
    
    return $self->request(
        method => 'GET',
        endpoint => $endpoint,
        get_param => $get_param
    );
}


sub get_platform_versions {
    my ($self, %options) = @_;

    return $self->request(
        method => 'GET',
        endpoint => '/platform/versions'
    );    
}

sub get_scheduling_jobs {
    my ($self, %options) = @_;

    my $get_param;
    if (defined($options{search})) {
        $get_param = ['search=' . $options{search}];
    }

    my $endpoint = '/auto-discovery/scheduling/jobs';
    my ($status, $response) = $self->request(
        method => 'GET',
        endpoint => $endpoint,
        get_param => $get_param
    );
    return $status if $status != 0;

    if (ref $response ne 'HASH' or ! $response->{result}) {
        # failed to parse the http output ?
        return 1;
    }
    my @filtered_jobs = ();

    foreach my $job (@{$response->{result}}) {
        next if (ref $job ne "HASH" || !defined($job->{job_id}));
        push(@filtered_jobs, $job);
    }

    $response = {result => \@filtered_jobs};

    return (0,$response);
}

sub get_api_token {
    my ($self, %options) = @_;

    my $endpoint = '/administration/tokens/' . uri_escape($options{token_name});

    return $self->request(
        method => 'GET',
        endpoint => $endpoint
    );
}

sub DESTROY {
    my ($self) = @_;

    if ($self->{is_logged} == 1) {
        $self->request(
            method => 'GET',
            endpoint => '/logout',
            no_replay => 1
        );
    }
}

1;
