#!/usr/bin/python3
import os
import re
from xml.etree import ElementTree as ET
import matplotlib.pyplot as plt

content = os.listdir('.')
r = re.compile(r"\d+")
tests = []
for f in content:
    success = 0
    fail = 0
    count = 0
    if r.match(f):
        if os.path.isdir(f):
            tree = ET.parse(f + '/output.xml')
            root = tree.getroot()
            for p in root.findall('.//test'):
                for s in p.findall('./status'):
                    if s.attrib['status'] == 'FAIL':
                        fail += 1
                    if s.attrib['status'] == 'PASS':
                        success += 1
                    count += 1
            tests.append((f, success, fail))
            print("%s: %d/%d passed tests" % (f, success, count))

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
