import json
import sys
import csv

import tornado.ioloop
import tornado.web
import tornado
from tornado import autoreload

class Data(object):
    __slots__ = ['ts', 'verified',
                 'data_verified', 'data_unverified', 'data_total',
                 'prabowo', 'jokowi', 'total',
                 'sah', 'tidaksah', 'totalsuara',
                 'diterima', 'rusak', 'simpan', 'pakai']

    def __init__(self):
        self.ts = None
        self.verified = None

        self.data_verified = 0
        self.data_unverified = 0
        self.data_total = 0

        # Hasil
        self.prabowo = 0
        self.jokowi = 0
        self.total = 0

        # Suara
        self.sah = 0
        self.tidaksah = 0
        self.totalsuara = 0

        # Surat suara
        self.diterima = 0
        self.rusak = 0
        self.simpan = 0
        self.pakai = 0

    def add(self, data):
        if self.ts is None or data.ts > self.ts:
            self.ts = data.ts
        if self.verified is None:
            self.verified = data.verified
        if data.verified is not None:
            self.verified &= data.verified

        if data.verified is None:
            pass
        elif data.verified:
            self.data_verified += 1
        else:
            self.data_unverified += 1
        self.data_total += 1

        self.prabowo += data.prabowo
        self.jokowi += data.jokowi
        self.total += data.total

        self.sah += data.sah
        self.tidaksah += data.tidaksah
        self.totalsuara += data.totalsuara

        self.diterima += data.diterima
        self.rusak += data.rusak
        self.simpan += data.simpan
        self.pakai += data.pakai

class Node(object):
    __slots__ = ['pid', 'ppid', 'level', 'nama', 'ch', 'da1', 'db1', 'dc1']

    def __init__(self, pid, ppid, level, nama):
        self.pid = pid
        self.ppid = ppid
        self.level = level
        self.nama = nama
        self.ch = []
        self.da1 = Data()
        self.db1 = Data()
        self.dc1 = Data()

class Tree(object):
    __slots__ = ['nodes', 'ppid']

    nodes = {}
    pids = []

    def add(self, pid, form, data, recursive=True):
        node = self.nodes[pid]
        while True:
            d = getattr(node, form)
            d.add(data)

            if not recursive:
                return

            if node.pid == 0:
                break
            node = self.nodes[node.ppid]

    @staticmethod
    def read(flokasi, fdc1):
        tree = Tree()

        print 'Reading data..'

        r = csv.reader(open(flokasi))
        r.next() # header
        for row in r:
            pid, ppid, level = map(int, row[:3])
            nama = row[3]

            tree.pids.append(pid)
            tree.nodes[pid] = Node(pid, ppid, level, nama)

        print 'Building tree..'
        for pid in tree.pids:
            if pid == 0:
                continue
            node = tree.nodes[pid]
            tree.nodes[node.ppid].ch.append(node)

        print 'Reading DC1..'
        def read_data(form, fname):
            for line in open(fname):
                line = line.strip()
                pid, ts, js = line.split(' ', 2)
                pid = int(pid)

                raw = json.loads(js)
                data, datachildren = Tree._load_data(raw, len(tree.nodes[pid].ch))

                tree.add(pid, form, data)
                for nch, data in zip(tree.nodes[pid].ch, datachildren):
                    tree.add(nch.pid, form, data, recursive=False)

        read_data('dc1', fdc1)

        return tree

    @staticmethod
    def _load_data(raw, nch):
        datalist = []
        for i in range(nch+1):
            data = Data()
            data.prabowo += raw['hasil']['ph'][i]
            data.jokowi += raw['hasil']['jj'][i]
            data.total += raw['hasil']['ss'][i]
            data.sah += raw['suara']['sah'][i]
            data.tidaksah += raw['suara']['tidaksah'][i]
            data.totalsuara += raw['suara']['ss'][i]
            data.diterima = raw['surat']['diterima'][i]
            data.rusak = raw['surat']['rusak'][i]
            data.simpan = raw['surat']['simpan'][i]
            data.pakai = raw['surat']['pakai'][i]
            datalist.append(data)
        return datalist[-1], datalist[:-1]

def data_handler(obj):
    if hasattr(obj, 'timetuple'):
        return time.mktime(obj.timetuple())
    elif isinstance(obj, Data):
        return 'data'
    return obj

def get_data_line(pid, nama, data):
    p = 0.0
    if data.data_total > 0:
        p = float(data.data_verified + data.data_unverified) / data.data_total

    return [pid, nama,
            data.prabowo, data.jokowi, data.total,
            p, data.data_verified+data.data_unverified, data.data_total]

def serialize(form, node):
    data = getattr(node, form)

    d = dict(ts=data.ts, children=[])
    d[form] = get_data_line(node.pid, node.nama, data)

    for nc in node.ch:
        d['children'].append(get_data_line(nc.pid, nc.nama, getattr(nc, form)))

    return json.dumps(d)

class MainHandler(tornado.web.RequestHandler):
    def get(self, form, pid):
        pid = int(pid)
        node = self.application.tree.nodes[pid]
        self.write(serialize(form, node))

class App(tornado.web.Application):
    def __init__(self, flokasi, fdc1, *args, **kwargs):
        super(App, self).__init__(*args, **kwargs)

        self.tree = Tree.read(flokasi, fdc1)

if __name__ == "__main__":
    flokasi = sys.argv[1]
    fdc1 = sys.argv[2]

    application = App(flokasi, fdc1, [
        (r"/(da1|db1|dc1|dabc1)/(\d+)\.json", MainHandler),
    ])
    application.listen(8888)

    ioloop = tornado.ioloop.IOLoop.instance()
    autoreload.start(ioloop)
    ioloop.start()

