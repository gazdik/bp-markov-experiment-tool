#!/bin/bash
# Options: 1:stat_file 2:dictionary 3:thresholds 4:min_length 5:max_length
#
# Run experiments. It takes few hours to complete.

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
      if [ ! -f "results/$stat-$dict/$out_file" ]
      then
        ./clMarkovGen -s "stats/$stat.wstat" -d "dictionaries/$dict.dic" -t $thr -l "$min:$max" -g 10240000 -M $mod -p > "results/$stat-$dict/$out_file"
      fi
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
        if [ ! -f "results/$stat-$dict/$out_file" ]
        then
          ./clMarkovGen -s "stats/$stat.wstat" -d "dictionaries/$dict.dic" -t "$thr:$lim" -l "$min:$max" -g 10240000 -M $mod -p > "results/$stat-$dict/$out_file"
        fi
      done
    done
  done
}

stat=$1
dict=$2
min=$3
max=$4

mkdir "results/$stat-$dict"

if [ $max -le 8 ]
then
  thresholds="5 6 7 8 9 10 11 12 13 14 15 20 25 30 35 40 45 50 55 60 65 70 75"
  run_simple $stat $dict "$thresholds" $min $max

  thresholds="5 6 7 8 9 10 11 12 13 14 15 20 25 30 35 40 45"
  limits="50"
  run_comp $stat $dict "$thresholds" "$limits" $min $max

  thresholds="5 6 7 8 9 10 11 12 13 14 15 20 25 30 35 40 45 50 55 60 64"
  limits="64"
  run_comp $stat $dict "$thresholds" "$limits" $min $max

  thresholds="5 6 7 8 9 10 11 12 13 14 15 20 25"
  limits="30"
  run_comp $stat $dict "$thresholds" "$limits" $min $max
fi

if [ $max -eq 8 ]
then
  thresholds="5 6 7 8 9 10 11 12 13 14 15 20 25 30 35 40 45"
  run_simple $stat $dict "$thresholds" $min $max

  thresholds="5 6 7 8 9 10 11 12 13 14 15 20 25 30 35 40 45"
  limits="50"
  run_comp $stat $dict "$thresholds" "$limits" $min $max

  thresholds="5 6 7 8 9 10 11 12 13 14 15 20 25 30 35 40 45"
  limits="64"
  run_comp $stat $dict "$thresholds" "$limits" $min $max

  thresholds="5 6 7 8 9 10 11 12 13 14 15 20 25"
  limits="30"
  run_comp $stat $dict "$thresholds" "$limits" $min $max
fi

if [ $max -eq 9 ]
then
  thresholds="5 6 7 8 9 10 11 12 13 14 15 20 25 30"
  run_simple $stat $dict "$thresholds" $min $max

  thresholds="5 6 7 8 9 10 11 12 13 14 15 20 25 30"
  limits="50"
  run_comp $stat $dict "$thresholds" "$limits" $min $max

  thresholds="5 6 7 8 9 10 11 12 13 14 15 20 25"
  limits="64"
  run_comp $stat $dict "$thresholds" "$limits" $min $max

  thresholds="5 6 7 8 9 10 11 12 13 14 15 20 25"
  limits="30"
  run_comp $stat $dict "$thresholds" "$limits" $min $max
fi
