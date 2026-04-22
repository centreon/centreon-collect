#!/usr/bin/env python3
"""
Minimal HTTPS server simulating a HashiCorp Vault approle login endpoint.
Requires only standard library modules (ssl, http.server, json, subprocess).
"""
import json
import socket
import ssl
import subprocess
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer


class HTTPServerV6(HTTPServer):
    """HTTPServer variant that listens on IPv6 (dual-stack on most Linux systems)."""
    address_family = socket.AF_INET6


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


def main():
    pem_file = "/tmp/vault.pem"
    result = subprocess.run(
        [
            "openssl", "req", "-new", "-x509", "-newkey", "rsa:2048",
            "-nodes", "-keyout", pem_file, "-out", pem_file,
            "-days", "365", "-subj", "/CN=localhost",
        ],
        capture_output=True,
    )
    if result.returncode != 0:
        print(f"openssl failed: {result.stderr.decode()}", file=sys.stderr)
        sys.exit(1)

    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.minimum_version = ssl.TLSVersion.TLSv1_2
    ctx.load_cert_chain(pem_file)

    # Bind on :: so the server accepts both IPv4 (127.0.0.1) and IPv6 ([::1])
    server = HTTPServerV6(("::", 4443), VaultHandler)
    server.socket = ctx.wrap_socket(server.socket, server_side=True)
    print("Vault HTTPS server started on port 4443", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
