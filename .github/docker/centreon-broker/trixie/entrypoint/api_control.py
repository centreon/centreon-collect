#!/usr/bin/env python3
"""
HTTP shim for cbd reload/restart, standing in until broker gets a real gRPC
RPC for this (engine already has one; broker.proto doesn't).

Signal semantics verified in broker/core/src/main.cc: SIGHUP reloads config
in-place (also refreshes the AES decrypt key), SIGTERM shuts down cleanly.
/restart exits this wrapper after SIGTERM rather than respawning cbd
in-place, so the container's restart policy relaunches it.

Usage: api_control.py <path-to-broker-config.json>
"""
import asyncio
import os
import signal
import subprocess
import sys
import threading
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
    # Own process group so killpg targets cbd, not this wrapper too.
    cbd_proc = subprocess.Popen([CBD_BIN, CONFIG_PATH], preexec_fn=os.setpgrp)
    print(f"cbd started with PID {cbd_proc.pid}", flush=True)
    threading.Thread(target=_watch_cbd, args=(cbd_proc,), daemon=True).start()


def _watch_cbd(proc):
    # If cbd exits on its own (bad config, missing file, crash...), this
    # wrapper must not keep running and reporting "healthy" - exit so the
    # container itself dies and the restart policy relaunches it, same as a
    # direct `cbd <config>` invocation would.
    code = proc.wait()
    print(f"cbd exited with code {code}, exiting wrapper", flush=True)
    # A negative code means cbd died from a signal (e.g. the SIGKILL
    # fallback in /restart) - os._exit() needs a plain 0-255 status.
    os._exit(code if code is not None and code >= 0 else 1)


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
    # Delay exit so the HTTP response is sent before the process dies.
    asyncio.get_event_loop().call_later(0.5, lambda: os._exit(0))
    return {"restart": "cbd stopped, exiting for the container restart policy to relaunch"}


@app.get("/health")
def health():
    alive = cbd_proc is not None and cbd_proc.poll() is None
    return {"cbd_running": alive, "pid": cbd_proc.pid if alive else None}


if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8080)
