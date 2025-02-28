#!/bin/bash

LOG_DIR="/home/leedaeeun/test"
VALUE_SIZE=8192 # 원하는 값으로 설정

mkdir -p ~/test/rocksdb_primary && mkdir -p ~/test/rocksdb_secondary

# echo "Running Primary fillrandom..."
# time ./db_bench --benchmarks="fillrandom,readrandom,levelstats,stats,memstats" \
#   --histogram \
#   --db=/home/leedaeeun/test/rocksdb_primary \
#   --num=1000000 \
#   --max_background_jobs=16 \
#   --value_size="$VALUE_SIZE" | tee "$LOG_DIR/Primary_fillrandom.log"
# sleep 3

echo "Running Primary fillrandom..."
time ./db_bench --benchmarks="fillrandom,levelstats,stats,memstats" \
  --histogram \
  --db=/home/leedaeeun/test/rocksdb_primary \
  --num=1000000 \
  --max_background_jobs=16 \
  --value_size="$VALUE_SIZE" | tee "$LOG_DIR/Primary_fillrandom.log"
sleep 3

echo "Running Secondary1 (Local SSD)..."
time ./db_bench --benchmarks="readrandom,levelstats,stats,memstats" \
  --histogram \
  --db=/home/leedaeeun/test/rocksdb_primary \
  --secondary_path=/home/leedaeeun/test/rocksdb_secondary \
  --use_existing_db=1 \
  --use_secondary_db=1 \
  --num=1000000 \
  --value_size="$VALUE_SIZE" | tee "$LOG_DIR/Secondary1_local.log"
sleep 3

# echo "Running Secondary2 (External SSD)..."
# time ./db_bench --benchmarks="readrandom,levelstats,stats,memstats" \
#   --histogram \
#   --db=/home/leedaeeun/test/rocksdb_primary \
#   --secondary_path=/tmp/test \
#   --use_existing_db=1 \
#   --use_secondary_db=1 \
#   --num=1000000 \
#   --value_size="$VALUE_SIZE" | tee "$LOG_DIR/Secondary2_external.log"
# sleep 3

echo "Running Primary readrandom..."
time ./db_bench --benchmarks="readrandom,levelstats,stats,memstats" \
  --histogram \
  --db=/home/leedaeeun/test/rocksdb_primary \
  --use_existing_db=1 \
  --num=1000000 \
  --max_background_jobs=16 \
  --value_size="$VALUE_SIZE" | tee "$LOG_DIR/Primary_readrandom.log"
sleep 3

# echo "Select an option:"
# echo "1) Primary"
# echo "2) Secondary1"
# echo "3) Secondary2"
# read -p "Enter your choice: " choice
#
# case $choice in
# 1)
#   echo "Running Primary..."
#   time ./db_bench --benchmarks="fillrandom,readrandom,levelstats,stats,memstats" \
#     --histogram \
#     --db=/home/leedaeeun/test/rocksdb_primary \
#     --num=1000000 \
#     --max_background_jobs=16 \
#     --value_size="$VALUE_SIZE" | tee "$LOG_DIR/Primary.log"
#   ;;
# 2)
#   echo "Running Secondary1..."
#   time ./db_bench --benchmarks="readrandom,levelstats,stats,memstats" \
#     --histogram \
#     --db=/home/leedaeeun/test/rocksdb_primary \
#     --secondary_path=/home/leedaeeun/test/rocksdb_secondary \
#     --use_existing_db=1 \
#     --use_secondary_db=1 \
#     --num=1000000 \
#     --value_size="$VALUE_SIZE" | tee "$LOG_DIR/Secondary1.log"
#   ;;
# 3)
#   echo "Running Secondary2..."
#   time ./db_bench --benchmarks="readrandom,levelstats,stats,memstats" \
#     --histogram \
#     --db=/home/leedaeeun/test/rocksdb_primary \
#     --secondary_path=/tmp/test \
#     --use_existing_db=1 \
#     --use_secondary_db=1 \
#     --num=1000000 \
#     --value_size="$VALUE_SIZE" | tee "$LOG_DIR/Secondary2.log"
#   ;;
# *)
#   echo "Invalid choice. Exiting..."
#   exit 1
#   ;;
# esac
