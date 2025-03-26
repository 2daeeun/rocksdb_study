f executed_command.sh
  exit 0
fi
#!/bin/bash
# RocksDB db_bench 실행 셸 스크립트
# (여러 옵션 선택, 최종 명령어 확인, 헤더에 실행 명령어와 벤치마크 시작/종료 시간 기록,
#  스크립트 종료 시 임시 파일 삭제)

# 경로 및 로그 파일 변수 정의
BASE_DIR="/home/leedaeeun/bench_rocksdb"  # 데이터베이스 저장 베이스 경로
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")            # 현재 날짜와 시간 (폴더 구분용)

# 사용자 입력 받기 (디렉토리 이름)
echo "디렉토리 이름을 입력하세요 (빈 칸이면 timestamp만 사용):"
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

# 현재 디렉토리 내의 모든 *.txt 파일 목록 검색 (기본 옵션 파일 대신)
TXT_FILES=(*.txt)
if [ ${#TXT_FILES[@]} -eq 0 ]; then
  echo "오류: 현재 디렉토리에 텍스트 파일이 없습니다."
  exit 1
fi

echo "사용 가능한 텍스트 파일 목록:"
for i in "${!TXT_FILES[@]}"; do
  echo "$((i+1)) : ${TXT_FILES[$i]}"
done

echo "실행할 텍스트 파일의 번호를 입력하세요:"
read FILE_CHOICE

# 입력값 검증: 숫자여야 하며 목록 범위 내여야 함
if ! [[ "$FILE_CHOICE" =~ ^[0-9]+$ ]] || [ "$FILE_CHOICE" -lt 1 ] || [ "$FILE_CHOICE" -gt ${#TXT_FILES[@]} ]; then
  echo "잘못된 입력입니다. 스크립트를 종료합니다."
  exit 1
fi

SELECTED_FILE="${TXT_FILES[$((FILE_CHOICE-1))]}"
echo "선택한 파일: $SELECTED_FILE"
# 선택한 파일의 내용을 읽어 옵션으로 사용
FINAL_OPTS=$(<"$SELECTED_FILE")

# 추가 커스텀 옵션 파일 선택 (예: _custom1.txt ~ _custom9.txt; 없으면 기본 옵션만 사용)
echo "추가로 사용할 커스텀 옵션 파일 번호를 입력하세요 (1~9, 예: 2 또는 12; 없으면 추가하지 않습니다):"
read CUSTOM_INPUT

if [ -n "$CUSTOM_INPUT" ]; then
  # 입력된 문자열의 각 문자(옵션 번호)를 개별적으로 처리
  for ((i = 0; i < ${#CUSTOM_INPUT}; i++)); do
    digit="${CUSTOM_INPUT:$i:1}"
    if [[ "$digit" =~ [1-9] ]]; then
      CUSTOM_FILE="_custom${digit}.txt"
      if [ -f "$CUSTOM_FILE" ]; then
        CUSTOM_OPTS=$(<"$CUSTOM_FILE")
        FINAL_OPTS="${FINAL_OPTS} ${CUSTOM_OPTS}"
        echo "${CUSTOM_FILE} 옵션을 추가하였습니다."
      else
        echo "경고: ${CUSTOM_FILE} 파일을 찾을 수 없어 건너뜁니다."
      fi
    else
      echo "경고: '$digit'은 유효한 옵션 번호(1~9)가 아닙니다. 건너뜁니다."
    fi
  done
else
  echo "커스텀 옵션을 추가하지 않습니다."
fi

# 실행할 db_bench 명령어 구성 (예시로 fillrandom 벤치마크 사용)
# CMD="time ./db_bench --db=\"$DB_PATH\" ${FINAL_OPTS}"
CMD="time ./db_bench ${FINAL_OPTS}"

# 실행 전, 최종 명령어를 임시 파일에 저장 후 cat으로 출력
echo "실행될 명령어는 다음과 같습니다:"
echo "$CMD" > executed_command.sh
cat executed_command.sh
echo ""

# 사용자에게 최종 명령어 확인 후 실행 여부 선택
echo "실행할까요? (Y/N):"
read CONFIRM

if [[ "$CONFIRM" =~ ^[Yy]$ ]]; then
  # 스크립트 종료 시 사용된 임시 파일 삭제
  rm -f executed_command.sh

  # 벤치마크 시작 시간 기록
  START_TIME=$(date)

  # 벤치마크 실행 결과를 임시 파일에 저장
  OUTPUT_FILE=$(mktemp)
  eval "$CMD" | tee "$OUTPUT_FILE"

  # 벤치마크 종료 시간 기록
  END_TIME=$(date)

  # 헤더 내용 작성 (원하는 형식)
  HEADER_FILE=$(mktemp)
  {
    echo "========================================"
    echo "실행된 명령어: $CMD"
    echo "벤치마크 시작: $START_TIME"
    echo "벤치마크 종료: $END_TIME"
    echo "========================================"
  } > "$HEADER_FILE"

  # 헤더와 실행 결과를 최종 로그 파일에 기록 (헤더가 앞부분에 위치)
  cat "$HEADER_FILE" "$OUTPUT_FILE" > "$LOG_FILE"

  # 임시 파일 삭제
  rm -f "$HEADER_FILE" "$OUTPUT_FILE"

  echo "벤치마크가 완료되었습니다! $(date)"
  echo "데이터베이스 경로: $DB_PATH"
  echo "결과 로그 파일: $LOG_FILE"
else
  echo "실행이 취소되었습니다."
  rm -
