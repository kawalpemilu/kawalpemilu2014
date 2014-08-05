import json
import sys
import dateutil.parser
import csv
import time

flokasi = sys.argv[1]

# Lokasi

lokasi = dict()
r = csv.reader(open(flokasi))
r.next()
pids = []
for row in r:
    pid, ppid, level = map(int, row[:3])
    pids.append(pid)
    nama = row[3]
    lokasi[pid] = dict(pid=pid, ppid=ppid, level=level, nama=nama,
                       ch=[],
                       dc1=dict(ts=None, verified=None, prabowo=0, jokowi=0, total=0))

for pid in pids:
    if pid == 0:
        continue
    ppid = lokasi[pid]['ppid']
    lokasi[ppid]['ch'].append(pid)

# dc1

for line in sys.stdin:
    line = line.strip()
    prov_id, ts, js = line.split(' ', 2)
    prov_id = int(prov_id)
    data = json.loads(js)

    dc1 = lokasi[prov_id]['dc1']
    dc1['prabowo'] = data['ph'][-1]
    dc1['jokowi'] = data['jj'][-1]
    dc1['total'] = data['ss'][-1]
    dc1['verified'] = data['ver']
    dc1['ts'] = dateutil.parser.parse(data['ts'])

    # To kecamatan

    names = data['kab']
    # if len(names) != len(set(names)):
    #     print prov_id, names
    lnames = [lokasi[cid]['nama'] for cid in lokasi[prov_id]['ch']]
    if names != lnames:
        print prov_id, names, lnames
    assert names == lnames

    for cid, prabowo, jokowi, total in zip(lokasi[prov_id]['ch'], data['ph'], data['jj'], data['ss']):
        cdc1 = lokasi[cid]['dc1']
        cdc1['prabowo'] = prabowo
        cdc1['jokowi'] = jokowi
        cdc1['total'] = total
        cdc1['ts'] = data['ts']

    # To parents

    ppid = lokasi[prov_id]['ppid']
    while True:
        pdc1 = lokasi[ppid]['dc1']
        pdc1['prabowo'] += dc1['prabowo']
        pdc1['jokowi'] += dc1['jokowi']
        pdc1['total'] += dc1['total']
        if pdc1['ts'] is None or pdc1['ts'] < dc1['ts']:
            pdc1['ts'] = dc1['ts']

        if ppid == 0:
            break
        ppid = lokasi[ppid]['ppid']

# Count

clokasi = dict()
def count(pid):
    c = [0, 0, 0]
    if lokasi[pid]['level'] == 2:
        dc1 = lokasi[pid]['dc1']
        # print pid, lokasi[pid]['level'], dc1['verified']
        if dc1['verified'] is None:
            c = [0, 0, 1]
        elif dc1['verified']:
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
    f = open('dc1-data/%s.json' % pid, 'w')
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

dc1all = dict()
for pid in lokasi:
    level = lokasi[pid]['level']
    if level == 3 or level == 4:
        continue

    cp, cf, ct = get_count(pid)
    dc1 = lokasi[pid]['dc1']
    data = dict(dc1=[pid, lokasi[pid]['nama'], dc1['prabowo'], dc1['jokowi'], dc1['total'], cp, cf, ct],
                ts=dc1['ts'],
                children=[])
    dc1all[pid] = [data['dc1'][idx] for idx in [2, 3, 4, 6, 7]]

    dc1['id'] = pid
    dc1['children'] = []
    if level < 2:
        for cid in lokasi[pid]['ch']:
            cp, cf, ct = get_count(cid)
            cdc1 = lokasi[cid]['dc1']
            cdata = [cid,lokasi[cid]['nama'],
                     cdc1['prabowo'],cdc1['jokowi'],cdc1['total'], cp, cf, ct]
            data['children'].append(cdata)

    write(pid, data)
    print pid, lokasi[pid]['nama']

# All in one

write('all', dc1all)


