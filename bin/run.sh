#!/bin/bash

# stat dict thresholds min max
function run_simple 
{ 
  stat=$1
  dict=$2
  thresholds=$3
  min=$4
  max=$5
  models="classic layered"
  
  for thr in $thresholds; do
    for mod in $models; do   
      out_file="$mod-$thr.txt"    
      echo "$mod $thr"
      ./clMarkovGen -s "stats/$stat.wstat" -d "dictionaries/$dict.dic" -t $thr -l "$min:$max" -g 10240000 --model $mod > "results/$stat-$dict/$out_file"
    done
  done
}

# stat dict thresholds limits min max
function run_comp {
  stat=$1
  dict=$2
  thresholds=$3
  limits=$4
  min=$5
  max=$6 
  models="classic layered"
  
  for thr in $thresholds; do
    for mod in $models; do   
      for lim in $limits; do
        out_file="$mod-$thr-$lim.txt"
        echo "$mod $thr:$lim"   
        ./clMarkovGen -s "stats/$stat.wstat" -d "dictionaries/$dict.dic" -t "$thr:$lim" -l "$min:$max" -g 10240000 --model $mod > "results/$stat-$dict/$out_file"
      done
    done
  done
}

stat=$1
dict=$2
min=$3
max=$4 

mkdir "results/$stat-$dict"

thresholds="5 6 7 8 9 10 11 12 13 14 15 20 25 30 35 40 45 50 55 60 65 70 75"
#run_simple $stat $dict "$thresholds" $min $max

thresholds="5 6 7 8 9 10 11 12 13 14 15 20 25 30 35 40 45 50"
limits="50"
#run_comp $stat $dict "$thresholds" "$limits" $min $max

thresholds="5 6 7 8 9 10 11 12 13 14 15 20 25 30 35 40 45 50 55 60 64"
limits="64"
#run_comp $stat $dict "$thresholds" "$limits" $min $max

thresholds="5 6 7 8 9 10 11 12 13 14 15 20 25 30"
limits="30"
run_comp $stat $dict "$thresholds" "$limits" $min $max



