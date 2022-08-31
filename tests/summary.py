#!/usr/bin/python3
import argparse
import datetime
import os
import re
from xml.etree import ElementTree as ET

import matplotlib.pyplot as plt

parser = argparse.ArgumentParser(prog='summary.py', description='Draw a summary on the tests historical.')
parser.add_argument('--fail', '-f', action='store_true', help='Add a summary on tests that failed.')
parser.add_argument('--slow', '-s', action='store_true', help='Add a summary on slow tests.')
args = parser.parse_args()

content = os.listdir('.')
r = re.compile(r"\d+")
dirs = []
tests = []
gl_durations = []
gl_avg_duration = []
fail_dict = {}

for f in content:
    durations = []
    success = 0
    fail = 0
    count = 0
    if r.match(f):
        if os.path.isdir(f):
            dirs.append(f)
            tree = ET.parse(f + '/output.xml')
            root = tree.getroot()
            total_duration = 0
            for p in root.findall('.//test'):
                for s in p.findall('./status'):
                    starttime = datetime.datetime.strptime(s.attrib['starttime'], '%Y%m%d %H:%M:%S.%f')
                    endtime = datetime.datetime.strptime(s.attrib['endtime'], '%Y%m%d %H:%M:%S.%f')
                    duration = endtime - starttime
                    total_duration += duration.total_seconds()
                    durations.append((duration, p.attrib['name'], s.attrib['status']))
                    if s.attrib['status'] == 'FAIL':
                        fail += 1
                        if p.attrib['name'] not in fail_dict:
                            fail_dict[p.attrib['name']] = []
                        fail_dict[p.attrib['name']].append(int(starttime.timestamp()))
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
    for l in lst:
        fail_x.append(l[4])
        fail_y.append(l[0])
        print(l[3])

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

if args.fail:
    fig, ax = plt.subplots(3)
else:
    fig, ax = plt.subplots(2)
fig.suptitle("Centreon-Tests")
ax[0].set_ylabel('tests')
ax[0].set_xlabel('date')
ax[0].tick_params(labelrotation=45)
fig.tight_layout()
ax[0].stackplot(x, ys, yf, labels=['Success', 'failure'])
ax[0].legend(loc='upper left')
ax[0].grid(color='gray', linestyle='dashed')

ax[1].set_ylabel('average duration(s)')
ax[1].set_xlabel('date')
ax[1].tick_params(labelrotation=45)
ax[1].plot(x1, y1)
ax[1].grid(color='gray', linestyle='dashed')

if args.fail:
    ax[2].bar(fail_x, fail_y, linewidth=2)
    ax[2].set_ylabel('Fails count')
    ax[2].tick_params(labelrotation=90, labelsize=8)

plt.show()
