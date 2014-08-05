import json
import sys
import dateutil.parser
import csv
import time

flokasi = sys.argv[1]

# Lokasi

lokasi = dict()
kcids = []
r = csv.reader(open(flokasi))
r.next()
pids = []
for row in r:
    pid, ppid, level = map(int, row[:3])
    pids.append(pid)
    nama = row[3]
    lokasi[pid] = dict(pid=pid, ppid=ppid, level=level, nama=nama,
                       ch=[],
                       db1=dict(ts=None, verified=None, prabowo=0, jokowi=0, total=0))
    if level == 3:
        kcids.append(pid)

for pid in pids:
    if pid == 0:
        continue
    ppid = lokasi[pid]['ppid']
    lokasi[ppid]['ch'].append(pid)

# DB1

for line in sys.stdin:
    line = line.strip()
    kab_id, ts, js = line.split(' ', 2)
    kab_id = int(kab_id)
    data = json.loads(js)

    db1 = lokasi[kab_id]['db1']
    db1['prabowo'] = data['ph'][-1]
    db1['jokowi'] = data['jj'][-1]
    db1['total'] = data['ss'][-1]
    db1['verified'] = data['ver']
    db1['ts'] = dateutil.parser.parse(data['ts'])

    # To kecamatan

    names = data['kec']
    # if len(names) != len(set(names)):
    #     print kab_id, names
    lnames = [lokasi[cid]['nama'] for cid in lokasi[kab_id]['ch']]
    if names != lnames:
        print kab_id, names, lnames
    assert names == lnames

    for cid, prabowo, jokowi, total in zip(lokasi[kab_id]['ch'], data['ph'], data['jj'], data['ss']):
        cdb1 = lokasi[cid]['db1']
        cdb1['prabowo'] = prabowo
        cdb1['jokowi'] = jokowi
        cdb1['total'] = total
        cdb1['ts'] = data['ts']

    # To parents

    ppid = lokasi[kab_id]['ppid']
    while True:
        pdb1 = lokasi[ppid]['db1']
        pdb1['prabowo'] += db1['prabowo']
        pdb1['jokowi'] += db1['jokowi']
        pdb1['total'] += db1['total']
        if pdb1['ts'] is None or pdb1['ts'] < db1['ts']:
            pdb1['ts'] = db1['ts']

        if ppid == 0:
            break
        ppid = lokasi[ppid]['ppid']

# Count

clokasi = dict()
def count(pid):
    c = [0, 0, 0]
    if lokasi[pid]['level'] == 2:
        db1 = lokasi[pid]['db1']
        # print pid, lokasi[pid]['level'], db1['verified']
        if db1['verified'] is None:
            c = [0, 0, 1]
        elif db1['verified']:
            c = [1, 0, 1]
        else:
            c = [0, 1, 1]
    else:
        for cid in lokasi[pid]['ch']:
            cc = count(cid)
            c[0] += cc[0]
            c[1] += cc[1]
            c[2] += cc[2]

    clokasi[pid] = c
    return c

count(0)

# Out

def date_handler(obj):
    if hasattr(obj, 'timetuple'):
        return time.mktime(obj.timetuple())
    return obj

def write(pid, data):
    f = open('db1-data/%s.json' % pid, 'w')
    f.write(json.dumps(data, default=date_handler))
    f.close()

def get_count(pid):
    if pid not in clokasi:
        return [1.0, 1, 1]
    cv, cu, ct = clokasi[pid]
    cp = 0.0
    if ct > 0:
        cp = float(cu+cv)/float(ct)

    return cp, cu+cv, ct

db1all = dict()
for pid in lokasi:
    level = lokasi[pid]['level']
    if level == 4:
        continue

    cp, cf, ct = get_count(pid)
    db1 = lokasi[pid]['db1']
    data = dict(db1=[pid, lokasi[pid]['nama'], db1['prabowo'], db1['jokowi'], db1['total'], cp, cf, ct],
                ts=db1['ts'],
                children=[])
    db1all[pid] = [data['db1'][idx] for idx in [2, 3, 4, 6, 7]]

    db1['id'] = pid
    db1['children'] = []
    if level < 3:
        for cid in lokasi[pid]['ch']:
            cp, cf, ct = get_count(cid)
            cdb1 = lokasi[cid]['db1']
            cdata = [cid,lokasi[cid]['nama'],
                     cdb1['prabowo'],cdb1['jokowi'],cdb1['total'], cp, cf, ct]
            data['children'].append(cdata)

    write(pid, data)
    print pid, lokasi[pid]['nama']

# All in one

write('all', db1all)


