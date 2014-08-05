import sys
import csv
import json
import os

r = csv.reader(sys.stdin)
r.next()

DIR = dict(da1=sys.argv[1],
           db1=sys.argv[2],
           dc1=sys.argv[3])
DEST = sys.argv[4]

ids = []
for row in r:
    ids.append(int(row[0]))

def read(d, pid):
    p = os.path.join(DIR[d], '%s.json' % pid)
    if not os.path.exists(p):
        return {}

    data = json.load(open(p))

    res = dict()
    res[d] = data[d]
    res['children_%s' % d] = data['children']
    return res

def write(data, pid):
    fname = os.path.join(DEST, '%s.json' % pid)
    f = open(fname, 'w')
    f.write(json.dumps(data))
    f.close()

for pid in ids:
    da1 = read('da1', pid)
    db1 = read('db1', pid)
    dc1 = read('dc1', pid)

    data = {}
    data.update(da1)
    data.update(db1)
    data.update(dc1)

    write(data, pid)
    print pid

