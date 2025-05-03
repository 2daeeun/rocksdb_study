#!/bin/bash

########################################
# Primary Instance 실행
########################################
# --threads=8 \
# --max_background_jobs=8

echo "Primary Instance (코어 0-3) 실행 중..."
time taskset -c 0-3 ./db_bench --benchmarks="readrandom,levelstats,stats,memstats" \
  --readonly \
  --histogram=true \
  --statistics \
  --value_size=4096 \
  --num=1000000 \
  --threads=8 \
  --db=/home/leedaeeun/bench_rocksdb/fill_data_1M_fix_ver2 >"./primary_(2_Core).txt" &

########################################
# Secondary Instance 실행
########################################

echo "Secondary Instance (코어 4-7) 실행 중..."
time taskset -c 4-7 ./db_bench --benchmarks="readrandom,levelstats,stats,memstats" \
  --use_secondary_db=1 \
  --secondary_path=/home/leedaeeun/bench_rocksdb/Secondary_path \
  --use_existing_db=1 \
  --histogram=true \
  --statistics \
  --value_size=4096 \
  --num=1000000 \
  --threads=8 \
  --db=/home/leedaeeun/bench_rocksdb/fill_data_1M_fix_ver2 >"./secondary.txt" &
