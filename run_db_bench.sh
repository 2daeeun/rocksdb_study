#!/bin/bash

# RocksDB db_bench 실행 셸 스크립트

# 경로 및 로그 파일 변수 정의
BASE_DIR="/home/leedaeeun/bench_rocksdb"  # 데이터베이스 저장 베이스 경로
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")        # 현재 날짜와 시간 (폴더 구분용)
DB_PATH="${BASE_DIR}/${TIMESTAMP}"        # 시간 기반 폴더로 설정된 DB 경로
LOG_FILE="${DB_PATH}/db_bench_result.log" # 벤치마크 결과가 저장될 파일

# 데이터베이스 경로 디렉토리 생성
mkdir -p "$DB_PATH"

# db_bench 실행
time ./db_bench \                      # RocksDB의 db_bench 명령 실행 (성능 측정 도구)
--db=$DB_PATH \                        # 데이터베이스 경로 지정
--num=$NUM \                           # 총 키-값 쌍의 수 (테스트 데이터 크기)
--value_size=$VALUE_SIZE \             # 값의 크기 (바이트 단위)
--key_size=$KEY_SIZE \                 # 키의 크기 (바이트 단위)
--reads=$READS \                       # 읽기 요청 수
--writes=$WRITES \                     # 쓰기 요청 수
--threads=$THREADS \                   # 벤치마크를 실행할 스레드 수
--benchmarks=$BENCHMARKS \             # 실행할 벤치마크의 리스트 (쉼표로 구분)
--compression_type=$COMPRESSION_TYPE | # 압축 유형 (예: snappy, zlib)
  tee "$LOG_FILE"                      # 결과를 터미널과 로그 파일에 동시에 출력

# 결과 확인 메시지
echo "벤치마크가 완료되었습니다!"
echo "데이터베이스 경로: $DB_PATH"
echo "결과 로그 파일: $LOG_FILE"
