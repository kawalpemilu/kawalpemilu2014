#!/bin/sh
# usage: ./doit.sh version_number timestamp

unamestr=`uname`
filename="index.dabcppln."$1".html"
timestamp=$2
fzip=$filename".zip"
cp index.dabcppln.ver.raw.html index.dabcppln.$1.raw.html
python combine.py $1 $2
echo "zipping $filename"
zip -9 $fzip $filename
if [[ $unamestr == 'Linux' ]]; then
  md5sum $filename
  md5sum $fzip
else
  md5 $filename
  md5 $fzip
fi
