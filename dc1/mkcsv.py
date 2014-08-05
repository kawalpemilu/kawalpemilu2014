import csv
import sys
import json

flokasi = sys.argv[1]

lokasi = dict()
r = csv.reader(open(flokasi))
r.next()
kbids = []
for row in r:
    pid, ppid, level = map(int, row[:3])
    nama = row[3]
    lokasi[pid] = [pid, ppid, level, nama]

    if level == 2:
        kbids.append(pid)

data = json.load(sys.stdin)

w = csv.writer(sys.stdout)
w.writerow(['kabupaten_id', 'kabupaten', 'provinsi', 'prabowo', 'jokowi', 'form_terkumpul', 'form_total'])

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

for kbid in kbids:
    skbid = str(kbid)
    row = [kbid, lokasi[kbid][3], get(1, kbid),
           data[skbid][0], data[skbid][1], data[skbid][3], data[skbid][4]]
    w.writerow(row)

