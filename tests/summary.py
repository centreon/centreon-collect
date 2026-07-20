#!/usr/bin/python3
import argparse
import datetime
import json
import os
import re
import time
from xml.etree import ElementTree as ET

import matplotlib.pyplot as plt

parser = argparse.ArgumentParser(
    prog='summary.py', description='Draw a summary on the tests historical.')
parser.add_argument('--fail', '-f', action='store_true',
                    help='Add a summary on tests that failed.')
parser.add_argument('--benchmark', '-b', action='store_true',
                    help='Display history of benchmark tests.')
parser.add_argument('--slow', '-s', action='store_true',
                    help='Add a summary on slow tests.')
parser.add_argument('--count', '-c', action='store_true',
                    help='Display some stack bars with successes and failures.')
parser.add_argument('--top', '-t', type=int, default=10,
                    help='Comes with --fail option. Limits the display to the top N.')
args = parser.parse_args()

content = os.listdir('.')
r = re.compile(r"\d+")
dirs = []
tests = []
gl_durations = []
gl_avg_duration = []
fail_dict = {}

top = args.top

if os.path.exists("benchmarks.json"):
    with open("benchmarks.json", "r") as f:
        benchmark = json.load(f)
else:
    benchmark = {}

for f in content:
    durations = []
    success = 0
    fail = 0
    count = 0
    if r.match(f):
        if os.path.isdir(f):
            dirs.append(f)
            print(f"reading {f}/output.xml")
            if not os.path.exists(f"{f}/output.xml"):
                print(f" * {f}/output.xml does not exist")
                continue
            try:
                tree = ET.parse(f"{f}/output.xml")
            except ET.ParseError as e:
                print(f" * {f}/output.xml is not a valid XML file: {e}")
                continue
            root = tree.getroot()
            total_duration = 0
            for p in root.findall('.//test'):
                for s in p.findall('./status'):
                    starttime = datetime.datetime.strptime(
                        s.attrib['start'], '%Y-%m-%dT%H:%M:%S.%f')
                    duration = float(s.attrib['elapsed'])
                    total_duration += duration
                    for t in p.findall('./tag'):
                        if t.text == 'benchmark':
                            if not p.attrib['name'] in benchmark:
                                benchmark[p.attrib['name']] = []
                            t = time.mktime(starttime.timetuple())
                            benchmark[p.attrib['name']].append(
                                (t, float(total_duration)))
                    durations.append(
                        (duration, p.attrib['name'], s.attrib['status']))
                    if s.attrib['status'] == 'FAIL':
                        fail += 1
                        if p.attrib['name'] not in fail_dict:
                            fail_dict[p.attrib['name']] = []
                        fail_dict[p.attrib['name']].append(int(f))
                    if s.attrib['status'] == 'PASS':
                        success += 1
                    count += 1
            durations.sort(reverse=True)
            durations = durations[:10]
            gl_durations.append(durations)
            gl_avg_duration.append((f, total_duration / count))
            tests.append((f, success, fail))
            print("%s: %d/%d passed tests" % (f, success, count))

# Display the arrays of longest tests
if args.slow:
    for i in range(len(gl_durations)):
        t = gl_durations[i]
        print("############# {:12} ##############".format(dirs[i]))
        for tt in t:
            print("{}: {:20}: {}".format(str(tt[0]), tt[1], tt[2]))
    print("#" * 40)

if args.fail:
    lst = []
    fail_x = []
    fail_y = []
    for k in fail_dict:
        s = len(fail_dict[k])
        m = min(fail_dict[k])
        M = max(fail_dict[k])
        n = k
        d = f"############# {k} ##############\n * size = {s}\n * min = {m}\n * max = {M}\n"
        lst.append((s, m, M, d, n))
    lst.sort()
    # We keep the last 10
    lst = lst[-top:]
    names = []
    for l in lst:
        print(l[3])
        names.append(l[4])
    for k in fail_dict:
        if k in names:
            for v in fail_dict[k]:
                fail_y.append(k)
                fail_x.append(v)
    fail_xx = fail_x.copy()
    fail_xx.sort()
    fail_yy = []
    for xx in fail_xx:
        j = fail_x.index(xx)
        fail_yy.append(fail_y[j])
        fail_x[j] = None

    fail_xx = list(map(str, fail_xx))
tests.sort()
x = []
ys = []
yf = []
for xx in tests:
    x.append(xx[0])
    ys.append(xx[1])
    yf.append(xx[2])

gl_avg_duration.sort()
x1 = []
y1 = []
for xx in gl_avg_duration:
    x1.append(xx[0])
    y1.append(xx[1])

size = 0
if args.count:
    size += 1
if args.fail:
    size += 1
if args.slow:
    size += 1
if args.benchmark:
    size += 1

if args.benchmark:
    with open("benchmark.json", "w") as f:
        json.dump(benchmark, f, indent=True)
    print(benchmark)

if size == 0:
    exit(0)

if size == 1:
    fig, ax = plt.subplots()
else:
    fig, ax = plt.subplots(size)
fig.suptitle("Centreon-Tests")
fig.tight_layout()

idx = 0
if args.count:
    if size == 1:
        AX = ax
    else:
        AX = ax[idx]
    AX.set_ylabel('tests')
    AX.set_xlabel('date')
    AX.tick_params(labelrotation=45)

    AX.stackplot(x, ys, yf, labels=['Success', 'failure'])
    AX.legend(loc='upper left')
    AX.grid(color='gray', linestyle='dashed')
    AX.axis(ymin=min(ys))
    idx += 1

if args.slow:
    if size == 1:
        AX = ax
    else:
        AX = ax[idx]

    AX.set_ylabel('average duration(s)')
    AX.set_xlabel('date')
    AX.tick_params(labelrotation=45)
    AX.plot(x1, y1)
    AX.grid(color='gray', linestyle='dashed')
    idx += 1

if args.fail:
    if size == 1:
        AX = ax
    else:
        AX = ax[idx]

    AX.set_xlabel('date')
    AX.set_ylabel('Failed tests')
    AX.set_title("Fail top 10")
    AX.tick_params(axis="x", labelrotation=45, labelsize=8)

    AX.plot(fail_xx, fail_yy, 'ro')
    AX.grid(color='gray', linestyle='dashed')
    idx += 1

if args.benchmark:
    if size == 1:
        AX = ax
    else:
        AX = ax[idx]

    AX.set_xlabel('date')
    AX.tick_params(labelrotation=45)
    values = []
    dates = []
    for k, v in benchmark.items():
        AX.set_ylabel(f'{k} duration(s)')
        for vv in v:
            dates.append(vv[0])
        dates.sort()
        for d in dates:
            for vv in v:
                if vv[0] == d:
                    values.append(vv[1])
                    break

    AX.plot(dates, values)
    AX.grid(color='gray', linestyle='dashed')
    idx += 1

plt.show()
