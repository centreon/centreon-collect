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

import tkinter as tk
import matplotlib
import argparse
import boto3
import os
import re
from unqlite import UnQLite

# With the following line, matplotlib can interact with Tkinter
matplotlib.use('TkAgg')

import matplotlib.pyplot as plt

from matplotlib.backends.backend_tkagg import (
    FigureCanvasTkAgg,
    NavigationToolbar2Tk
)


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


def extract_tests(collect_list):
    """! Build a tree from the DB content. This tree has two levels, the first one
    is the executable *engine* or *broker*. The second level is the name of the test.

    Args:
        collect_list List of collection returned by list_collection

    Returns:
        A dictionary of the form: {'engine': { 'test_name1':'collection_name1',... }, 'broker': {'test_name1':'collection_name1',...}}
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


class App(tk.Tk):
    def __init__(self, tests_tree):
        super().__init__()

        self.title('Centreon-Collect Benchmarks Tool')

        menu = tk.Menu(self)
        menu_tests = tk.Menu(menu, tearoff=0)
        menu.add_cascade(label="Tests", menu=menu_tests)

        self.collection = tk.StringVar()
        for k in tests_tree:
            menu_collection = tk.Menu(menu_tests, tearoff=0)
            tests = list(tests_tree[k])
            tests.sort()
            for kk in tests:
                menu_collection.add_radiobutton(label=tests_tree[k][kk], variable=self.collection, value=tests_tree[k][kk], command=self.collection_chosen)
            menu_tests.add_cascade(label=k, menu=menu_collection)
        self.config(menu=menu)
        origins_label = tk.Label(self, text = "Origins")
        origins_label.grid(column=0, row=0)
        self.origins_list = tk.Listbox(self, selectmode="SINGLE")
        self.origins_list.grid(column=0, row=1)
        self.origins_list.bind('<<ListboxSelect>>', self.origins_list_changed)

        confs_label = tk.Label(self, text = "Configurations")
        confs_label.grid(column=0, row=2)
        self.confs_list = tk.Listbox(self, selectmode="SINGLE")
        self.confs_list.grid(column=0, row=3)
        self.confs_list.bind('<<ListboxSelect>>', self.confs_list_changed)

        update = tk.Button(self, text = "Update", command=self.update_graph)
        update.grid(column=0, row=4)

        self.fig = plt.figure()
        plt.subplots_adjust(left=0.22, right=0.98, bottom=0.1, top=0.99)
        self.ax = self.fig.subplots()
        self.list_com = []
        self.ax_conf_choice = None

        # create FigureCanvasTkAgg object
        figure_canvas = FigureCanvasTkAgg(self.fig, self)

        # create the toolbar
        toolbar_frame = tk.Frame(master=self)
        toolbar_frame.grid(row=4, column=1)
        NavigationToolbar2Tk(figure_canvas, toolbar_frame)

        figure_canvas.get_tk_widget().grid(column=1, row=0, rowspan=4)

    def collection_chosen(self):
        self.origins_list.delete(0, self.origins_list.size())
        origins = list(list_origin_branch(self.collection.get()))
        origins.sort()
        for i, o in enumerate(origins):
            self.origins_list.insert(i, o)
        self.fig.canvas.draw()

    def origins_list_changed(self, evt):
        # Note here that Tkinter passes an event object to onselect()
        w = evt.widget
        if len(w.curselection()) > 0:
            index = int(w.curselection()[0])
            self.origin = w.get(index)
            confs = list(list_conf(self.collection.get(), self.origin))
            confs.sort()
            self.confs_list.delete(0, self.confs_list.size())
            for i, c in enumerate(confs):
                self.confs_list.insert(i, c)

    def confs_list_changed(self, evt):
        # Note here that Tkinter passes an event object to onselect()
        w = evt.widget
        if len(w.curselection()) > 0:
            index = int(w.curselection()[0])
            self.conf = w.get(index)

    def update_graph(self):
        global db
        collection = db.collection(self.collection.get())
        tmp = self.conf.split('\n mem:')
        cpu = tmp[0]
        mem = int(tmp[1])
        points = collection.filter(lambda row: row['origin'] == self.origin and row['cpu'] == cpu and int(row['memory_size']/1024/1024/1024) == mem)

        points.sort(key=lambda row: row['t'])
        self.fig.canvas.draw()
        self.plot_boxplot(points)

    def plot_boxplot(self, collection):
        commits = self.prepare_boxplot_data(collection)
        self.ax.clear()
        self.list_com.clear(),
        list_commits = list(set(commits.keys()))
        for list_commit in list_commits:
            self.list_com.append(list_commit[:8])
        self.ax.boxplot([commits[col] for col in commits.keys()], labels=self.list_com)

        self.ax.set_xlabel("Commits")
        self.ax.set_ylabel("Normalized Values")
        self.ax.set_title("Benchmark Data")
        self.ax.set_xticklabels(self.list_com, rotation=45, ha='right')
        self.ax.set_xlim(0, len(list_commits) + 1)
        self.fig.canvas.draw()

    #        for index in range(len(list_commits)):
    #            boxplot_tooltip(index)
    #        mplcursors.cursor(hover=True, highlight=True).connect(
    #            "add", lambda sel: sel.annotation.set_text(f"{sel.artist.get_label()}\n Commit : {list_commits[int(sel.artist.get_xdata()[int(sel.index)])]}\n No:{len(commits[list_commits[int(sel.artist.get_xdata()[int(sel.index)])]])}")
    #        )

    def prepare_boxplot_data(self, collection):
        global data_column
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

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        prog='bench_plot.py', description='Draws a summary on the benchmarks')
    parser.add_argument('-f', '--unqlite_file', default='bench.unqlite',
                        help='Path to the unqlite database file')
    parser.add_argument('-d', '--data_column',
                        default=['query_write_bytes'], nargs='+')
    parser.add_argument('-b', '--bucket', default='centreon-collect-robot-report',
                        help='The S3 bucket to use to get the unqlite file')
    parser.add_argument('-e', '--executable', help="Which executable to select for benchmarks, among (engine, broker)")
    parser.add_argument('-l', '--local', action="store_true", default=False, help="To work with a local database file")
    args = parser.parse_args()

    if not args.local and args.bucket is not None and not download_from_s3(args.unqlite_file, args.bucket):
        exit()

    db = UnQLite(args.unqlite_file)
    collection_list = list_collection(db)

    tests_tree = extract_tests(collection_list)
    data_column = args.data_column
    app = App(tests_tree)
    app.mainloop()
