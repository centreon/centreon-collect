#!/usr/bin/python3

#
# Copyright 2009-2023 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
# For more information : contact@centreon.com
#

import re
import numpy as np
import argparse
import matplotlib.pyplot as plt
import mplcursors
import itertools
import datetime
import time
import boto3
import os
from unqlite import UnQLite
from matplotlib.widgets import RadioButtons


def download_from_s3(file_path: str, bucket: str):
    try:
        s3_resource = boto3.resource('s3')
        s3_resource.Object(bucket, os.path.basename(
            file_path)).download_file(file_path)
        return True
    except Exception as e:
        print(f"upload to s3  exception:{e}\n")
        print("Have you defined AWS_ACCESS_KEY_ID and AWS_SECRET_ACCESS_KEY env variables?")
        return False


# layout

def list_collection(database):
    """! try to retreive the collection list in an unqlite file
    @param database unqlite database
    """
    collection_name = set()
    collect_name_extract = re.compile(r"(\w+_(?:engine|broker)_\d+).*")
    # one collection per stat
    # in global db, all collection row are object where [0] contains a sort of collection name
    with db.cursor() as cursor:
        for row in cursor:
            m = collect_name_extract.match(row[0])
            if m is not None:
                collection_name.add(m.group(1))
    return collection_name


def list_test(collect_list):
    """! list robot tests
    @param collect_list list of collection returned by list_collection
    @return a {'engine': { 'test_name1':'collection_name1',... }, 'broker': {'test_name1':'collection_name1',...}}
    """
    engine_broker = {'engine': {}, 'broker': {}}
    test_name_extract = re.compile(
        r"\w+_(\w+)_((?:engine|broker)).*")
    for collection_name in collect_list:
        m = test_name_extract.match(collection_name)
        if m is not None:
            engine_broker[m.group(2)][(m.group(1))] = collection_name
    return engine_broker


def list_origin_branch(collection_name: str):
    """! list origins (develop, dev-23.04.x....)
    @param collection_name name of an unqlite collection
    @return a set<str> of origins
    """
    origins = set()
    collection = db.collection(collection_name)
    for row in collection:
        origins.add(row['origin'])
    return origins


def list_conf(collection_name: str, origin: str):
    """! list machine configuration (cpu memory) for a commit branch (develop, dev-23.04.x....)
    @param collection_name name of an unqlite collection
    @param origin develop or dev-23.04.x
    @return a set<str> that contains strings like <cpu configuration>\n<memory size in Go>
    """
    confs = set()
    collection = db.collection(collection_name)
    selected = collection.filter(lambda row: row['origin'] == origin)
    for row in selected:
        confs.add(
            f"{row['cpu']}\n mem:{int(row['memory_size']/1024/1024/1024)}")
    return confs


parser = argparse.ArgumentParser(
    prog='bench_plot.py', description='Draw a summary on the benchs')
parser.add_argument('-f', '--unqlite_file', default='bench.unqlite',
                    help='path of the unqlite database file')
parser.add_argument('-b', '--bucket', default='centreon-collect-robot-report',
                    help='s3 bucket')
args = parser.parse_args()

if args.bucket is not None:
    if download_from_s3(args.unqlite_file, args.bucket) != True:
        exit()

db = UnQLite(args.unqlite_file)

collection_list = list_collection(db)

test_list = list_test(collection_list)

fig = plt.figure()
ax = fig.subplots()
lines = []
tooltip_cursor = None
plt.subplots_adjust(left=0.22, right=0.98, bottom=0.1, top=0.99)

ax_origin_choice = None
origin_choice = None

ax_conf_choice = None
conf_choice = None

engine_or_broker = None
active_collection_name = None

values = []

data_column = ['query_read_bytes', 'query_write_bytes', 'real_read_bytes',
               'real_write_bytes', 'user_time', 'kernel_time', 'event_propagation_delay']


def points_to_value_array(rows):
    """! fill a array from select rows this array can be passed to ax.plot()
    @param rows selected rows
    """
    ret = []
    for col_index in range(len(data_column)):
        ret.append([])
    for row in rows:
        for col_index in range(len(data_column)):
            ret[col_index].append(float(row[data_column[col_index]]))
    for col_index in range(len(data_column)):
        ret[col_index] = np.array(ret[col_index])
        max = np.max(ret[col_index])
        if max > 0:
            ret[col_index] /= max

    # invert x y in ordrer to obtain an array like [[<kernel_time_value1>,<user_time_value1>... ], ]
    plot_lib_ret = []
    for val_index in range(len(rows)):
        to_append = []
        for col_index in range(len(data_column)):
            to_append.append(ret[col_index][val_index])
        plot_lib_ret.append(to_append)

    return plot_lib_ret


