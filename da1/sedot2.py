import csv
import json
import os
import sys
from collections import OrderedDict
from datetime import datetime

import requests
from bs4 import BeautifulSoup

LEVEL = dict(da1=3,
             db1=2,
             dc1=1)

def read_lokasi(flokasi):
    lokasi = OrderedDict()

    r = csv.reader(open(flokasi))
    r.next() # header
    for row in r:
        pid, ppid, level = map(int, row[:3])
        nama = row[3]

        lokasi[pid] = dict(pid=pid, ppid=ppid, level=level, nama=nama)

    return lokasi

def get_ids(lokasi, level):
    ids = []
    for pid, data in lokasi.iteritems():
        if data['level'] == level:
            ids.append(pid)

    return ids

def parse_surat(s):
    t = s.find_all('table')[-2]

    trs = t.find_all('tr')
    tsub = trs[2]
    tditerima = trs[5]
    trusak = trs[6]
    tsimpan = trs[7]
    tpakai = trs[8]

    def suara(k):
        if k == '':
            return 0
        return int(k)

    sub = [k.text.strip() for k in tsub.find_all('th')[2:]]
    diterima = [suara(k.text.strip()) for k in tditerima.find_all('td')[2:]]
    rusak = [suara(k.text.strip()) for k in trusak.find_all('td')[2:]]
    simpan = [suara(k.text.strip()) for k in tsimpan.find_all('td')[2:]]
    pakai = [suara(k.text.strip()) for k in tpakai.find_all('td')[2:]]

    assert(sum(diterima[:-1]) == diterima[-1])
    assert(sum(rusak[:-1]) == rusak[-1])
    assert(sum(simpan[:-1]) == simpan[-1])
    assert(sum(pakai[:-1]) == pakai[-1])

    for i in range(len(sub)):
        assert rusak[i] + simpan[i] + pakai[i] == diterima[i]

    ver = None
    for div in s.find_all('div'):
        if 'infoboks' == div.attrs.get('id'):
            ver = div.find('strong').text.strip() == 'sudah'

    data=dict(ver=ver,
              sub=sub,
              diterima=diterima,
              rusak=rusak,
              simpan=simpan,
              pakai=pakai)
    return data

def parse_suara(s):
    t = s.find_all('table')[-2]

    trs = t.find_all('tr')
    tsub = trs[11]
    tsah = trs[14]
    ttidaksah = trs[15]
    tsum = trs[16]

    def suara(k):
        if k == '':
            return 0
        return int(k)

    sub = [k.text.strip() for k in tsub.find_all('th')[2:]]
    sah = [suara(k.text.strip()) for k in tsah.find_all('td')[2:]]
    tidaksah = [suara(k.text.strip()) for k in ttidaksah.find_all('td')[2:]]
    ss = [suara(k.text.strip()) for k in tsum.find_all('td')[2:]]

    assert(sum(sah[:-1]) == sah[-1])
    assert(sum(tidaksah[:-1]) == tidaksah[-1])
    assert(sum(ss[:-1]) == ss[-1])

    for i in range(len(sub)):
        assert sah[i] + tidaksah[i] == ss[i]

    ver = None
    for div in s.find_all('div'):
        if 'infoboks' == div.attrs.get('id'):
            ver = div.find('strong').text.strip() == 'sudah'

    data=dict(ver=ver,
              sub=sub,
              sah=sah,
              tidaksah=tidaksah,
              ss=ss)
    return data

def parse_hasil(s):
    t = s.find_all('table')[-1]

    trs = t.find_all('tr')
    tsub = trs[2]
    tph = trs[3]
    tjj = trs[4]
    tsum = trs[5]

    def suara(k):
        if k == '':
            return 0
        return int(k)

    sub = [k.text.strip() for k in tsub.find_all('th')[2:]]
    ph = [suara(k.text.strip()) for k in tph.find_all('td')[2:]]
    jj = [suara(k.text.strip()) for k in tjj.find_all('td')[2:]]
    ss = [suara(k.text.strip()) for k in tsum.find_all('td')[2:]]

    assert(sum(ph[:-1]) == ph[-1])
    assert(sum(jj[:-1]) == jj[-1])
    assert(sum(ss[:-1]) == ss[-1])

    for i in range(len(sub)):
        assert ph[i] + jj[i] == ss[i]

    ver = None
    for div in s.find_all('div'):
        if 'infoboks' == div.attrs.get('id'):
            ver = div.find('strong').text.strip() == 'sudah'

    data=dict(ver=ver,
              sub=sub,
              ph=ph,
              jj=jj,
              ss=ss)
    return data

def parse(html):
    s = BeautifulSoup(html)

    data = dict(surat=parse_surat(s),
                suara=parse_suara(s),
                hasil=parse_hasil(s))
    return data

def log(fname, *txt):
    t = ' '.join(map(str, txt))
    flog = open(fname, 'a')
    flog.write("%s\n" % t)
    flog.close()
    print t

def sedot(http, pid, url, log_file):
    html = http.get(url).text
    data = parse(html)
    data['ts'] = datetime.now().isoformat()
    log(log_file, pid, data['ts'], json.dumps(data))

def main():
    if len(sys.argv) < 4:
        print "Usage: %s lokasi.csv <da1|db1|dc1> <data.log>" % sys.argv[0]
        sys.exit(1)

    flokasi = sys.argv[1]
    form = sys.argv[2]
    log_file = sys.argv[3]
    fname = form

    if form not in LEVEL:
        print 'Invalid form: %s' % form
        sys.exit(2)

    if os.path.exists(log_file):
        print 'Log file exists: %s' % log_file
        sys.exit(3)

    lokasi = read_lokasi(flokasi)

    level = LEVEL[form]
    ids = get_ids(lokasi, level)

    http = requests.Session()
    for pid in ids:
        print '#', pid, lokasi[pid]['nama']
        url = 'http://pilpres2014.kpu.go.id/%s.php?cmd=select&parent=%s' % (fname, pid)
        sedot(http, pid, url, log_file)

if __name__ == '__main__':
    main()

