#!/usr/bin/env python3
"""
Minimal HTTPS server simulating a HashiCorp Vault approle login endpoint.
Requires only standard library modules (ssl, http.server, json, subprocess).
All diagnostic output goes to stdout so the test framework can capture it.
"""
print("vault-server.py: starting", flush=True)

import json
import ssl
import subprocess
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer


def log(msg):
    print(msg, flush=True)


class VaultHandler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass  # silence default access log

    def do_POST(self):
        if self.path == "/v1/auth/approle/login":
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length)
            try:
                json.loads(body)
            except (json.JSONDecodeError, ValueError):
                self.send_response(400)
                self.end_headers()
                self.wfile.write(b"Invalid JSON")
                return

            payload = {
                "request_id": "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",
                "lease_id": "",
                "renewable": False,
                "lease_duration": 0,
                "data": None,
                "wrap_info": None,
                "warnings": None,
                "auth": {
                    "client_token": "hvs.key that does not exist",
                    "accessor": "A0A0A0A0A0A0A0A0A0A0A0A0",
                    "policies": ["default", "john-doe"],
                    "token_policies": ["default", "john-doe"],
                    "metadata": {"role_name": "john-doe"},
                    "lease_duration": 2764800,
                    "renewable": True,
                    "entity_id": "bbbbbbbb-bbbb-cccc-dddd-ffffffffffff",
                    "token_type": "service",
                    "orphan": True,
                    "mfa_requirement": None,
                    "num_uses": 0,
                },
                "mount_type": "",
            }
            data = json.dumps(payload).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
        else:
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b"Not found")


try:
    key_file = "/tmp/vault-key.pem"
    cert_file = "/tmp/vault-cert.pem"

    log("vault-server.py: generating certificate")
    # Use -noenc (OpenSSL 3.x) with a fallback to -nodes (OpenSSL 1.x).
    for noenc_flag in ("-noenc", "-nodes"):
        result = subprocess.run(
            [
                "openssl", "req", "-new", "-x509", "-newkey", "rsa:2048",
                noenc_flag, "-keyout", key_file, "-out", cert_file,
                "-days", "365", "-subj", "/CN=localhost",
            ],
            capture_output=True,
        )
        log(f"vault-server.py: openssl {noenc_flag} returned {result.returncode}")
        if result.returncode == 0:
            break
        log(f"openssl stderr: {result.stderr.decode()}")
    if result.returncode != 0:
        log("vault-server.py: openssl failed with both -noenc and -nodes")
        sys.exit(1)

    log("vault-server.py: creating SSL context")
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)

    log("vault-server.py: loading cert chain")
    ctx.load_cert_chain(certfile=cert_file, keyfile=key_file)

    log("vault-server.py: binding on 0.0.0.0:4443")
    server = HTTPServer(("0.0.0.0", 4443), VaultHandler)

    log("vault-server.py: wrapping socket with SSL")
    server.socket = ctx.wrap_socket(server.socket, server_side=True)

    log("vault-server.py: ready, serving forever")
    server.serve_forever()

except Exception as e:
    log(f"vault-server.py: ERROR: {type(e).__name__}: {e}")
    sys.exit(1)
