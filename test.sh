#!/bin/bash

LOG_DIR="/home/leedaeeun/test"
VALUE_SIZE=8192 # 원하는 값으로 설정

# fillrandom 실행
time ./db_bench --benchmarks="fillrandom,levelstats,stats,memstats" \
  --histogram \
  --db=/home/leedaeeun/test/rocksdb_primary \
  --num=1000000 \
  --max_background_jobs=16 \
  --value_size="$VALUE_SIZE" | tee "$LOG_DIR/Primary_fillrandom.log"

sleep 3
echo "Running Primary and Secondary1 simultaneously with CPU pinning..."

# Primary 실행 (CPU 0, 1, 2, 3번 코어에 할당)
taskset -c 0,1,2,3 bash -c "time ./db_bench --benchmarks='readrandom,levelstats,stats,memstats' \
  --histogram \
  --db=/home/leedaeeun/test/rocksdb_primary \
  --use_existing_db=1 \
  --num=1000000 \
  --value_size='$VALUE_SIZE' | tee '$LOG_DIR/Primary1.log'" &

# Secondary 실행 (CPU 4, 5, 6, 7번 코어에 할당)
taskset -c 4,5,6,7 bash -c "time ./db_bench --benchmarks='readrandom,levelstats,stats,memstats' \
  --histogram \
  --db=/home/leedaeeun/test/rocksdb_primary \
  --secondary_path=/home/leedaeeun/test/rocksdb_secondary \
  --use_existing_db=1 \
  --use_secondary_db=1 \
  --num=1000000 \
  --value_size='$VALUE_SIZE' | tee '$LOG_DIR/Secondary1.log'" &

# 모든 백그라운드 작업이 끝날 때까지 기다림
wait

echo "Both processes are done."
