#!/usr/bin/python3
import math
import os
import re
import sys
from xml.etree import ElementTree as ET
import matplotlib.pyplot as plt
import datetime

content = os.listdir('.')
r = re.compile(r"\d+")
dirs = []
tests = []
gl_durations = []
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
            for p in root.findall('.//test'):
                for s in p.findall('./status'):
                    starttime = datetime.datetime.strptime(s.attrib['starttime'], '%Y%m%d %H:%M:%S.%f')
                    endtime = datetime.datetime.strptime(s.attrib['endtime'], '%Y%m%d %H:%M:%S.%f')
                    duration = endtime - starttime
                    durations.append((duration, p.attrib['name'], s.attrib['status']))
                    if s.attrib['status'] == 'FAIL':
                        fail += 1
                    if s.attrib['status'] == 'PASS':
                        success += 1
                    count += 1
            durations.sort(reverse = True)
            durations = durations[:10]
            gl_durations.append(durations)
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

plt.figure()
plt.title("Centreon-Tests")
plt.xlabel('date')
plt.xticks(rotation=45)
plt.ylabel('tests')
plt.subplots_adjust(bottom=0.226)
plt.stackplot(x, ys, yf, labels=['Success', 'failure'])
plt.legend(loc='upper left')
plt.grid(color='gray', linestyle='dashed')
plt.show()
