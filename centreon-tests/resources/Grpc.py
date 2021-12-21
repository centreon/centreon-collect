import subprocess
import json
import os

client : str = "./centreon-rpc-client.py"

def json_request(location : str, key1, key2 = None):
    return True

def grpc_request(component : str, exe : str, key1, key2 = None, port : int = 51001):
    return True

def stats_exists_in_file(location : str, key1, key2 = None):
    file = open(location, "r")
    buffer = file.read()
    file.close()
    content = json.loads(buffer)
    if content[key1] and key2 is None:
        return True
    if key2 is not None and content[key1][key2]:
        return True
    return False

def stats_exists_in_grpc(component : str, exe : str, key1, key2 = None, port : int = 51001):
    args = (client, "--component=" + component, "--port=" + str(port), "--exe=" + exe)
    buffer = subprocess.run(args, stdout = subprocess.PIPE).stdout
    content = json.loads(buffer)
    if content[key1] and key2 is None:
        return True
    if key2 is not None and content[key1][key2]:
        return True
    return False

def get_stats_in_file(location : str, key1, key2 = None):
    file = open(location, "r")
    buffer = file.read()
    file.close()
    content = json.loads(buffer)
    if (key2 is not None):
        return content[key1][key2]
    return content[key1]

def get_stats_in_grpc(component : str, exe : str, key1, key2 = None, port : int = 51001):
    args = (client, "--component=" + component, "--port=" + str(port), "--exe=" + exe)
    buffer = subprocess.run(args, stdout = subprocess.PIPE).stdout
    content = json.loads(buffer)
    if (key2 is not None):
        return content[key1][key2]
    return content[key1]