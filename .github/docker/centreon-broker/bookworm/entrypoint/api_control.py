from fastapi import FastAPI
import subprocess
import os
import signal
import threading
import sys
import time

app = FastAPI()
cbd_proc = None
cbd_thread = None
log_tail_thread = None
stop_log_tail = threading.Event()

# CBD log file path
CBD_LOG_FILE = "/var/log/centreon-broker/central-broker-master.log"

def stream_output(proc):
    """Stream cbd process stdout to docker logs"""
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
    """Tail the CBD log file and output to stdout for docker logs"""
    print(f"Starting log tail for {CBD_LOG_FILE}")

    # Wait for log file to be created
    wait_time = 0
    while not os.path.exists(CBD_LOG_FILE) and wait_time < 30:
        time.sleep(1)
        wait_time += 1

    if not os.path.exists(CBD_LOG_FILE):
        print(f"Warning: {CBD_LOG_FILE} not found after 30s")
        return

    try:
        with open(CBD_LOG_FILE, 'r') as f:
            # Go to end of file
            f.seek(0, 2)

            while not stop_log_tail.is_set():
                line = f.readline()
                if line:
                    # Prefix with [CBD-LOG] to distinguish from other output
                    sys.stdout.write(f"[CBD-LOG] {line}")
                    sys.stdout.flush()
                else:
                    # No new data, sleep briefly
                    time.sleep(0.1)
    except Exception as e:
        print(f"Error tailing log file: {e}")

@app.on_event("startup")
def start_cbd():
    global cbd_proc, cbd_thread, log_tail_thread

    print("Starting CBD process...")
    cbd_proc = subprocess.Popen(
        ["/usr/sbin/cbd", "/etc/centreon-broker/central-broker.json"],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, preexec_fn=os.setpgrp
    )
    cbd_thread = threading.Thread(target=stream_output, args=(cbd_proc,), daemon=True)
    cbd_thread.start()

    # Start log file tailer
    stop_log_tail.clear()
    log_tail_thread = threading.Thread(target=tail_log_file, daemon=True)
    log_tail_thread.start()

    print(f"CBD started with PID {cbd_proc.pid}")

@app.post("/restart")
def restart_cbd():
    global cbd_proc, cbd_thread
    if cbd_proc is not None:
        try:
            cbd_proc.terminate()
            cbd_proc.wait(timeout=5)
        except Exception:
            try:
                os.killpg(os.getpgid(cbd_proc.pid), signal.SIGKILL)
            except Exception:
                pass
    cbd_proc = subprocess.Popen(
        ["/usr/sbin/cbd", "/etc/centreon-broker/central-broker.json"],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, preexec_fn=os.setpgrp
    )
    cbd_thread = threading.Thread(target=stream_output, args=(cbd_proc,), daemon=True)
    cbd_thread.start()
    return {"start_pid": cbd_proc.pid}

@app.post("/reload")
def reload_cbd():
    global cbd_proc
    if cbd_proc is not None:
        try:
            os.killpg(os.getpgid(cbd_proc.pid), signal.SIGHUP)
            return {"reload": "sent SIGHUP"}
        except Exception as e:
            return {"error": str(e)}
    return {"reload": "cbd not running"}

@app.on_event("shutdown")
def stop_cbd():
    global cbd_proc, log_tail_thread

    print("Shutting down CBD...")

    # Stop log tail thread
    stop_log_tail.set()
    if log_tail_thread is not None:
        log_tail_thread.join(timeout=2)

    # Stop cbd process
    if cbd_proc is not None:
        try:
            cbd_proc.terminate()
            cbd_proc.wait(timeout=5)
        except Exception:
            try:
                os.killpg(os.getpgid(cbd_proc.pid), signal.SIGKILL)
            except Exception:
                pass
        cbd_proc = None

    print("CBD shutdown complete")
