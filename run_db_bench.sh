#!/bin/bash
# RocksDB db_bench 실행 셸 스크립트
# (옵션 선택, 최종 명령어 확인, 헤더에 실행 명령어와 벤치마크 시작/종료 시간 기록,
#  스크립트 종료 시 임시 파일 삭제)

# 경로 및 로그 파일 변수 정의
BASE_DIR="/home/leedaeeun/bench_rocksdb" # 데이터베이스 저장 베이스 경로
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")       # 현재 날짜와 시간 (폴더 구분용)

# 사용자 입력 받기 (디렉토리 이름)
echo "저장할 디렉토리 이름을 입력하세요 (빈 칸이면 timestamp만 사용):"
read USER_DIR_NAME

if [ -z "$USER_DIR_NAME" ]; then
  DB_PATH="${BASE_DIR}/${TIMESTAMP}"
  LOG_FILE="${BASE_DIR}/db_bench_result_${TIMESTAMP}.log"
else
  DB_PATH="${BASE_DIR}/${USER_DIR_NAME}_${TIMESTAMP}"
  LOG_FILE="${BASE_DIR}/db_bench_result_${USER_DIR_NAME}_${TIMESTAMP}.log"
fi

# 데이터베이스 경로 디렉토리 생성
mkdir -p "$DB_PATH"

# 현재 디렉토리 내의 모든 *.txt 파일을 번호와 함께 출력
declare -A TXT_FILES_MAP
index=1

printf "\n"
echo "현재 디렉토리의 *.txt 파일 목록:"
for file in *.txt; do
  if [ -f "$file" ]; then
    TXT_FILES_MAP[$index]="$file"
    echo "  $index : $file"
    index=$((index + 1))
  fi
done

if [ ${#TXT_FILES_MAP[@]} -eq 0 ]; then
  echo "오류: 현재 디렉토리에 텍스트 파일이 없습니다."
  exit 1
fi

# 기본 옵션 파일 번호 선택
echo "기본 옵션으로 사용할 텍스트 파일의 번호를 입력하세요:"
read BASE_CHOICE

if ! [[ "$BASE_CHOICE" =~ ^[0-9]+$ ]] || [ -z "${TXT_FILES_MAP[$BASE_CHOICE]}" ]; then
  echo "잘못된 입력입니다. 스크립트를 종료합니다."
  exit 1
fi

BASE_OPTION_FILE="${TXT_FILES_MAP[$BASE_CHOICE]}"
printf "\n"
echo "선택한 기본 옵션 파일: $BASE_OPTION_FILE"
# 선택한 파일의 내용을 읽어 옵션으로 사용
FINAL_OPTS=$(<"$BASE_OPTION_FILE")

# 실행할 db_bench 명령어 구성
CMD="time ./db_bench --db=\"$DB_PATH\" ${FINAL_OPTS}"

# 최종 명령어 출력 및 임시 파일에 저장
echo "실행될 명령어는 다음과 같습니다:"
echo "$CMD" >executed_command.sh
cat executed_command.sh
echo ""

# 실행 여부 확인 (Y, y, 또는 엔터 허용)
echo "실행할까요? (Y/N):"
read CONFIRM

if [[ -z "$CONFIRM" || "$CONFIRM" =~ ^[Yy]$ ]]; then
  rm -f executed_command.sh

  # 벤치마크 시작 시간 기록
  START_TIME=$(date)

  # 벤치마크 실행 결과 임시 파일에 저장
  OUTPUT_FILE=$(mktemp)
  eval "$CMD" | tee "$OUTPUT_FILE"

  # 벤치마크 종료 시간 기록
  END_TIME=$(date)

  # 헤더 내용 작성
  HEADER_FILE=$(mktemp)
  {
    echo "========================================"
    echo "실행된 명령어: $CMD"
    echo "벤치마크 시작: $START_TIME"
    echo "벤치마크 종료: $END_TIME"
    echo "========================================"
  } >"$HEADER_FILE"

  # 헤더와 실행 결과를 최종 로그 파일에 기록
  cat "$HEADER_FILE" "$OUTPUT_FILE" >"$LOG_FILE"

  # 임시 파일 삭제
  rm -f "$HEADER_FILE" "$OUTPUT_FILE"

  printf "\n"
  echo "벤치마크가 완료되었습니다! $(date)"
  echo "데이터베이스 경로: $DB_PATH"
  echo "결과 로그 파일: $LOG_FILE"
else
  echo "실행이 취소되었습니다."
  rm -f executed_command.sh
  exit 0
fi