def tooltip(sel):
    """! fill tooltip with the nearest data
    """
    global values
    data_index = round(sel.index)
    tooltip_text = ""
    if data_index < len(values):
        for col_index in range(len(data_column)):
            label = data_column[col_index]
            tooltip_text += f"{label}: {values[data_index][label]}\n"
        tooltip_text += f"commit: {values[data_index]['commit']}\n{datetime.datetime.fromtimestamp(values[data_index]['t'])}"

    sel.annotation.set(text=tooltip_text)


# radio bouton handlers
radio_width = 0.18


def conf_choice_on_clicked(conf: str, origin: str):
    """! conf radiobuttons handler the last step to plot
    @param conf label of the selected configuration
    @param origin label of the selected origin (develop, dev....)
    """
    global ax, values, lines, tooltip_cursor, fig
    for line in lines:
        line.remove()
        line = None

    if tooltip_cursor is not None:
        tooltip_cursor.remove()
        tooltip_cursor = None
    # there we have collection and origin so we can plot
    collection = db.collection(active_collection_name)
    points = collection.filter(
        lambda row: row['origin'] == origin and f"{row['cpu']}\n mem:{int(row['memory_size']/1024/1024/1024)}" == conf)
    sorted(points, key=lambda row: row['t'])
    values = points.copy()
    x = np.linspace(0, len(points) - 1, len(points))
    ax.clear()
    lines = ax.plot(x, points_to_value_array(points))
    tooltip_cursor = mplcursors.cursor(
        lines, hover=mplcursors.HoverMode.Transient)
    tooltip_cursor.connect("add", tooltip)
    ax.legend(lines, data_column)
    fig.canvas.draw()


def origin_choice_on_clicked(origin):
    """! origin radiobuttons handler it initializes conf radiobuttons 
    @param origin label of the selected origin 
    """
    global active_collection_name, ax_conf_choice, conf_choice
    if ax_conf_choice is not None:
        ax_conf_choice.remove()
    ax_conf_choice = plt.axes([0.01, 0.01, radio_width, 0.2])
    confs = list_conf(active_collection_name, origin)
    conf_choice = RadioButtons(ax_conf_choice, list(confs), 0, "blue", label_props={
                               'fontsize': {8}})
    conf_choice.on_clicked(
        lambda conf_choice: conf_choice_on_clicked(conf_choice, origin))
    conf_choice_on_clicked(next(itertools.islice(confs, 1)), origin)
    fig.canvas.draw()


def test_choice_on_clicked(label):
    """! test radiobuttons handler it initializes origin radiobuttons 
    @param label of the selected test 
    """
    global active_collection_name, ax_origin_choice, origin_choice
    active_collection_name = test_list[engine_or_broker][label]
    origins = list_origin_branch(active_collection_name)
    if ax_origin_choice is not None:
        ax_origin_choice.remove()
    ax_origin_choice = plt.axes([0.01, 0.23, radio_width, 0.2])
    origin_choice = RadioButtons(ax_origin_choice, list(origins))
    origin_choice.on_clicked(origin_choice_on_clicked)
    origin_choice_on_clicked(next(itertools.islice(origins, 1)))
    fig.canvas.draw()


ax_test_choice = None
test_choice = None


def engine_broker_button_on_clicked(label):
    """! engine/broker radiobuttons handler it initializes test radiobuttons 
    @param label engine or broker
    """
    global ax_test_choice, test_choice, engine_or_broker
    engine_or_broker = label
    new_test_list = list(test_list[label])
    if ax_test_choice is not None:
        ax_test_choice.remove()
    ax_test_choice = plt.axes([0.01, 0.45, radio_width, 0.4])
    test_choice = RadioButtons(ax_test_choice, new_test_list)
    test_choice.on_clicked(test_choice_on_clicked)
    fig.canvas.draw()
    test_choice_on_clicked(new_test_list[0])


# engine or broker
# xposition, yposition, width, height
ax_engine_broker = plt.axes([0.01, 0.89, radio_width, 0.1])
engine_broker_button = RadioButtons(
    ax_engine_broker, ['engine', 'broker'])


engine_broker_button.on_clicked(engine_broker_button_on_clicked)


plt.show()
