from bs4 import BeautifulSoup
import sys
import csv
from datetime import datetime
import requests
import json

flokasi = sys.argv[1]

# Lokasi

lokasi = dict()
prids = []
r = csv.reader(open(flokasi))
r.next()
for row in r:
    pid, ppid, level = map(int, row[:3])
    nama = row[3]
    lokasi[pid] = dict(pid=pid, ppid=ppid, level=level, nama=nama)
    if level == 1:
        prids.append(pid)


def log(*txt):
    t = ' '.join(map(str, txt))
    flog = open('dc1.log', 'a')
    flog.write("%s\n" % t)
    flog.close()
    print t

def parse(html):
    s = BeautifulSoup(html)
    t = s.find_all('table')[-1]

    trs = t.find_all('tr')
    tkab = trs[2]
    tph = trs[3]
    tjj = trs[4]
    tsum = trs[5]

    def suara(k):
        if k == '':
            return 0
        return int(k)

    kab = [k.text.strip() for k in tkab.find_all('th')[2:]]
    ph = [suara(k.text.strip()) for k in tph.find_all('td')[2:]]
    jj = [suara(k.text.strip()) for k in tjj.find_all('td')[2:]]
    ss = [suara(k.text.strip()) for k in tsum.find_all('td')[2:]]

    assert(sum(ph[:-1]) == ph[-1])
    assert(sum(jj[:-1]) == jj[-1])
    assert(sum(ss[:-1]) == ss[-1])

    for i in range(len(kab)):
        assert ph[i] + jj[i] == ss[i]

    ver = None
    for div in s.find_all('div'):
        if 'infoboks' == div.attrs.get('id'):
            ver = div.find('strong').text.strip() == 'sudah'

    data=dict(ver=ver,
              kab=kab,
              ph=ph,
              jj=jj,
              ss=ss,
              ts=datetime.now().isoformat())
    return data

S = requests.Session()

def sedot(pid):
    print '#', pid, lokasi[pid]['nama']
    url = 'http://pilpres2014.kpu.go.id/dc1.php?cmd=select&parent=%s' % pid
    html = S.get(url).text
    data = parse(html)
    log(pid, data['ts'], json.dumps(data))

for pid in prids:
    sedot(pid)

