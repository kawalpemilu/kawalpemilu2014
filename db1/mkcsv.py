import csv
import sys
import json

flokasi = sys.argv[1]

lokasi = dict()
r = csv.reader(open(flokasi))
r.next()
kcids = []
for row in r:
    pid, ppid, level = map(int, row[:3])
    nama = row[3]
    lokasi[pid] = [pid, ppid, level, nama]

    if level == 3:
        kcids.append(pid)

data = json.load(sys.stdin)

w = csv.writer(sys.stdout)
w.writerow(['kecamatan_id', 'kecamatan', 'kabupaten', 'provinsi', 'prabowo', 'jokowi', 'form_terkumpul', 'form_total'])

def get(level, pid):
    nama = None
    while True:
        if lokasi[pid][2] == level:
            nama = lokasi[pid][3]
            break
        pid = lokasi[pid][1]
        if pid == 0:
            break
    return nama

for kcid in kcids:
    skcid = str(kcid)
    row = [kcid, lokasi[kcid][3], get(2, kcid), get(1, kcid),
           data[skcid][0], data[skcid][1], data[skcid][3], data[skcid][4]]
    w.writerow(row)

