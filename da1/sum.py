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
                       da1=dict(ts=None, verified=None, prabowo=0, jokowi=0, total=0))
    if level == 3:
        kcids.append(pid)

for pid in pids:
    if pid == 0:
        continue
    ppid = lokasi[pid]['ppid']
    lokasi[ppid]['ch'].append(pid)

# DA1

for line in sys.stdin:
    line = line.strip()
    kec_id, ts, js = line.split(' ', 2)
    kec_id = int(kec_id)
    data = json.loads(js)

    da1 = lokasi[kec_id]['da1']
    da1['prabowo'] = data['ph'][-1]
    da1['jokowi'] = data['jj'][-1]
    da1['total'] = data['ss'][-1]
    da1['verified'] = data['ver']
    da1['ts'] = dateutil.parser.parse(data['ts'])

    # To kelurahan

    names = data['kel']
    # if len(names) != len(set(names)):
    #     print kec_id, names
    lnames = [lokasi[cid]['nama'] for cid in lokasi[kec_id]['ch']]
    if names != lnames:
        print kec_id, names, lnames
    assert names == lnames

    for cid, prabowo, jokowi, total in zip(lokasi[kec_id]['ch'], data['ph'], data['jj'], data['ss']):
        cda1 = lokasi[cid]['da1']
        cda1['prabowo'] = prabowo
        cda1['jokowi'] = jokowi
        cda1['total'] = total
        cda1['ts'] = data['ts']

    # To parents

    ppid = lokasi[kec_id]['ppid']
    while True:
        pda1 = lokasi[ppid]['da1']
        pda1['prabowo'] += da1['prabowo']
        pda1['jokowi'] += da1['jokowi']
        pda1['total'] += da1['total']
        if pda1['ts'] is None or pda1['ts'] < da1['ts']:
            pda1['ts'] = da1['ts']

        if ppid == 0:
            break
        ppid = lokasi[ppid]['ppid']

# Count

clokasi = dict()
def count(pid):
    c = [0, 0, 0]
    if lokasi[pid]['level'] == 3:
        da1 = lokasi[pid]['da1']
        # print pid, lokasi[pid]['level'], da1['verified']
        if da1['verified'] is None:
            c = [0, 0, 1]
        elif da1['verified']:
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
    f = open('da1-data/%s.json' % pid, 'w')
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

da1all = dict()
for pid in lokasi:
    level = lokasi[pid]['level']

    cp, cf, ct = get_count(pid)
    da1 = lokasi[pid]['da1']
    data = dict(da1=[pid, lokasi[pid]['nama'], da1['prabowo'], da1['jokowi'], da1['total'], cp, cf, ct],
                ts=da1['ts'],
                children=[])
    da1all[pid] = [data['da1'][idx] for idx in [2, 3, 4, 6, 7]]

    da1['id'] = pid
    da1['children'] = []
    for cid in lokasi[pid]['ch']:
        cp, cf, ct = get_count(cid)
        cda1 = lokasi[cid]['da1']
        cdata = [cid,lokasi[cid]['nama'],
                 cda1['prabowo'],cda1['jokowi'],cda1['total'], cp, cf, ct]
        data['children'].append(cdata)

    write(pid, data)
    print pid, lokasi[pid]['nama']

# All in one

write('all', da1all)


