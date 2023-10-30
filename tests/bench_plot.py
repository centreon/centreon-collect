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
                collection_name.add(m[1])
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
            engine_broker[m[2]][m[1]] = collection_name
    return engine_broker


def list_origin_branch(collection_name: str):
    """! list origins (develop, dev-23.04.x....)
    @param collection_name name of an unqlite collection
    @return a set<str> of origins
    """
    collection = db.collection(collection_name)
    return {row['origin'] for row in collection}


def list_conf(collection_name: str, origin: str):
    """! list machine configuration (cpu memory) for a commit branch (develop, dev-23.04.x....)
    @param collection_name name of an unqlite collection
    @param origin develop or dev-23.04.x
    @return a set<str> that contains strings like <cpu configuration>\n<memory size in Go>
    """
    collection = db.collection(collection_name)
    selected = collection.filter(lambda row: row['origin'] == origin)
    return {
        f"{row['cpu']}\n mem:{int(row['memory_size'] / 1024 / 1024 / 1024)}"
        for row in selected
    }


parser = argparse.ArgumentParser(
    prog='bench_plot.py', description='Draw a summary on the benchs')
parser.add_argument('-f', '--unqlite_file', default='bench.unqlite',
                    help='path of the unqlite database file')
parser.add_argument('-d', '--data_column',
                    default=['query_write_bytes'], nargs='+')
parser.add_argument('-b', '--bucket', default='centreon-collect-robot-report',
                    help='s3 bucket')
args = parser.parse_args()

if args.bucket is not None and download_from_s3(args.unqlite_file, args.bucket) != True:
    exit()

data_column = args.data_column

db = UnQLite(args.unqlite_file)

collection_list = list_collection(db)

test_list = list_test(collection_list)

fig = plt.figure()
plt.subplots_adjust(left=0.22, right=0.98, bottom=0.1, top=0.99)
ax = fig.subplots()
list_com = []
cursor = mplcursors.cursor()

ax_origin_choice = None
origin_choice = None

ax_conf_choice = None
conf_choice = None

engine_or_broker = None
active_collection_name = None


def points_to_value_array(rows):  # sourcery skip: avoid-builtin-shadow
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


# radio bouton handlers
radio_width = 0.1

def prepare_boxplot_data(collection):
    data = {col: [] for col in data_column}
    commits = []
    for row in collection:
        for col in data_column:
            data[col].append(float(row[col]))
        commits.append(row['commit'])

    list_commits = list(set(commits))
    commits_data = {cola: [] for cola in list_commits}
    for row in collection:
        for col in data_column:
            if row['commit'] in commits_data:
                commits_data[row['commit']].append(float(row[col]))
    return commits_data


def boxplot_tooltip(index):
    """Custom tooltip for boxplot"""
    global list_commits, commits
    data = commits.values()
    tooltip_text = "".join(f"commit: {list_commits[index]}\n")
    ax.text(index + 1, 1.1, tooltip_text, ha='center',
            va='center', fontsize=8, color='blue')

def plot_boxplot(collection):
    global ax, list_commits, commits, cursor
    commits = prepare_boxplot_data(collection)
    ax.clear()
    list_com.clear(),
    list_commits = list(set(commits.keys()))
    for list_commit in list_commits:
        list_com.append(list_commit[:8])
    ax.boxplot([commits[col] for col in commits.keys()], labels=list_com)

    ax.set_xlabel("Commits")
    ax.set_ylabel("Normalized Values")
    ax.set_title("Benchmark Data")
    ax.set_xticklabels(list_com, rotation=45, ha='right')
    ax.set_xlim(0, len(list_commits) + 1)
    for index in range(len(list_commits)):
        boxplot_tooltip(index)
    mplcursors.cursor(hover=True, highlight=True).connect(
        "add", lambda sel: sel.annotation.set_text(f"{sel.artist.get_label()}\n Commit : {list_commits[int(sel.artist.get_xdata()[int(sel.index)])]}\n No:{len(commits[list_commits[int(sel.artist.get_xdata()[int(sel.index)])]])}")
    )
    fig.canvas.draw()


def conf_choice_on_clicked(conf: str, origin: str):
    """! conf radiobuttons handler the last step to plot
    @param conf label of the selected configuration
    @param origin label of the selected origin (develop, dev....)
    """
    global ax, lines, tooltip_cursor, fig
    collection = db.collection(active_collection_name)
    points = collection.filter(
        lambda row: row['origin'] == origin and f"{row['cpu']}\n mem:{int(row['memory_size']/1024/1024/1024)}" == conf)
    sorted(points, key=lambda row: row['date_commit'])
    fig.canvas.draw()
    plot_boxplot(points)


def origin_choice_on_clicked(origin):
    """! origin radiobuttons handler it initializes conf radiobuttons 
    @param origin label of the selected origin 
    """
    global active_collection_name, ax_conf_choice, conf_choice
    # there we have collection and origin so we can plot
    collection = db.collection(active_collection_name)
    collection = collection.filter(lambda row: row['origin'] == origin)
    if ax_conf_choice is not None:
        ax_conf_choice.remove()
    ax_conf_choice = plt.axes([0.01, 0.01, 0.18, 0.2])
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
    # there we have collection and origin so we can plot
    collection = db.collection(active_collection_name)
    origins = list_origin_branch(active_collection_name)
    if ax_origin_choice is not None:
        ax_origin_choice.remove()
    ax_origin_choice = plt.axes([0.01, 0.23, radio_width, 0.2])
    origin_choice = RadioButtons(ax_origin_choice, list(origins))
    origin_choice.on_clicked(origin_choice_on_clicked)
    origin_choice_on_clicked(next(itertools.islice(origins, 1)))


ax_test_choice = None
test_choice = None


def engine_broker_button_on_clicked(label):
    """! engine/broker radiobuttons handler it initializes test radiobuttons
    Status button handler it initializes test radiobuttons
    exemple: 10000STATUS; 1000STATUSE; 1000STATUS
    @param label engine or broker
    """
    global ax_test_choice, test_choice, engine_or_broker
    engine_or_broker = label
    new_test_list = list(test_list[label])
    if ax_test_choice is not None:
        ax_test_choice.remove()
    ax_test_choice = plt.axes([0.01, 0.45, radio_width, 0.4])  # type: ignore
    test_choice = RadioButtons(ax_test_choice, new_test_list)
    test_choice.on_clicked(test_choice_on_clicked)
    fig.canvas.draw()
    test_choice_on_clicked(new_test_list[0])


# engine or broker
# xposition, yposition, width, height
ax_engine_broker = plt.axes([0.01, 0.89, radio_width, 0.1])  # type: ignore
engine_broker_button = RadioButtons(
    ax_engine_broker, ['engine', 'broker'])


engine_broker_button.on_clicked(engine_broker_button_on_clicked)


plt.show()
