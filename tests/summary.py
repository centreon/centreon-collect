#!/usr/bin/python3
import datetime
import os
import re
from xml.etree import ElementTree as ET

import matplotlib.pyplot as plt

content = os.listdir('.')
r = re.compile(r"\d+")
dirs = []
tests = []
gl_durations = []
gl_avg_duration = []

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
for i in range(len(gl_durations)):
    t = gl_durations[i]
    print("############# {:12} ##############".format(dirs[i]))
    for tt in t:
        print("{}: {:20}: {}".format(str(tt[0]), tt[1], tt[2]))
print("#" * 40)

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

plt.show()
