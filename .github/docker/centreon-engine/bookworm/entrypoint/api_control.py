from fastapi import FastAPI
import subprocess
import os
import signal
import threading
import sys
import time

app = FastAPI()
centengine_proc = None
centengine_thread = None
log_tail_thread = None
stop_log_tail = threading.Event()

# Centengine log file path
CENTENGINE_LOG_FILE = "/var/log/centreon-engine/centengine.log"

def stream_output(proc):
    """Stream centengine process stdout to docker logs"""
    try:
        for raw in iter(proc.stdout.readline, b''):
            if not raw:
                break
            try:
                sys.stdout.write(raw.decode(errors="replace"))
            except Exception:
                sys.stdout.write(str(raw))
            sys.stdout.flush()
    except Exception:
        pass

def tail_log_file():
    """Tail the centengine log file and output to stdout for docker logs"""
    print(f"Starting log tail for {CENTENGINE_LOG_FILE}")

    # Wait for log file to be created
    wait_time = 0
    while not os.path.exists(CENTENGINE_LOG_FILE) and wait_time < 30:
        time.sleep(1)
        wait_time += 1

    if not os.path.exists(CENTENGINE_LOG_FILE):
        print(f"Warning: {CENTENGINE_LOG_FILE} not found after 30s")
        return

    try:
        with open(CENTENGINE_LOG_FILE, 'r') as f:
            # Go to end of file
            f.seek(0, 2)

            while not stop_log_tail.is_set():
                line = f.readline()
                if line:
                    # Prefix with [ENGINE-LOG] to distinguish from other output
                    sys.stdout.write(f"[ENGINE-LOG] {line}")
                    sys.stdout.flush()
                else:
                    # No new data, sleep briefly
                    time.sleep(0.1)
    except Exception as e:
        print(f"Error tailing log file: {e}")

@app.on_event("startup")
def start_centengine():
    global centengine_proc, centengine_thread, log_tail_thread

    print("Starting centengine process...")
    centengine_proc = subprocess.Popen(
        ["/usr/sbin/centengine", "/etc/centreon-engine/centengine.cfg"],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, preexec_fn=os.setpgrp
    )
    centengine_thread = threading.Thread(target=stream_output, args=(centengine_proc,), daemon=True)
    centengine_thread.start()

    # Start log file tailer
    stop_log_tail.clear()
    log_tail_thread = threading.Thread(target=tail_log_file, daemon=True)
    log_tail_thread.start()

    print(f"Centengine started with PID {centengine_proc.pid}")

@app.post("/restart")
def restart_centengine():
    global centengine_proc, centengine_thread
    if centengine_proc is not None:
        try:
            centengine_proc.terminate()  # sends SIGTERM
            centengine_proc.wait(timeout=5)
        except Exception:
            try:
                os.killpg(os.getpgid(centengine_proc.pid), signal.SIGKILL)
            except Exception:
                pass
    centengine_proc = subprocess.Popen(
        ["/usr/sbin/centengine", "/etc/centreon-engine/centengine.cfg"],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, preexec_fn=os.setpgrp
    )
    centengine_thread = threading.Thread(target=stream_output, args=(centengine_proc,), daemon=True)
    centengine_thread.start()
    return {"start_pid": centengine_proc.pid}

@app.post("/reload")
def reload_centengine():
    global centengine_proc
    if centengine_proc is not None:
        try:
            os.killpg(os.getpgid(centengine_proc.pid), signal.SIGHUP)
            return {"reload": "sent SIGHUP"}
        except Exception as e:
            return {"error": str(e)}
    return {"reload": "centengine not running"}

@app.on_event("shutdown")
def stop_centengine():
    global centengine_proc, log_tail_thread

    print("Shutting down centengine...")

    # Stop log tail thread
    stop_log_tail.set()
    if log_tail_thread is not None:
        log_tail_thread.join(timeout=2)

    # Stop centengine process
    if centengine_proc is not None:
        try:
            centengine_proc.terminate()
            centengine_proc.wait(timeout=5)
        except Exception:
            try:
                os.killpg(os.getpgid(centengine_proc.pid), signal.SIGKILL)
            except Exception:
                pass
        centengine_proc = None

    print("Centengine shutdown complete")
