#!/bin/bash

set -x
set -e

B=/srv/pemilu.dahsy.at/www/api
python combine-data.py $B/da1 $B/db1 $B/dc1 $B/dabc1 < lokasi3.csv

