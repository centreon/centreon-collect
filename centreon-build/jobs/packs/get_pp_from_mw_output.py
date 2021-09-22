# this scripts parses middleware pp query to return only pp ids and versions
import sys, json

data = json.load(sys.stdin)['data']
for d in data:
    print(str(d['id']) + " " + d['attributes']['version'])