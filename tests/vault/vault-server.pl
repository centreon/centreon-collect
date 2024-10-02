#!/usr/bin/perl
use strict;
use warnings;

use HTTP::Daemon::SSL;
use HTTP::Status;
use HTTP::Request;
use JSON;

my $pem_file = '/tmp/vault.pem';

system("openssl req -new -x509 -newkey rsa:2048 -nodes -keyout $pem_file -out $pem_file -days 365 -subj /CN=localhost");

while (1) {
  my $d = HTTP::Daemon::SSL->new(
      ReuseAddr => 1,
      LocalPort => 4443,
      SSL_cert_file => $pem_file,
      SSL_key_file  => $pem_file,
  ) || die "Unable to start the HTTPS server: $!";

  print "HTTPS server started on ", $d->url, "\n";

  # Main loop where we accept connections
  while (my $client_conn = $d->accept) {
    print "Accepting...\n";
      while (my $request = $client_conn->get_request) {
    print "New request... ", $request->method, " ; path ", $request->uri->path, "\n";
          if ($request->method eq 'POST' && $request->uri->path eq '/v1/auth/approle/login') {
              print "Login from client\n";

              my $content = $request->content;

              # Parsing the request.
              my $json_data = eval { decode_json($content) };
              if ($@) {
                  $client_conn->send_response(400, 'Bad Request', undef, 'Invalid JSON');
              } else {
                  my $response_data = {
                    request_id => "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",
                    lease_id => "",
                    renewable => JSON::false,
                    lease_duration => 0,
                    data => undef,
                    wrap_info => undef,
                    warnings => undef,
                    auth => {
                      client_token => "hvs.key that does not exist",
                      accessor => "A0A0A0A0A0A0A0A0A0A0A0A0",
                      policies => ["default", "john-doe"],
                      token_policies => ["default", "john-doe"],
                      metadata => {
                        role_name => "john-doe"
                      },
                      lease_duration => 2764800,
                      renewable => JSON::true,
                      entity_id => "bbbbbbbb-bbbb-cccc-dddd-ffffffffffff",
                      token_type => "service",
                      orphan => JSON::true,
                      mfa_requirement => undef,
                      num_uses => 0
                    },
                    mount_type => ""
                  };

                  # Send JSON response
                  $client_conn->send_response(200, 'OK', undef, encode_json($response_data));
              }
          } elsif ($request->method eq 'GET' && $request->uri->path =~ m|/v1/johndoe/data/configuration/broker/[0-9a-f\-]*|) {
              print "Get some password\n";
                  my $response_data = {
                      request_id => "c8ecf655-f337-c0a2-36dd-28fa95b541d7",
                      lease_id => "",
                      renewable => JSON::false,
                      lease_duration => 0,
                      data => {
                        data => {
                          "central-broker-master-unified-sql_db_password" => "centreon"
                        },
                        metadata => {
                          created_time => "2020-10-10T00:00:00.00000001Z",
                          custom_metadata => undef,
                          deletion_time => "",
                          destroyed => JSON::false,
                          version => 2
                        }
                      },
                      wrap_info => undef,
                      warnings => undef,
                      auth => undef,
                      mount_type => "jd"};

                  $client_conn->send_response(200, 'OK', undef, encode_json($response_data));
          } else {
              $client_conn->send_error(RC_NOT_FOUND, "Path not found");
          }
      }
      $client_conn->close;
      undef($client_conn);
  }
  print "Connection terminated.\n";
}
print "Server termination.\n";
