#!/bin/bash

set -x
set -e

B=/home/iang/d/db1
cd $B

savelog db1.log

source /home/iang/d/da1/+env/bin/activate
python sedot.py lokasi3.csv

python sum.py lokasi3.csv < db1.log
python mkcsv.py lokasi3.csv < db1-data/all.json > db1-data/db1.csv
gzip -9 -c db1-data/db1.csv > db1-data/db1.csv.gz

rsync -avH --progress $B/db1-data/ /srv/pemilu.dahsy.at/www/api/db1/

# cd ~/git/backup/
# git add db1
# git commit -m "DB1 $(date -R)"
# git push origin master

