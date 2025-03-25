#!/usr/bin/perl
use strict;
use warnings;

use HTTP::Daemon::SSL;
use HTTP::Status;
use HTTP::Request;
use JSON;

my $pem_file = '/tmp/vault.pem';

system("openssl req -new -x509 -newkey rsa:2048 -nodes -keyout $pem_file -out $pem_file -days 365 -subj /CN=localhost");

# Création du serveur HTTPS sur le port 4443
my $d = HTTP::Daemon::SSL->new(
    LocalPort => 4443,
    SSL_cert_file => $pem_file,
    SSL_key_file  => $pem_file,
) || die "Impossible de démarrer le serveur HTTPS: $!";

print "Serveur HTTPS démarré sur ", $d->url, "\n";

# Boucle principale pour accepter les connexions
while (my $client_conn = $d->accept) {
    while (my $request = $client_conn->get_request) {
        # Gérer les requêtes POST sur le chemin /v1/auth/approle/login
        if ($request->method eq 'POST' && $request->uri->path eq '/v1/auth/approle/login') {

            # Récupérer les données du corps de la requête POST
            my $content = $request->content;

            # Parse du contenu JSON de la requête (si c'est du JSON)
            my $json_data = eval { decode_json($content) };
            if ($@) {
                # Si erreur dans le JSON, renvoyer une erreur 400 (Bad Request)
                $client_conn->send_response(400, 'Bad Request', undef, 'Invalid JSON');
            } else {
                # Simuler une réponse JSON pour le login
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

                # Envoyer une réponse JSON
                $client_conn->send_response(200, 'OK', undef, encode_json($response_data));
            }
        } else {
            # Gérer les autres chemins (404 Not Found)
            $client_conn->send_error(RC_NOT_FOUND, "Path not found");
        }
    }
    $client_conn->close;
    undef($client_conn);
}
