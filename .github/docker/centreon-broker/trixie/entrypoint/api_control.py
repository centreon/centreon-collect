#!/usr/bin/env python3
"""
Minimal HTTP shim for cbd reload/restart, standing in for a real gRPC RPC
until broker gets one of its own. centreon-engine already replaced an
equivalent shim with direct gRPC (Engine/SignalProcess, see commit
b486e99591 "replace api_control.py with direct gRPC management") - broker
never got the same treatment: broker.proto declares no Shutdown/Reload/
SignalProcess RPC, only stats/getters/setters (confirmed by reading it).

Signal semantics verified directly in broker/core/src/main.cc's own
signal_handler(), not assumed:
  - SIGHUP: a real in-process config reload - re-parses the config file,
    re-applies it, and refreshes the AES decrypt key via
    reload_engine_context() too (see container.sh's engine-context.json
    writer). cbd keeps running, no restart.
  - SIGTERM: clean shutdown (gl_term = true, event loop exits).

/restart sends SIGTERM and then exits this wrapper itself, rather than
respawning cbd in-place - mirrors the engine/gRPC convention (SignalProcess
(SHUTDOWN) + Docker's restart policy relaunches the whole container) instead
of a long-lived supervisor process accumulating state across restarts.

Usage: api_control.py <path-to-broker-config.json>
Meant to be the container's real CMD in place of a direct `cbd <config>`
invocation, e.g.:
  python3 api_control.py /etc/centreon-broker/central-broker.json
container.sh's own `.json` arg detection (for log-tail.sh) still finds this
same argument in "$@" unchanged - this script doesn't own log streaming,
that stays log-tail.sh's job (cbd never logs to stdout, only to its own
file).
"""
import asyncio
import os
import signal
import subprocess
import sys
from contextlib import asynccontextmanager

import uvicorn
from fastapi import FastAPI

if len(sys.argv) < 2:
    print("usage: api_control.py <path-to-broker-config.json>", file=sys.stderr)
    sys.exit(1)

CONFIG_PATH = sys.argv[1]
CBD_BIN = "/usr/sbin/cbd"

cbd_proc = None


def start_cbd():
    global cbd_proc
    # preexec_fn=os.setpgrp puts cbd in its own process group so a signal
    # sent via killpg reaches cbd (and any children it spawns) without also
    # hitting this wrapper process.
    cbd_proc = subprocess.Popen([CBD_BIN, CONFIG_PATH], preexec_fn=os.setpgrp)
    print(f"cbd started with PID {cbd_proc.pid}", flush=True)


@asynccontextmanager
async def lifespan(app: FastAPI):
    start_cbd()
    yield


app = FastAPI(lifespan=lifespan)


@app.post("/reload")
def reload_cbd():
    if cbd_proc is None or cbd_proc.poll() is not None:
        return {"error": "cbd is not running"}
    os.killpg(os.getpgid(cbd_proc.pid), signal.SIGHUP)
    return {"reload": "sent SIGHUP"}


@app.post("/restart")
async def restart_cbd():
    if cbd_proc is not None and cbd_proc.poll() is None:
        os.killpg(os.getpgid(cbd_proc.pid), signal.SIGTERM)
        try:
            cbd_proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            os.killpg(os.getpgid(cbd_proc.pid), signal.SIGKILL)
    # Exit after the response is sent, not before - an immediate os._exit()
    # here would kill uvicorn mid-response and the caller would see a
    # connection reset instead of a clean 200.
    asyncio.get_event_loop().call_later(0.5, lambda: os._exit(0))
    return {"restart": "cbd stopped, exiting for the container restart policy to relaunch"}


@app.get("/health")
def health():
    alive = cbd_proc is not None and cbd_proc.poll() is None
    return {"cbd_running": alive, "pid": cbd_proc.pid if alive else None}


if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8080)
