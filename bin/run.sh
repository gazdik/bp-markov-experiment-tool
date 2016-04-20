#!/bin/bash

del="_"
suffix=".txt"

thresholds="5 6 7 8 9 10 11 12 13 14 15 20 25"
models="classic layered"


for thr in $thresholds; do
  for mod in $models; do
    file=$mod$del$thr$suffix
    ./clMarkovGen -s stats/rockyou.wstat -d dictionaries/phpbb_1-7.dic -t $thresholds -l 1:7 -g 20480000 --model $mod > "results/$file"
  done
done

thresholds="5 6 7 8 9 10 15 20"
limits="50 64 30"

for thr in $thresholds; do
  for mod in $models; do
    for lim in $limits; do
      file=$mod$del$thr$del$lim$suffix
      ./clMarkovGen -s stats/rockyou.wstat -d dictionaries/phpbb_1-7.dic -t $thresholds:$lim -l 1:7 -g 20480000 --model $mod > "results/$file"
    done
  done
done


