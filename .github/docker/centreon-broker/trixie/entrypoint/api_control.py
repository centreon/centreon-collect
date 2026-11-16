#!/usr/bin/env python3
"""
HTTP shim for cbd: configuration push, reload and restart, standing in until
broker gets a real gRPC RPC for this (engine already has one; broker.proto
doesn't). Contract: gorgone-rs/docs/containers-contract.md, "Broker control
API".

Signal semantics verified in broker/core/src/main.cc: SIGHUP reloads config
in-place (also re-reads /etc/centreon-engine/engine-context.json, the AES
decrypt key), SIGTERM shuts down cleanly. /restart exits this wrapper after
SIGTERM rather than respawning cbd in-place, so the container's restart
policy relaunches it.

Environment:
- BROKER_CONTROL_TOKEN: when set, every endpoint but GET /health requires
  `Authorization: Bearer <token>`.
- BROKER_WAIT_FOR_CONFIG=true: while the config file is still the packaged
  default, cbd is not started; the first PUT /files/broker that brings
  another config file starts it.

Usage: api_control.py <path-to-broker-config.json>
"""
import asyncio
import hashlib
import hmac
import io
import os
import signal
import subprocess
import sys
import tarfile
import threading
from contextlib import asynccontextmanager

import uvicorn
from fastapi import Depends, FastAPI, HTTPException, Request, Response
from fastapi.responses import JSONResponse

if len(sys.argv) < 2:
    print("usage: api_control.py <path-to-broker-config.json>", file=sys.stderr)
    sys.exit(1)

CONFIG_PATH = sys.argv[1]
CBD_BIN = "/usr/sbin/cbd"
LOG_TAIL = "/usr/local/lib/centreon-broker/log-tail.sh"
DEFAULT_SUM_FILE = "/var/lib/centreon-broker/.default-config.sha256"
MAX_ARCHIVE = 64 * 1024 * 1024
# target -> (destination directory, file mode)
TARGETS = {
    "broker": ("/etc/centreon-broker", 0o644),
    "engine_context": ("/etc/centreon-engine", 0o640),
}

# Popped so that cbd does not inherit it.
TOKEN = os.environ.pop("BROKER_CONTROL_TOKEN", "")
WAIT_FOR_CONFIG = os.environ.get("BROKER_WAIT_FOR_CONFIG", "false") == "true"

cbd_proc = None
waiting = False
restarting = False


def _is_default_config() -> bool:
    try:
        with open(DEFAULT_SUM_FILE) as f:
            default_sum = f.read().strip()
        with open(CONFIG_PATH, "rb") as f:
            return hashlib.sha256(f.read()).hexdigest() == default_sum
    except OSError:
        return False


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
    if restarting:
        # /restart exits itself once its response is sent.
        return
    print(f"cbd exited with code {code}, exiting wrapper", flush=True)
    # A negative code means cbd died from a signal (e.g. the SIGKILL
    # fallback in /restart) - os._exit() needs a plain 0-255 status.
    os._exit(code if code is not None and code >= 0 else 1)


def _start_after_push() -> None:
    global waiting
    # container.sh skipped the log tail while waiting: the log path comes
    # from the config pushed now.
    if os.environ.get("BROKER_LOG_STDOUT", "false") == "true":
        subprocess.run([LOG_TAIL, "--link-stdout", CONFIG_PATH], check=False)
    else:
        subprocess.Popen([LOG_TAIL, CONFIG_PATH])
    waiting = False
    start_cbd()


def require_token(request: Request) -> None:
    if not TOKEN:
        return
    auth = request.headers.get("authorization", "")
    scheme, _, value = auth.partition(" ")
    if scheme.lower() != "bearer" or not hmac.compare_digest(
        value.strip().encode(), TOKEN.encode()
    ):
        raise HTTPException(401, "missing or invalid token",
                            headers={"WWW-Authenticate": "Bearer"})


def _member_parts(name: str) -> list[str]:
    if any(ord(c) < 0x20 or ord(c) == 0x7F for c in name):
        raise ValueError("control character in a member name")
    if name.startswith("/"):
        raise ValueError(f"absolute member name {name!r}")
    parts = [p for p in name.split("/") if p not in ("", ".")]
    if ".." in parts:
        raise ValueError(f"member name {name!r} contains '..'")
    return parts


