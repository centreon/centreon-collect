#!/usr/bin/python3
#
# Copyright 2024 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For more information : contact@centreon.com
#
# This script takes a file produced by centreon-malloc-trace library  and remove all malloc free pairs

import sys
import getopt
import csv




def main(argv):
    csv.field_size_limit(sys.maxsize)
    inputfile = ''
    outputfile = ''
    opts, args = getopt.getopt(argv,"hi:o:",["in_file=","out_file="])
    for opt, arg in opts:
        if opt == '-h':
            print ('remove_malloc_free.py -i <inputfile> -o <outputfile>')
            return
        elif opt in ("-i", "--in_file"):
            inputfile = arg
        elif opt in ("-o", "--out_file"):
            outputfile = arg

    if inputfile == '' or outputfile == '':
        print ('remove_malloc_free.py -i <inputfile> -o <outputfile>')
        return
    allocated = {}
    with open(inputfile) as csv_file:
        csv_reader = csv.reader(csv_file, delimiter=';')
        for row in csv_reader:
            if len(row) > 2 and row[2].isdigit():
                if row[0].find('free') >= 0:
                    if (row[2] in allocated):
                        allocated.pop(row[2])
                else:
                    allocated[row[2]] = row
    with open(outputfile, 'w') as f:
        for row in allocated.values():
            f.write(';'.join(row) )
            f.write('\n')


if __name__ == "__main__":
   main(sys.argv[1:])
