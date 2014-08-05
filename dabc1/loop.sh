#!/bin/bash

while [ /bin/true ];
do
  echo "Start: $(date -R)" | tee -a loop.log

  ./combine.sh

  echo "Finish: $(date -R)" | tee -a loop.log
  sleep 43300
done
