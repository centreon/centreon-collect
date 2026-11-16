#!/usr/bin/env python3
"""Provisioning of an engine container of the bench, run by its entrypoint.

register: declares the poller in centreon-web when needed and prints the id
          web gave it, which is the node id of its gorgone-agent.
deploy:   waits for gorgone-server to see the agent, then asks centreon-web to
          generate, push and restart, as an operator would.

Only the network is used: no file is shared with another container.
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
    """Logs in, waiting for centreon-web to be up and warm."""
    credentials = {
        "login": os.environ.get("WEB_API_USERNAME", "admin"),
        "password": os.environ.get("WEB_API_PASSWORD", "Centreon!2021"),
    }
    for attempt in range(180):
        try:
            answer = call(
                WEB + "/centreon/api/latest/login", {"security": {"credentials": credentials}}
            )
            return answer["security"]["token"]
        except (OSError, KeyError, ValueError) as error:
            if attempt == 179:
                raise
            print("centreon-web not ready yet:", error, file=sys.stderr)
            time.sleep(5)
    raise RuntimeError("unreachable")


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
             "centreon_central_ip": os.environ.get("BROKER_HOST", "broker"), "server_ip": POLLER,
             "server_name": POLLER, "server_type": "poller"},
            {"centreon-auth-token": token},
        )
    node_id = poller_id(token)
    if node_id is None:
        sys.exit("centreon-web did not create poller " + POLLER)
    print(node_id)


def wait_for_urls() -> None:
    """Waits until every URL of WAIT_FOR_URLS answers, when the variable is set."""
    for url in os.environ.get("WAIT_FOR_URLS", "").split():
        for _ in range(180):
            try:
                urllib.request.urlopen(url, timeout=5).close()
                break
            except OSError:
                time.sleep(1)
        else:
            sys.exit("no answer from " + url)


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
        if action == "POLLERRESTART":
            wait_for_urls()
            # The restart may be this container's: note the deployment first,
            # or the next start would deploy, and restart, again.
            marker = os.environ.get("DEPLOY_MARKER")
            if marker:
                open(marker, "w").close()
        answer = call(
            WEB + "/centreon/api/index.php?action=action&object=centreon_clapi",
            {"action": action, "values": POLLER},
            {"centreon-auth-token": token},
        )
        print(action, json.dumps(answer))


if __name__ == "__main__":
    {"register": register, "deploy": deploy}[sys.argv[1]]()
