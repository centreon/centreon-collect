import subprocess
import json
import os

client : str = "./centreon-rpc-client.py"

def json_stats_request(location : str, key1, key2 = None):
    file = open(location, "r")
    buffer = file.read()
    file.close()
    content = json.loads(buffer)
    if content[key1] and key2 is None:
        return (True, content[key1])
    if key2 is not None and content[key1][key2]:
        return (True, content[key1][key2])
    return (False, None)

def grpc_stats_request(component : str, exe : str, key1, key2 = None, port : int = 51001):
    args = (client, "--component=" + component, "--port=" + str(port), "--exe=" + exe)
    buffer = subprocess.run(args, stdout = subprocess.PIPE).stdout
    content = json.loads(buffer)
    if content[key1] and key2 is None:
        return (True, content[key1])
    if key2 is not None and content[key1][key2]:
        return (True, content[key1][key2])
    return (False, None)

def grpc_stats_request_args(component : str, exe : str, arg : str, key1, key2 = None, port : int = 51001):
    args = (client, "--component=" + component, "--port=" + str(port), "--exe=" + exe, "--args=" + arg)
    buffer = subprocess.run(args, stdout = subprocess.PIPE).stdout
    content = json.loads(buffer)
    if content[key1] and key2 is None:
        return (True, content[key1])
    if key2 is not None and content[key1][key2]:
        return (True, content[key1][key2])
    return (False, None)