#!/bin/bash

while [ /bin/true ];
do
  echo "Start: $(date -R)" | tee -a loop.log

  [ -f loop-sedot.log ] && savelog -c 100 loop-sedot.log 
  ./sedot.sh | tee loop-sedot.log

  echo "Finish: $(date -R)" | tee -a loop.log
  sleep 86400
done
