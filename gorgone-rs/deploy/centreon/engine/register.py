#!/usr/bin/env python3
"""Registers the engine container as a poller of the bench.

register: declares the poller in centreon-web and writes the configuration of
          its gorgone-agent sidecar (the node id is the id web gave it).
deploy:   waits for gorgone-server to see the agent, then asks centreon-web to
          generate, push and restart, as an operator would.
"""

import base64
import json
import os
import sys
import time
import urllib.parse
import urllib.request
from typing import Any, Optional

WEB = "http://" + os.environ.get("WEB_HOST", "web")
GORGONE = "http://{}:{}".format(
    os.environ.get("GORGONE_HOST", "gorgone-server"),
    os.environ.get("GORGONE_API_PORT", "8085"),
)
POLLER = os.environ.get("POLLER_NAME", "engine")
AGENT_CONFIG = "/agent/agent.toml"

AGENT_TEMPLATE = """\
node_id = "{node_id}"
server_url = "http://{gorgone_host}:{agent_port}"
# Bench value: gorgone-server enrols nodes automatically here.
token = "integration-test"

[commands]
allowlist = []

[engine]
# No command pipe and no systemctl: the engine is another container, reached
# through the socket of a volume shared with it.
grpc_url = "unix:/var/lib/centreon-engine/rw/engine.sock"
allowed_commands = [
  'SCHEDULE_(FORCED_)?(HOST|SVC)_CHECK',
  'ACKNOWLEDGE_(HOST|SVC)_PROBLEM',
  'REMOVE_(HOST|SVC)_ACKNOWLEDGEMENT',
  'SCHEDULE_(HOST|SVC)_DOWNTIME',
  'PROCESS_(HOST|SERVICE)_CHECK_RESULT',
  '(ENABLE|DISABLE)_NOTIFICATIONS',
]

[targets.engine]
path = "/etc/centreon-engine"
prune = false
# Checked by the engine container, through its API, before it is applied.
engine_main_file = "centengine.cfg"

[targets.broker]
path = "/etc/centreon-broker"
prune = false

[targets.vmware]
path = "/etc/centreon"
prune = false

[services.engine]
reload = "grpc:reload"
# The engine exits and its container restart policy starts it again.
restart = "grpc:shutdown"

[services.broker]
# No cbd here: the broker module is reloaded with the engine.
reload = "grpc:reload"
"""


def call(
    url: str,
    payload: Optional[dict[str, Any]] = None,
    headers: Optional[dict[str, str]] = None,
) -> dict[str, Any]:
    """Sends a JSON request; an empty answer gives an empty dict."""
    data = None if payload is None else json.dumps(payload).encode()
    request = urllib.request.Request(url, data=data, headers=headers or {})
    if data is not None:
        request.add_header("Content-Type", "application/json")
    with urllib.request.urlopen(request, timeout=60) as answer:
        body = answer.read()
    return json.loads(body) if body.strip() else {}


def login() -> str:
    credentials = {
        "login": os.environ.get("WEB_API_USERNAME", "admin"),
        "password": os.environ.get("WEB_API_PASSWORD", "Centreon!2021"),
    }
    answer = call(WEB + "/centreon/api/latest/login", {"security": {"credentials": credentials}})
    return answer["security"]["token"]


def poller_id(token: str) -> Optional[int]:
    search = urllib.parse.quote(json.dumps({"name": POLLER}))
    answer = call(
        WEB + "/centreon/api/latest/configuration/monitoring-servers?search=" + search,
        headers={"X-AUTH-TOKEN": token},
    )
    return answer["result"][0]["id"] if answer["result"] else None


def register() -> None:
    token = login()
    if poller_id(token) is None:
        call(
            WEB + "/centreon/api/latest/platform/topology",
            {"name": POLLER, "hostname": POLLER, "type": "poller", "address": POLLER,
             "parent_address": "127.0.0.1"},
            {"X-AUTH-TOKEN": token},
        )
        call(
            WEB + "/centreon/api/index.php?object=centreon_configuration_remote"
            "&action=linkCentreonRemoteServer",
            {"linked_remote_master": "", "linked_remote_slaves": [], "open_broker_flow": False,
             "centreon_central_ip": os.environ.get("WEB_HOST", "web"), "server_ip": POLLER,
             "server_name": POLLER, "server_type": "poller"},
            {"centreon-auth-token": token},
        )
    node_id = poller_id(token)
    if node_id is None:
        sys.exit("centreon-web did not create poller " + POLLER)
    with open(AGENT_CONFIG, "w") as config:
        config.write(AGENT_TEMPLATE.format(
            node_id=node_id,
            gorgone_host=os.environ.get("GORGONE_HOST", "gorgone-server"),
            agent_port=os.environ.get("GORGONE_AGENT_PORT", "5556"),
        ))
    print("poller {} registered with id {}".format(POLLER, node_id))


def deploy() -> None:
    token = login()
    node_id = str(poller_id(token))
    basic = base64.b64encode("{}:{}".format(
        os.environ["GORGONE_API_USERNAME"], os.environ["GORGONE_API_PASSWORD"]).encode()).decode()
    for _ in range(120):
        nodes = call(GORGONE + "/v1/nodes", headers={"Authorization": "Basic " + basic})["nodes"]
        if any(node["id"] == node_id and node["connected"] for node in nodes):
            break
        time.sleep(1)
    else:
        sys.exit("gorgone-server never saw the agent of node " + node_id)
    for action in ("POLLERGENERATE", "CFGMOVE", "POLLERRESTART"):
        answer = call(
            WEB + "/centreon/api/index.php?action=action&object=centreon_clapi",
            {"action": action, "values": POLLER},
            {"centreon-auth-token": token},
        )
        print(action, json.dumps(answer))


if __name__ == "__main__":
    {"register": register, "deploy": deploy}[sys.argv[1]]()
