#!/bin/bash

LOG_DIR="/home/leedaeeun/test"
VALUE_SIZE=4096 # 원하는 값 크기 설정
NUM_RECORDS=500000

# 데이터 저장 경로 생성
mkdir -p ~/test/rocksdb_primary && mkdir -p ~/test/rocksdb_secondary

#########################
# 1. Primary Fill (데이터 채우기)
#########################
echo "Running Primary fillrandom with auto compactions enabled..."
time ./db_bench \
  --benchmarks="fillrandom,levelstats,stats,memstats" \
  --histogram \
  --statistics \
  --db=/home/leedaeeun/test/rocksdb_primary \
  --num=${NUM_RECORDS} \
  --disable_auto_compactions=0 \
  --max_background_jobs=16 \
  --value_size="${VALUE_SIZE}" | tee "$LOG_DIR/Primary_fillrandom.log"
sleep 3

# #########################
# # 2. Primary Read (읽기 성능 측정)
# #########################
# echo "Running Primary readrandom with auto compactions enabled..."
# time ./db_bench \
#   --benchmarks="readrandom,levelstats,stats,memstats" \
#   --histogram \
#   --statistics \
#   --db=/home/leedaeeun/test/rocksdb_primary \
#   --use_existing_db=1 \
#   --num=${NUM_RECORDS} \
#   --disable_auto_compactions=0 \
#   --max_background_jobs=16 \
#   --value_size="${VALUE_SIZE}" | tee "$LOG_DIR/Primary_readrandom.log"
# sleep 3

#########################
# 3. Secondary Read (Secondary 인스턴스: compaction 비활성화로 읽기 최적화)
#########################
echo "Running Secondary readrandom with auto compactions disabled (read-only mode)..."
time ./db_bench \
  --benchmarks="readrandom,levelstats,stats,memstats" \
  --histogram \
  --statistics \
  --db=/home/leedaeeun/test/rocksdb_primary \
  --secondary_path=/home/leedaeeun/test/rocksdb_secondary \
  --use_existing_db=1 \
  --use_secondary_db=1 \
  --num=${NUM_RECORDS} \
  --disable_auto_compactions=1 \
  --max_background_jobs=16 \
  --value_size="${VALUE_SIZE}" | tee "$LOG_DIR/Secondary_readrandom.log"
sleep 3

#  --secondary_update_interval=10 \
# disable_auto_compactions=1 이면 compaction을 수행 X
# disable_auto_compactions=0 이면 compaction을 수행 O

#########################
# 2. Primary Read (읽기 성능 측정)
#########################
echo "Running Primary readrandom with auto compactions enabled..."
time ./db_bench \
  --benchmarks="readrandom,levelstats,stats,memstats" \
  --histogram \
  --statistics \
  --db=/home/leedaeeun/test/rocksdb_primary \
  --use_existing_db=1 \
  --num=${NUM_RECORDS} \
  --disable_auto_compactions=0 \
  --max_background_jobs=16 \
  --value_size="${VALUE_SIZE}" | tee "$LOG_DIR/Primary_readrandom.log"
sleep 3
