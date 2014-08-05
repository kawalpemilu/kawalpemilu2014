#!/bin/bash

set -x
set -e

B=/home/iang/d/dc1
cd $B

savelog dc1.log

source /home/iang/d/da1/+env/bin/activate
python sedot.py lokasi3.csv

python sum.py lokasi3.csv < dc1.log
python mkcsv.py lokasi3.csv < dc1-data/all.json > dc1-data/dc1.csv
gzip -9 -c dc1-data/dc1.csv > dc1-data/dc1.csv.gz

rsync -avH --progress $B/dc1-data/ /srv/pemilu.dahsy.at/www/api/dc1/

#  cd ~/git/backup/
#  git add dc1
#  git commit -m "DC1 $(date -R)"
# git push origin master

