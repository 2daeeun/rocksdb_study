#!/bin/bash

LOG_DIR="/home/leedaeeun/test"
VALUE_SIZE=128 # 원하는 값으로 설정

mkdir -p ~/test/rocksdb_primary && mkdir -p ~/test/rocksdb_secondary

echo "Running Primary fillrandom..."
time ./db_bench --benchmarks="fillrandom,readrandom,levelstats,stats,memstats" \
  --histogram \
  --db=/home/leedaeeun/test/rocksdb_primary \
  --num=10000000 \
  --max_background_jobs=16 \
  --value_size="$VALUE_SIZE" | tee "$LOG_DIR/Primary_fillrandom.log"
sleep 3

echo "Running Primary readrandom..."
time ./db_bench --benchmarks="readrandom,levelstats,stats,memstats" \
  --histogram \
  --db=/home/leedaeeun/test/rocksdb_primary \
  --use_existing_db=1 \
  --num=10000000 \
  --max_background_jobs=16 \
  --value_size="$VALUE_SIZE" | tee "$LOG_DIR/Primary_readrandom.log"
sleep 3

echo "Running Secondary1 (Local SSD)..."
time ./db_bench --benchmarks="readrandom,levelstats,stats,memstats" \
  --histogram \
  --db=/home/leedaeeun/test/rocksdb_primary \
  --secondary_path=/home/leedaeeun/test/rocksdb_secondary \
  --use_existing_db=1 \
  --use_secondary_db=1 \
  --num=10000000 \
  --value_size="$VALUE_SIZE" | tee "$LOG_DIR/Secondary1_local.log"
sleep 3
