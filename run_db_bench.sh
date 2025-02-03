#!/bin/bash

# RocksDB db_bench 실행 셸 스크립트

# 경로 및 로그 파일 변수 정의
BASE_DIR="/home/leedaeeun/bench_rocksdb" # 데이터베이스 저장 베이스 경로
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")       # 현재 날짜와 시간 (폴더 구분용)

# 사용자 입력 받기 (디렉토리 이름)
echo "디렉토리 이름을 입력하세요 (빈 칸으로 두면 timestamp만 사용됩니다):"
read USER_DIR_NAME
# USER_DIR_NAME="수동 입력"

# 사용자 입력이 없으면 timestamp를 디렉토리 이름으로 사용
if [ -z "$USER_DIR_NAME" ]; then
  DB_PATH="${BASE_DIR}/${TIMESTAMP}"
  LOG_FILE="${BASE_DIR}/db_bench_result_${TIMESTAMP}.log" # 디렉토리 이름 대신 timestamp 사용
else
  DB_PATH="${BASE_DIR}/${USER_DIR_NAME}_${TIMESTAMP}"
  LOG_FILE="${BASE_DIR}/db_bench_result_${USER_DIR_NAME}_${TIMESTAMP}.log" # 사용자 입력값을 로그 파일 이름에 반영
fi

# 데이터베이스 경로 디렉토리 생성
mkdir -p "$DB_PATH" # $DB_PATH에 해당하는 디렉토리 생성

# db_bench 실행
echo "벤치마크 시작: $(date)"
time ./db_bench \
  --benchmarks="fillseq,readseq,levelstats,stats" \
  --db="$DB_PATH" \
  --num=100000 \
  --value_size=$((1024 * 256)) \
  --compression_type="snappy" \
  --write_buffer_size=$((1024 * 1024 * 256)) \
  --max_write_buffer_number=6 \
  --max_bytes_for_level_base=$((1024 * 1024 * 1024)) \
  --max_bytes_for_level_multiplier=6 \
  --block_size=$((1024 * 128)) \
  --cache_size=$((1024 * 1024 * 2048)) \
  --target_file_size_base=$((1024 * 1024 * 256)) |
  tee "$LOG_FILE"

# 결과 확인 메시지
echo "벤치마크가 완료되었습니다! $(date)"
echo "데이터베이스 경로: $DB_PATH"
echo "결과 로그 파일: $LOG_FILE"
