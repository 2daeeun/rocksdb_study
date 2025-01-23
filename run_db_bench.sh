#!/bin/bash

# RocksDB db_bench 실행 셸 스크립트

# 변수 정의
BASE_DIR="/home/leedaeeun/bench_rocksdb"  # 데이터베이스 저장 베이스 경로
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")        # 현재 날짜와 시간
DB_PATH="${BASE_DIR}/${TIMESTAMP}"        # 시간 기반 폴더 경로
LOG_FILE="${DB_PATH}/db_bench_result.log" # 벤치마크 결과 로그 파일 경로
NUM=10000000                              # 총 키-값 쌍 수
VALUE_SIZE=1024                           # 값 크기 (바이트)
KEY_SIZE=16                               # 키 크기 (바이트)
READS=1000000                             # 읽기 요청 수
WRITES=500000                             # 쓰기 요청 수
THREADS=$(nproc)                          # 스레드 수
BENCHMARKS="fillrandom,readrandom"        # 실행할 벤치마크 리스트
COMPRESSION_TYPE="snappy"                 # 압축 유형

# 데이터베이스 경로 디렉토리 생성
mkdir -p "$DB_PATH"

# db_bench 실행
time ./db_bench \
  --db=$DB_PATH \
  --num=$NUM |
  # --value_size=$VALUE_SIZE \
  # --key_size=$KEY_SIZE \
  # --reads=$READS \
  # --writes=$WRITES \
  # --threads=$THREADS \
  # --benchmarks=$BENCHMARKS \
  # --compression_type=$COMPRESSION_TYPE \
  tee "$LOG_FILE"

# 결과 확인 메시지
echo "벤치마크가 완료되었습니다!"
echo "데이터베이스 경로: $DB_PATH"
echo "결과 로그 파일: $LOG_FILE"
