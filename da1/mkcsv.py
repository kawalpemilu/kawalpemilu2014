import csv
import sys
import json

flokasi = sys.argv[1]

lokasi = dict()
r = csv.reader(open(flokasi))
r.next()
klids = []
for row in r:
    pid, ppid, level = map(int, row[:3])
    nama = row[3]
    lokasi[pid] = [pid, ppid, level, nama]

    if level == 4:
        klids.append(pid)

data = json.load(sys.stdin)

w = csv.writer(sys.stdout)
w.writerow(['kelurahan_id', 'kelurahan', 'kecamatan', 'kabupaten', 'provinsi', 'prabowo', 'jokowi', 'form_terkumpul', 'form_total'])

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

for klid in klids:
    sklid = str(klid)
    row = [klid, lokasi[klid][3], get(3, klid), get(2, klid), get(1, klid),
           data[sklid][0], data[sklid][1], data[sklid][3], data[sklid][4]]
    w.writerow(row)