def _parse_archive(body: bytes) -> list[tuple[list[str], bytes]]:
    """Validates the whole archive before anything is written."""
    files = []
    total = 0
    with tarfile.open(fileobj=io.BytesIO(body), mode="r:") as tar:
        for member in tar:
            parts = _member_parts(member.name)
            if member.isdir():
                continue
            # isreg() is also true for GNU sparse members: refuse them.
            if member.type not in (tarfile.REGTYPE, tarfile.AREGTYPE):
                raise ValueError(f"{member.name!r} is not a regular file")
            if not parts:
                raise ValueError(f"empty member name {member.name!r}")
            total += member.size
            if total > MAX_ARCHIVE:
                raise ValueError("unpacked size above 64 MiB")
            files.append((parts, tar.extractfile(member).read()))
    return files


def _open_dir(dir_fd: int, name: str) -> int:
    # O_NOFOLLOW: never follow a symlink planted in the destination tree.
    flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
    try:
        return os.open(name, flags, dir_fd=dir_fd)
    except FileNotFoundError:
        os.mkdir(name, 0o755, dir_fd=dir_fd)
        return os.open(name, flags, dir_fd=dir_fd)


def _write_atomic(root: str, parts: list[str], data: bytes, mode: int) -> None:
    fds = [os.open(root, os.O_RDONLY | os.O_DIRECTORY)]
    try:
        for part in parts[:-1]:
            fds.append(_open_dir(fds[-1], part))
        dfd, name = fds[-1], parts[-1]
        tmp = f".{name}.{os.getpid()}.tmp"
        fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                     mode, dir_fd=dfd)
        try:
            with os.fdopen(fd, "wb") as f:
                os.fchmod(f.fileno(), mode)
                f.write(data)
                f.flush()
                os.fsync(f.fileno())
            # Replaces a symlink itself, never its target.
            os.replace(tmp, name, src_dir_fd=dfd, dst_dir_fd=dfd)
        except BaseException:
            try:
                os.unlink(tmp, dir_fd=dfd)
            except OSError:
                pass
            raise
    finally:
        for fd in fds:
            os.close(fd)


@asynccontextmanager
async def lifespan(app: FastAPI):
    global waiting
    if WAIT_FOR_CONFIG and _is_default_config():
        waiting = True
        print(f"{CONFIG_PATH} is the packaged default, waiting for a "
              "configuration push before starting cbd", flush=True)
    else:
        start_cbd()
    yield


app = FastAPI(lifespan=lifespan)


@app.put("/files/{target}", dependencies=[Depends(require_token)])
async def put_files(target: str, request: Request) -> Response:
    if target not in TARGETS:
        raise HTTPException(400, f"unknown target {target!r}")
    if request.headers.get("content-type", "").split(";")[0].strip() != "application/x-tar":
        raise HTTPException(400, "Content-Type must be application/x-tar")
    length = request.headers.get("content-length")
    if length is not None and (not length.isdigit() or int(length) > MAX_ARCHIVE):
        raise HTTPException(413, "archive above 64 MiB")
    chunks, size = [], 0
    async for chunk in request.stream():
        size += len(chunk)
        if size > MAX_ARCHIVE:
            raise HTTPException(413, "archive above 64 MiB")
        chunks.append(chunk)
    try:
        files = _parse_archive(b"".join(chunks))
    except (tarfile.TarError, ValueError) as e:
        raise HTTPException(400, f"bad archive: {e}")
    root, mode = TARGETS[target]
    try:
        for parts, data in files:
            _write_atomic(root, parts, data, mode)
    except OSError as e:
        # e.g. a path component that is a symlink or a file (ELOOP, ENOTDIR).
        raise HTTPException(400, f"cannot write {target}: {e.strerror}")
    print(f"{len(files)} file(s) written to {root}", flush=True)
    if target == "broker" and waiting and not _is_default_config():
        _start_after_push()
    return Response(status_code=204)


@app.post("/reload", dependencies=[Depends(require_token)])
def reload_cbd():
    if waiting:
        raise HTTPException(409, "waiting for configuration")
    if cbd_proc is None or cbd_proc.poll() is not None:
        return {"error": "cbd is not running"}
    os.killpg(os.getpgid(cbd_proc.pid), signal.SIGHUP)
    return {"reload": "sent SIGHUP"}


@app.post("/restart", dependencies=[Depends(require_token)])
async def restart_cbd():
    global restarting
    if waiting:
        raise HTTPException(409, "waiting for configuration")
    restarting = True
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
    if waiting:
        return JSONResponse({"state": "waiting_for_config"}, status_code=503)
    alive = cbd_proc is not None and cbd_proc.poll() is None
    return {"cbd_running": alive, "pid": cbd_proc.pid if alive else None}


if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8080)
