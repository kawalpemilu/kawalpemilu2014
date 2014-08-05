#!/bin/bash

set -x
set -e

B=/home/iang/d/da1
cd $B

savelog da1.log

source $B/+env/bin/activate
python sedot.py lokasi3.csv

python sum.py lokasi3.csv < da1.log
python mkcsv.py lokasi3.csv < da1-data/all.json > da1-data/da1.csv
gzip -9 -c da1-data/da1.csv > da1-data/da1.csv.gz

# rsync -avH --progress $B/da1-data/ /srv/pemilu.dahsy.at/www/api/da1/

# cd ~/git/backup/
# git add da1
# git commit -m "DA1 $(date -R)"
# git push origin master

