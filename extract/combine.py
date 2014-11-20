import csv
import sys
import json
import requests
from collections import defaultdict

flokasi = "lokasi3.csv"
fdata = "clean.csv"
release = sys.argv[1]
timestamp = sys.argv[2]
print "fetching da1.json"
URL_BASE = "http://dummy.kawalpemilu.org/" # need to change this to the real url
da1string = requests.get(URL_BASE + "da1/all.json").text
print "fetching da1.json done"
print "fetching db1.json"
db1string = requests.get(URL_BASE + "db1/all.json").text
print "fetching db1.json done"
print "fetching dc1.json"
dc1string = requests.get(URL_BASE + "dc1/all.json").text
print "fetching dc1.json done"
print "fetching ppln.json"
pplnstring = requests.get(URL_BASE + "ppln/all.json").text.replace('\r', '').replace('\n', '')
print "fetching ppln.json done"
filename = "index.dabcppln." + release + ".raw.html"
result = "index.dabcppln." + release + ".html"

# Lokasi
print "reading lokasi3.csv"
lokasi = dict()
r = csv.reader(open(flokasi))
r.next()
for row in r:
    pid, ppid, level, nama, tps = row
    pid, ppid, level, tps = map(int, [pid, ppid, level, tps])
    lokasi[pid] = [pid, ppid, nama, tps, []]

for pid in lokasi:
    if pid == 0:
        continue
    ppid = lokasi[pid][1]
    assert ppid in lokasi
    lokasi[ppid][4].append(pid)
print "reading lokasi3.csv done"

# Suara
suara = dict(prabowo=dict(),
             jokowi=dict(),
             sah=dict(),
             tidaksah=dict(),
             tpsentered=dict(),
             error=dict())
err_acc = dict()
err_tps_id = []
# init suara
for pid in lokasi:
  err_acc[pid] = 0
  suara['prabowo'][pid] = 0
  suara['jokowi'][pid] = 0
  suara['sah'][pid] = 0
  suara['tidaksah'][pid] = 0
  suara['tpsentered'][pid] = 0
  suara['error'][pid] = 0

print "reading", fdata
provs = set()
kabs = set()
kecs = set()
kels = set()
r = csv.reader(open(fdata))
r.next()
k = 0
for row in r:
    provinsi_id, kabupaten_id, kecamatan_id, kelurahan_id, \
    tps_no, prabowo, jokowi, sah, tidaksah, entered_by, error_reported = map(int, row)
    tpsentered = 0
    if entered_by != 0:
      tpsentered = 1
    if error_reported != 0:
      error_reported = 1
    if error_reported != 0 or entered_by ==  0:
      err_tps_id.append(kelurahan_id * 1000 + tps_no)
    s = dict(prabowo = prabowo,
             jokowi = jokowi,
             sah = sah,
             tidaksah = tidaksah,
             tpsentered = tpsentered,
             error = error_reported)
    for pid in [0, provinsi_id, kabupaten_id, kecamatan_id, kelurahan_id]:
        if error_reported != 0:
          err_acc[pid] += 1
        for f in s:
          suara[f][pid] += s[f]

#    tps_id = kelurahan_id * 1000 + tps_no
    lokasi[kelurahan_id][4].append([tps_no, prabowo, jokowi, sah, tidaksah, tpsentered, error_reported])
    provs.add(provinsi_id)
    kabs.add(kabupaten_id)
    kecs.add(kecamatan_id)
    kels.add(kelurahan_id)

# Index
data = []
for pid in lokasi:
    lokasi[pid].append(suara['prabowo'][pid])
    lokasi[pid].append(suara['jokowi'][pid])
    lokasi[pid].append(suara['sah'][pid])
    lokasi[pid].append(suara['tidaksah'][pid])
    lokasi[pid].append(suara['tpsentered'][pid])
    lokasi[pid].append(suara['error'][pid])
    data.append(lokasi[pid])

print "reading", fdata, "done"
print "writing", result
c1string = json.dumps(data, separators=(',',':')).replace('\\', '\\\\').replace("'", "\\'")
newindex = open(filename).read().replace("RAWC1DATA", c1string)
newindex = newindex.replace("RAWDA1DATA", da1string)
newindex = newindex.replace("RAWDB1DATA", db1string)
newindex = newindex.replace("RAWDC1DATA", dc1string)
newindex = newindex.replace("RAWPPLNDATA", pplnstring)
newindex = newindex.replace("TIMESTAMP", timestamp)
f = open(result, 'w')
f.write(newindex)
f.flush()
f.close()
print "writing", result, "done"

error_tps = "tps.id.error"
print "writing", error_tps
f = open(error_tps, 'w')
for e in err_tps_id:
  f.write(str(e) + "\n")
f.flush()
f.close()
print "writing", error_tps, "done"
