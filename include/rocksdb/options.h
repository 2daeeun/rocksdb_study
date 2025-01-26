// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "rocksdb/advanced_options.h"
#include "rocksdb/comparator.h"
#include "rocksdb/compression_type.h"
#include "rocksdb/customizable.h"
#include "rocksdb/data_structure.h"
#include "rocksdb/env.h"
#include "rocksdb/file_checksum.h"
#include "rocksdb/listener.h"
#include "rocksdb/sst_partitioner.h"
#include "rocksdb/types.h"
#include "rocksdb/universal_compaction.h"
#include "rocksdb/version.h"
#include "rocksdb/write_buffer_manager.h"

#ifdef max
#undef max
#endif

namespace ROCKSDB_NAMESPACE {

class Cache;
class CompactionFilter;
class CompactionFilterFactory;
class Comparator;
class ConcurrentTaskLimiter;
class Env;
enum InfoLogLevel : unsigned char;
class SstFileManager;
class FilterPolicy;
class Logger;
class MergeOperator;
class Snapshot;
class MemTableRepFactory;
class RateLimiter;
class Slice;
class Statistics;
class InternalKeyComparator;
class WalFilter;
class FileSystem;

struct Options;
struct DbPath;

using FileTypeSet = SmallEnumSet<FileType, FileType::kBlobFile>;

struct ColumnFamilyOptions : public AdvancedColumnFamilyOptions {
  // 이 함수는 옵션을 이전 버전으로 복구합니다. 4.6 이상 버전만 지원됩니다.
  // 유지 관리되지 않음: 이 함수는 현재 유지 관리되지 않으며, 앞으로도 유지
  // 관리되지 않을 예정입니다. 더 이상 사용되지 않음: 이 함수는 향후 릴리스에서
  // 제거될 수 있습니다. 일반적으로 기본값은 광범위한 관심사를 반영하여
  // 변경됩니다. 업그레이드 시 변경 사항을 선택하지 않으려면 신중하고 의도적으로
  // 결정해야 합니다.

  ColumnFamilyOptions* OldDefaults(int rocksdb_major_version = 4,
                                   int rocksdb_minor_version = 6);

  // RocksDB 최적화를 더 쉽게 할 수 있는 몇 가지 함수들
  // DB 크기가 매우 작고 (예: 1GB 미만) memtable에 많은 메모리를 할당하고 싶지
  // 않다면 이 방법을 사용하세요. 선택적인 캐시 객체가 전달되어 블록 캐시로
  // 사용됩니다.

  ColumnFamilyOptions* OptimizeForSmallDb(
      std::shared_ptr<Cache>* cache = nullptr);

  // 데이터를 정렬 상태로 유지할 필요가 없으면 이 방법을 사용하세요. 즉,
  // 이터레이터를 사용하지 않고 Put()과 Get() API 호출만 사용할 경우입니다.

  ColumnFamilyOptions* OptimizeForPointLookup(uint64_t block_cache_size_mb);

  // ColumnFamilyOptions의 일부 매개변수에 대한 기본값은 무거운 작업 부하와 큰
  // 데이터셋에 최적화되어 있지 않으므로 특정 조건에서는 쓰기 지연(write
  // stalls)이 발생할 수 있습니다. RocksDB 옵션을 조정하기 위한 출발점으로, 다음
  // 두 함수를 사용하세요:
  // * OptimizeLevelStyleCompaction -- 레벨 스타일 컴팩션 최적화
  // * OptimizeUniversalStyleCompaction -- 유니버설 스타일 컴팩션 최적화
  // 유니버설 스타일 컴팩션은 큰 데이터셋에 대해 쓰기 증폭(Write Amplification)
  // 계수를 줄이는 데 중점을 두지만, 공간 증폭(Space Amplification)은
  // 증가시킵니다. 다양한 스타일에 대해 더 알고 싶다면 여기에서 확인할 수
  // 있습니다:
  // https://github.com/facebook/rocksdb/wiki/Rocksdb-Architecture-Guide
  // 또한, 가장 큰 성능 향상을 제공하는 IncreaseParallelism()도 호출해야 합니다.
  // 참고: 높은 쓰기 속도 기간 동안 memtable_memory_budget보다 더 많은 메모리를
  // 사용할 수 있습니다.

  ColumnFamilyOptions* OptimizeLevelStyleCompaction(
      uint64_t memtable_memory_budget = 512 * 1024 * 1024);
  ColumnFamilyOptions* OptimizeUniversalStyleCompaction(
      uint64_t memtable_memory_budget = 512 * 1024 * 1024);

  // -------------------
  // 동작에 영향을 미치는 매개변수들

  // 테이블에서 키의 순서를 정의하는 데 사용되는 비교기(Comparator).
  // 기본값: 어휘 순서(byte-wise lexicographic ordering)를 사용하는 비교기
  //
  // 요구 사항: 클라이언트는 여기에서 제공된 비교기가 동일한 이름을 사용하고,
  // 이전 DB 열기 호출에서 제공된 비교기와 *정확히* 동일한 방식으로 키를
  // 정렬하는지 확인해야 합니다.
  const Comparator* comparator = BytewiseComparator();

  // 요구 사항: Merge 작업에 접근해야 할 경우 클라이언트는 머지 연산자(merge
  // operator)를 제공해야 합니다. 머지 연산자 없이 DB에서 Merge를 호출하면
  // Status::NotSupported가 반환됩니다. 클라이언트는 여기에서 제공된 머지
  // 연산자가 동일한 이름을 사용하고, 이전 DB 열기 호출에서 제공된 머지 연산자와
  // *정확히* 동일한 의미론적 동작을 수행하는지 확인해야 합니다. 유일한 예외는
  // 업그레이드 상황으로, 이전에 머지 연산자가 없었던 DB에 처음으로 Merge 작업이
  // 도입될 때입니다. 이 경우 DB를 열 때 머지 연산자를 지정하는 것이 필요합니다.
  // 기본값: nullptr
  std::shared_ptr<MergeOperator> merge_operator = nullptr;

  // 단일 CompactionFilter 인스턴스를 컴팩션 중에 호출합니다.
  // 백그라운드 컴팩션 중에 키-값을 수정하거나 삭제할 수 있도록 애플리케이션에
  // 허용합니다.
  //
  // 클라이언트가 서로 다른 컴팩션 실행에 대해 새로운 `CompactionFilter`를
  // 사용하거나 컴팩션 외부에서 테이블 파일 생성을 위해 `CompactionFilter`가
  // 필요하면, 이 옵션 대신 compaction_filter_factory를 지정할 수 있습니다. 두
  // 가지 중 하나만 지정해야 합니다. compaction_filter와
  // compaction_filter_factory가 모두 지정되면 compaction_filter가 우선됩니다.
  //
  // 멀티스레드 컴팩션을 사용하는 경우, 제공된 CompactionFilter 인스턴스는 서로
  // 다른 스레드에서 동시에 사용될 수 있으므로 스레드 안전해야 합니다.
  //
  // 기본값: nullptr
  const CompactionFilter* compaction_filter = nullptr;

  // 이것은 `CompactionFilter` 객체를 제공하는 팩토리로, 애플리케이션이 테이블
  // 파일 생성 중에 키-값을 수정하거나 삭제할 수 있게 합니다.
  //
  // `compaction_filter` 옵션과 달리, 이는 컴팩션이 테이블 파일을 생성할 때
  // 사용되며, 테이블 파일이 여러 이유로 생성될 때 `CompactionFilter`를 사용할
  // 수 있게 합니다. 이 팩토리는 어떤 `TableFileCreationReason`이
  // `CompactionFilter`를 사용할지를 결정할 수 있습니다. 호환성을 위해
  // 기본적으로 이 결정은 `TableFileCreationReason::kCompaction`에 대해서만
  // `CompactionFilter`를 사용하도록 설정됩니다.
  //
  // 테이블 파일을 생성하는 작업을 담당하는 각 스레드는 위의
  // `TableFileCreationReason`에 기반한 결정에 따라 `CompactionFilter`를 새로
  // 생성합니다. 이 방식은 애플리케이션이 각 작업 스레드에 대해 알 수 있게
  // 해주며, `CompactionFilter`가 스레드 안전성을 제공할 필요가 없도록 합니다.
  //
  // 기본값: nullptr
  std::shared_ptr<CompactionFilterFactory> compaction_filter_factory = nullptr;

  // -------------------
  // 성능에 영향을 미치는 매개변수들

  // 정렬된 디스크 파일로 변환되기 전에 메모리에서 축적되는 데이터 양 (디스크에
  // 정렬되지 않은 로그로 백업됨).
  //
  // 더 큰 값은 성능을 증가시킵니다, 특히 대량 로드 중에 그렇습니다.
  // max_write_buffer_number에 지정된 만큼의 쓰기 버퍼가 동시에 메모리에 보유될
  // 수 있습니다, 따라서 이 매개변수를 조정하여 메모리 사용을 제어할 수
  // 있습니다. 또한, 더 큰 쓰기 버퍼는 데이터베이스를 다음에 열 때 복구 시간을
  // 길게 만듭니다.
  //
  // write_buffer_size는 컬럼 패밀리별로 적용됩니다.
  // 컬럼 패밀리 간 메모리 공유를 위해 db_write_buffer_size를 참조하세요.
  //
  // 기본값: 64MB
  //
  // SetOptions() API를 통해 동적으로 변경 가능
  size_t write_buffer_size = 64 << 20;

  // 지정된 압축 알고리즘을 사용하여 블록을 압축합니다.
  //
  // 기본값: kSnappyCompression, 지원되는 경우. Snappy가 라이브러리와 연결되지
  // 않은 경우, 기본값은 kNoCompression입니다.
  //
  // Intel(R) Core(TM)2 2.4GHz에서의 kSnappyCompression의 일반적인 속도:
  //    ~200-500MB/s 압축
  //    ~400-800MB/s 압축 해제
  //
  // 이 속도는 대부분의 지속적인 저장 장치 속도보다 훨씬 빠르므로
  // 일반적으로 kNoCompression으로 전환할 필요는 없습니다.
  // 입력 데이터가 압축할 수 없는 경우에도, kSnappyCompression 구현은 이를
  // 효율적으로 감지하고 압축되지 않은 모드로 전환합니다.
  //
  // `compression_opts.level`을 설정하지 않거나
  // `CompressionOptions::kDefaultCompressionLevel`로 설정하면, 우리는 아래와
  // 같이 `compression`에 해당하는 기본값을 선택하려고 시도합니다:
  //
  // - kZSTD: 3
  // - kZlibCompression: Z_DEFAULT_COMPRESSION (현재 -1)
  // - kLZ4HCCompression: 0
  // - kLZ4: -1 (즉, `acceleration=1`; `CompressionOptions::level` 문서 참조)
  // - 나머지 모든 경우, 압축 수준을 지정하지 않습니다.
  //
  // SetOptions() API를 통해 동적으로 변경 가능
  CompressionType compression;

  // 파일을 포함하는 가장 하위 레벨에 대해 사용될 압축 알고리즘.
  // num_levels = 1에 대한 동작은 명확하게 정의되지 않았습니다.
  // 현재 num_levels = 1일 경우, 모든 컴팩션 출력은 bottommost_compression을
  // 사용하고 모든 플러시 출력은 여전히 options.compression을 사용하지만, 이
  // 동작은 변경될 수 있습니다.
  //
  // 기본값: kDisableCompressionOption (비활성화)
  CompressionType bottommost_compression = kDisableCompressionOption;

  // bottommost_compression에서 사용되는 압축 알고리즘에 대한 다양한 옵션들.
  // 이를 활성화하려면 CompressionOptions의 정의를 참조하세요.
  // num_levels = 1에 대한 동작은 options.bottommost_compression과 동일합니다.
  CompressionOptions bottommost_compression_opts;

  // 압축 알고리즘에 대한 다양한 옵션들
  CompressionOptions compression_opts;

  // level-0 컴팩션을 트리거할 파일 수. 값이 <0이면
  // level-0 컴팩션은 파일 수에 의해 전혀 트리거되지 않습니다.
  //
  // 유니버설 컴팩션: RocksDB는 정렬된 실행(run)의 수가 이 숫자를 초과하지
  // 않도록 하려고 시도합니다.
  //   CompactionOptionsUniversal::max_read_amp가 설정되면, 이 옵션은 컴팩션을
  //   찾기 위한 트리거로만 사용됩니다.
  //   CompactionOptionsUniversal::max_read_amp는 정렬된 실행의 수에 대한 제한이
  //   됩니다.
  //
  // 기본값: 4
  //
  // SetOptions() API를 통해 동적으로 변경 가능
  int level0_file_num_compaction_trigger = 4;

  // nullptr이 아닌 경우, 지정된 함수를 사용하여 키를 "접두어(prefix)"라고
  // 불리는 연속적인 그룹에 배치합니다. 이 접두어들은 각 키의 항목 대신 그룹의
  // 대표 항목 하나를 Bloom 필터에 넣는 데 사용됩니다 (전체 키 필터링을 참조).
  // 특정 조건 하에서, 이는 일부 범위 쿼리(이터레이터)와 일부 포인트
  // 조회(Get/MultiGet)를 최적화할 수 있게 해줍니다.
  //
  // `prefix_extractor`와 `comparator`는 범위 쿼리에 대해 유효한 접두어 필터링을
  // 위해 반드시 다음과 같은 중요한 속성을 만족해야 합니다:
  //   Compare(k1, k2) <= 0이고 Compare(k2, k3) <= 0이며
  //      InDomain(k1)이고 InDomain(k3)이고 prefix(k1) == prefix(k3)일 때,
  //   그러면 InDomain(k2)이고 prefix(k2) == prefix(k1)
  //
  // 다시 말해, 동일한 접두어를 가진 모든 키는 비교기 순서에 따라 연속적인
  // 그룹에 있어야 하며, 접두어가 없는 키("도메인 밖의 키")에 의해 방해받지
  // 않아야 합니다. (이 속성 덕분에, 상한 및 하한이 공통의 접두어를 가지며, 그
  // 접두어를 가진 항목이 없으면 해당 범위 내에 항목이 없다고 결론지을 수
  // 있습니다.)
  //
  // 몇 가지 다른 속성들도 추천되지만 필수적인 것은 아닙니다. 대부분의 합리적인
  // 비교기 하에서, 위의 중요한 속성을 만족하려면 다음 조건이 충족되어야 합니다:
  // * "접두어는 접두어이다": key.starts_with(prefix(key))
  // * "접두어는 순서를 유지한다": Compare(k1, k2) <= 0이면,
  //   Compare(prefix(k1), prefix(k2)) <= 0이어야 한다
  //
  // 다음 두 속성은 접두어로 검색할 때 해당 접두어를 가진 모든 항목을 열거할 수
  // 있도록 보장합니다:
  // * "접두어는 그룹을 시작한다": Compare(prefix(key), key) <= 0
  // * "접두어는 항등적이다": prefix(prefix(key)) == prefix(key)
  //
  // 기본값: nullptr
  std::shared_ptr<const SliceTransform> prefix_extractor = nullptr;

  // 레벨에 대한 최대 총 데이터 크기를 제어합니다.
  // max_bytes_for_level_base는 level-1의 최대 총 크기입니다.
  // 레벨 L에 대한 최대 바이트 수는 다음과 같이 계산할 수 있습니다:
  // (max_bytes_for_level_base) * (max_bytes_for_level_multiplier ^ (L-1))
  // 예를 들어, max_bytes_for_level_base가 200MB이고,
  // max_bytes_for_level_multiplier가 10이면, level-1의 총 데이터 크기는 200MB,
  // level-2의 총 파일 크기는 2GB, level-3의 총 파일 크기는 20GB가 됩니다.
  //
  // 기본값: 256MB.
  //
  // SetOptions() API를 통해 동적으로 변경 가능
  uint64_t max_bytes_for_level_base = 256 * 1048576;

  // 더 이상 사용되지 않음.
  uint64_t snap_refresh_nanos = 0;

  // 자동 컴팩션을 비활성화합니다. 이 컬럼 패밀리에서는 여전히 수동 컴팩션을
  // 실행할 수 있습니다.
  //
  // SetOptions() API를 통해 동적으로 변경 가능
  bool disable_auto_compactions = false;

  // 이것은 TableFactory 객체를 제공하는 팩토리입니다.
  // 기본값: 기본적인 BlockBasedTableOptions를 사용하여 TableBuilder와
  // TableReader의 기본 구현을 제공하는 블록 기반 테이블 팩토리입니다.
  std::shared_ptr<TableFactory> table_factory;

  // 이 컬럼 패밀리의 SST 파일을 넣을 수 있는 경로 목록과 해당 경로의 대상
  // 크기입니다. db_paths와 유사하게, 새로운 데이터는 벡터의 앞부분에 지정된
  // 경로에 배치되며, 오래된 데이터는 점차 벡터 뒤쪽에 지정된 경로로 이동합니다.
  // 참고로, 만약 경로가 여러 컬럼 패밀리에 제공되면, 해당 경로에는 모든 컬럼
  // 패밀리의 파일과 총 크기가 합쳐져 있게 됩니다. 이런 경우에는 사용자 측에서
  // 모든 컬럼 패밀리의 총 크기를 고려하여 용량을 준비해야 합니다.
  //
  // 비어 있을 경우, db_paths가 사용됩니다.
  // 기본값: 비어 있음
  std::vector<DbPath> cf_paths;

  // 컬럼 패밀리의 컴팩션 동시 스레드 제한기.
  // nullptr이 아닌 경우, 주어진 동시 스레드 제한기를 사용하여 최대 동시 컴팩션
  // 작업을 제어합니다. 제한기는 여러 컬럼 패밀리에서 db 인스턴스 간에 공유할 수
  // 있습니다.
  //
  // 기본값: nullptr
  std::shared_ptr<ConcurrentTaskLimiter> compaction_thread_limiter = nullptr;

  // nullptr이 아닌 경우, 지정된 팩토리를 사용하여 sst 파일의 파티셔닝을
  // 결정하는 함수를 제공합니다. 이는 컴팩션이 흥미로운 경계(키 접두어)에서
  // 파일을 분할하여 SST 파일의 전파가 더 적은 쓰기 증폭을 일으키도록 돕습니다
  // (전체 키 공간을 덮지 않도록).
  // 이 기능은 아직 실험적입니다.
  //
  // 기본값: nullptr
  std::shared_ptr<SstPartitionerFactory> sst_partitioner_factory = nullptr;

  // RocksDB는 범위 삭제 수가 이 한계값 이상일 때 현재 memtable을 플러시하려고
  // 시도합니다. 많은 범위 삭제가 있는 작업 부하에서는, memtable에서 범위 삭제의
  // 수를 제한하는 것이 성능 저하 및/또는 하나의 memtable에 너무 많은 범위
  // tombstone이 포함되어 발생할 수 있는 OOM을 방지하는 데 도움이 될 수
  // 있습니다.
  //
  // 기본값: 0 (비활성화)
  //
  // SetOptions() API를 통해 동적으로 변경 가능
  uint32_t memtable_max_range_deletions = 0;

  // 실험적(EXPERIMENTAL)
  // 값이 0보다 크면, RocksDB는 삭제될 예정인 파일에 대해 일부 블록 캐시 항목을
  // 지우려고 시도합니다. 과도한 추적을 피하기 위해, 이 "언캐싱" 프로세스는
  // 반복적이고 추측적이며, 파일의 블록이 일반적으로 캐시되지 않는 경우
  // 백그라운드에서 추가적인 CPU 작업이 발생할 수 있습니다. 더 큰 숫자는 알려진
  // 구식 항목을 지워 블록 캐시 적중률을 최대화하기 위해 CPU 시간을 더 많이
  // 할애하려는 의지를 나타냅니다.
  //
  // uncache_aggressiveness=1일 때, 구식 파일의 블록 캐시 항목은
  // 블록이 캐시되지 않아서 삭제 시도가 실패할 때까지만 지워집니다.
  // 그 후, 해당 파일에 대해 캐시된 블록을 삭제하려는 추가적인 시도는 하지
  // 않습니다.
  //
  // 더 큰 값일 경우, 삭제 시도는 성공 가능성이 < 0.99^(a-1)로 나타날 때까지
  // 계속 시도됩니다. 여기서 a는 uncache_aggressiveness입니다. 예를 들어: 2 ->
  // 99% 이상의 성공적인/유용한 삭제를 기대하며 시도 11 -> 90% 69 -> 50% 110 ->
  // 33% 230 -> 10% 460 -> 1% 690 -> 0.1% 1000 -> 1 in 23000 10000 -> 항상
  // (실용적인 측면에서) 주의: UINT32_MAX와 그 근처 값은 미래에 추가적인 특별한
  // 의미를 가질 수 있습니다.
  //
  // 고정된 캐시 항목(항상 존재하는 항목)은 uncache_aggressiveness > 0일 경우
  // 항상 삭제되지만, 비고정 항목의 삭제 성공 확률을 예측하는 데는 사용되지
  // 않습니다.
  //
  // 주의: 체크포인트와 같은 복사된 DB들이 블록 캐시를 공유하는 경우,
  // 파일이 구식이 되더라도 해당 파일의 블록 캐시 항목(복사본들 간에 공유됨)이
  // 구식이 아닐 수 있습니다. 이런 시나리오는 uncache_aggressiveness = 0일 때
  // 가장 적합합니다.
  //
  // allow_mmap_reads=true일 경우, 이 옵션은 무시됩니다(언캐싱 없음).
  //
  // 생산 환경에서 검증되면 기본값은 300 정도로 변경될 가능성이 큽니다.
  uint32_t uncache_aggressiveness = 0;

  // 모든 필드에 대한 기본값으로 ColumnFamilyOptions를 생성합니다.
  ColumnFamilyOptions();
  // Options로부터 ColumnFamilyOptions를 생성합니다.
  explicit ColumnFamilyOptions(const Options& options);

  void Dump(Logger* log) const;
};

enum class WALRecoveryMode : char {
  // 원래의 levelDB 복구
  //
  // 로그의 마지막 레코드가 쓰기 중에 충돌로 인해 불완전한 경우를 허용합니다.
  // 또한, 로그의 후속 데이터에서 미리 할당된 0 바이트도 허용됩니다.
  //
  // 사용 사례: 업데이트가 적용된 후, 충돌 복구 후에도 롤백되지 않아야 하는
  // 애플리케이션.
  // 이 복구 모드에서는 `WritableFile::Append()` 쓰기가 내구성을 보장하는 한,
  // RocksDB가 이를 보장합니다.
  // 사용자가 더 많은 상황에서 이 보장을 원할 경우(예:
  // `WritableFile::Append()`가 페이지 캐시로 쓰지만,
  // 전원 손실 복구 시에도 이 보장을 원할 경우), RocksDB는 추가적으로
  // `WritableFile::Sync()`를 호출하여
  // 보장을 강화할 수 있는 다양한 메커니즘을 제공합니다.
  //
  // 이 모드는 `kPointInTimeRecovery`와 다릅니다. 복구 중에 손상이 감지되면, 이
  // 모드는 DB를 열지 않으려고 합니다.
  // 반면, `kPointInTimeRecovery`는 손상 직전에 복구를 중지하는데, 이는 복구할
  // 수 있는 유효한 시점입니다.
  kTolerateCorruptedTailRecords = 0x00,

  // 깨끗한 종료에서 복구
  // WAL에서 어떤 손상도 발견되지 않을 것으로 예상합니다.
  // 사용 사례: 유닛 테스트와 높은 일관성 보장이 필요한 드문 애플리케이션에
  // 이상적입니다.
  kAbsoluteConsistency = 0x01,

  // 시점 일관성 복구 (기본값)
  // WAL 불일치가 발견되면 WAL 재생을 중지합니다.
  // 사용 사례: 하드 디스크, SSD와 같은 디스크 컨트롤러 캐시가 있는 시스템에
  // 이상적입니다.
  // 슈퍼 커패시터 없이 관련 데이터를 저장하는 시스템에 적합합니다.
  kPointInTimeRecovery = 0x02,

  // 재난 후 복구
  // WAL에서 어떤 손상도 무시하고 가능한 한 많은 데이터를 복구하려고 시도합니다.
  // 사용 사례: 데이터를 복구하려는 마지막 시도에 이상적이거나,
  // 낮은 품질의 관련 없는 데이터를 처리하는 시스템에 적합합니다.
  kSkipAnyCorruptedRecords = 0x03,
};

struct DbPath {
  std::string path;
  uint64_t target_size;  // Target size of total files under the path, in byte.

  DbPath() : target_size(0) {}
  DbPath(const std::string& p, uint64_t t) : path(p), target_size(t) {}
};

extern const char* kHostnameForDbHostId;

enum class CompactionServiceJobStatus : char {
  kSuccess,
  kFailure,
  kUseLocal,
};

struct CompactionServiceJobInfo {
  std::string db_name;
  std::string db_id;
  std::string db_session_id;
  uint64_t job_id;  // job_id는 현재 DB와 세션 내에서만 고유합니다.
                    // DB를 다시 시작하면 job_id가 리셋됩니다. `db_id`와
                    // `db_session_id`는 서로 다른 DB와 세션에서 고유한 ID를
                    // 생성하는 데 도움이 될 수 있습니다.

  Env::Priority priority;

  // 컴팩션 서비스에서 유용할 수 있는 추가 컴팩션 세부 사항
  CompactionReason compaction_reason;
  bool is_full_compaction;
  bool is_manual_compaction;
  bool bottommost_level;

  CompactionServiceJobInfo(std::string db_name_, std::string db_id_,
                           std::string db_session_id_, uint64_t job_id_,
                           Env::Priority priority_,
                           CompactionReason compaction_reason_,
                           bool is_full_compaction_, bool is_manual_compaction_,
                           bool bottommost_level_)
      : db_name(std::move(db_name_)),
        db_id(std::move(db_id_)),
        db_session_id(std::move(db_session_id_)),
        job_id(job_id_),
        priority(priority_),
        compaction_reason(compaction_reason_),
        is_full_compaction(is_full_compaction_),
        is_manual_compaction(is_manual_compaction_),
        bottommost_level(bottommost_level_) {}
};

struct CompactionServiceScheduleResponse {
  std::string scheduled_job_id;  // primary 호스트 외부에서 생성된 job_id, 서로
                                 // 다른 DB와 세션 간에 고유 DB가 재시작되면
                                 // job_id가 리셋됩니다.
  CompactionServiceJobStatus status;
  CompactionServiceScheduleResponse(std::string scheduled_job_id_,
                                    CompactionServiceJobStatus status_)
      : scheduled_job_id(scheduled_job_id_), status(status_) {}
  explicit CompactionServiceScheduleResponse(CompactionServiceJobStatus status_)
      : status(status_) {}
};

// 예외는 RocksDB로 전달되지 않도록 해야 합니다,
// RocksDB는 예외 안전성이 보장되지 않기 때문입니다.
// 이로 인해 정의되지 않은 동작이 발생할 수 있으며,
// 데이터 손실, 보고되지 않은 손상, 교착 상태 등 여러 가지 문제가 발생할 수
// 있습니다.
class CompactionService : public Customizable {
 public:
  static const char* Type() { return "CompactionService"; }

  // 이 컴팩션 서비스의 이름을 반환합니다.
  const char* Name() const override = 0;

  // 원격에서 처리될 컴팩션을 예약합니다.
  virtual CompactionServiceScheduleResponse Schedule(
      const CompactionServiceJobInfo& /*info*/,
      const std::string& /*compaction_service_input*/) {
    CompactionServiceScheduleResponse response(
        CompactionServiceJobStatus::kUseLocal);
    return response;
  }

  // 원격 작업자가 예약된 컴팩션이 완료될 때까지 대기합니다.
  virtual CompactionServiceJobStatus Wait(
      const std::string& /*scheduled_job_id*/, std::string* /*result*/) {
    return CompactionServiceJobStatus::kUseLocal;
  }

  // 설치 시 선택적으로 호출되는 콜백 함수입니다.
  virtual void OnInstallation(const std::string& /*scheduled_job_id*/,
                              CompactionServiceJobStatus /*status*/) {}

  // 더 이상 사용되지 않음. 원격 컴팩션을 처리하려면 Schedule()과 Wait() API를
  // 구현하십시오.

  // `compaction_service_input`을 사용하여 원격에서 컴팩션을 시작합니다.
  // 이 입력은 원격 측에서 `DB::OpenAndCompact()`에 전달될 수 있습니다.
  // `info`는 사용자가 알고 싶어할 정보를 제공합니다. 여기에는 `job_id`가
  // 포함됩니다.
  virtual CompactionServiceJobStatus StartV2(
      const CompactionServiceJobInfo& /*info*/,
      const std::string& /*compaction_service_input*/) {
    return CompactionServiceJobStatus::kUseLocal;
  }

  // 원격 컴팩션이 완료될 때까지 대기합니다.
  virtual CompactionServiceJobStatus WaitForCompleteV2(
      const CompactionServiceJobInfo& /*info*/,
      std::string* /*compaction_service_result*/) {
    return CompactionServiceJobStatus::kUseLocal;
  }

  ~CompactionService() override = default;
};

struct DBOptions {
  // 이 함수는 옵션을 버전 4.6의 옵션으로 복구합니다.
  // 유지 관리되지 않음: 이 함수는 유지 관리되지 않으며 앞으로도 유지 관리되지
  // 않을 예정입니다. 더 이상 사용되지 않음: 이 함수는 향후 릴리스에서 제거될 수
  // 있습니다. 일반적으로 기본값은 광범위한 관심사를 반영하여 변경됩니다.
  // 업그레이드 시 변경 사항을 선택하지 않으려면 신중하고 의도적으로 결정해야
  // 합니다.
  DBOptions* OldDefaults(int rocksdb_major_version = 4,
                         int rocksdb_minor_version = 6);

  // RocksDB 최적화를 더 쉽게 할 수 있는 몇 가지 함수들

  // DB 크기가 매우 작고 (예: 1GB 미만) memtable에 많은 메모리를 할당하고 싶지
  // 않다면 이 방법을 사용하세요. 선택적인 캐시 객체가 전달되어 memtable의
  // 메모리 비용에 사용됩니다.
  DBOptions* OptimizeForSmallDb(std::shared_ptr<Cache>* cache = nullptr);

  // 기본적으로 RocksDB는 flush와 컴팩션에 하나의 백그라운드 스레드만
  // 사용합니다. 이 함수를 호출하면 `total_threads`의 총 스레드를 사용하도록
  // 설정됩니다. `total_threads`에 대한 좋은 값은 코어 수입니다. 시스템이
  // RocksDB에 의해 병목 현상이 발생하는 경우 이 함수를 호출하는 것이 거의
  // 확실히 좋습니다.
  DBOptions* IncreaseParallelism(int total_threads = 16);

  // true인 경우, 데이터베이스가 없으면 새로 생성됩니다.
  // 기본값: false
  bool create_if_missing = false;

  // true인 경우, DB::Open()에서 누락된 컬럼 패밀리가 자동으로 생성됩니다.
  // 기본값: false
  bool create_missing_column_families = false;

  // true인 경우, 데이터베이스가 이미 존재하면 오류가 발생합니다.
  // 기본값: false
  bool error_if_exists = false;

  // true인 경우, RocksDB는 데이터의 일관성을 적극적으로 검사합니다.
  // 또한, 데이터베이스에 대한 쓰기 작업(Put, Delete, Merge, Write) 중 하나라도
  // 실패하면, 데이터베이스는 읽기 전용 모드로 전환되고 다른 모든 쓰기 작업은
  // 실패합니다. 대부분의 경우 이 값을 true로 설정하는 것이 좋습니다. 기본값:
  // true
  bool paranoid_checks = true;

  // 더 이상 사용되지 않음: 이 옵션은 향후 릴리스에서 제거될 수 있습니다.
  //
  // true인 경우, memtable 플러시 중에 RocksDB는 플러시에서 읽은 총 항목을
  // 검증하고, 이를 플러시에 삽입된 카운터와 비교합니다.
  //
  // 이 옵션은 새로운 검증 기능에 버그가 있는 경우 이를 끄기 위해 제공됩니다.
  // 이 기능이 안정되면 이 옵션은 향후 제거될 수 있습니다.
  //
  // 기본값: true
  bool flush_verify_memtable_count = true;

  // 더 이상 사용되지 않음: 이 옵션은 향후 릴리스에서 제거될 수 있습니다.
  //
  // true인 경우, 컴팩션 중에 RocksDB는 읽은 항목의 수를 세고 이를 컴팩션 입력
  // 파일의 항목 수와 비교합니다. 이는 컴팩션 중에 손상으로부터 보호를
  // 추가하려는 목적입니다. 참고 사항:
  // - compaction 필터가 kRemoveAndSkipUntil을 반환하는 컴팩션에서는 이 검증이
  // 수행되지 않으며,
  // - 범위 삭제의 수는 검증되지 않습니다.
  //
  // 이 옵션은 새로운 검증 기능에 버그가 있는 경우 이를 끄기 위해 제공됩니다.
  // 이 기능이 안정되면 이 옵션은 향후 제거될 수 있습니다.
  //
  // 기본값: true
  bool compaction_verify_record_count = true;

  // true인 경우, 동기화된 WAL의 로그 번호와 크기가 MANIFEST에 추적됩니다.
  // DB 복구 중에 동기화된 WAL이 디스크에서 누락되었거나, WAL의 크기가
  // MANIFEST에 기록된 크기와 일치하지 않으면 오류가 보고되고 복구가 중단됩니다.
  //
  // 이는 WAL 손상에 대한 추가적인 보호 장치로, per-WAL-entry 체크섬 외에
  // 제공됩니다.
  //
  // 이 옵션은 보조 인스턴스와 함께 작동하지 않습니다.
  // 현재는 닫힌 WAL만 동기화 추적됩니다. `DB::SyncWAL()`을 호출하거나,
  // 성능/효율성 이유로 라이브 WAL을 동기화하는 `WriteOptions::sync=true`로 쓰는
  // 것은 추적되지 않습니다.
  //
  // 기본값: false
  bool track_and_verify_wals_in_manifest = false;

  // true인 경우, 매번 SST 파일을 열 때 MANIFEST와 실제 파일 간의 SST 고유 ID를
  // 검증합니다. 이 검사는 SST 파일이 덮어쓰이거나 잘못 배치되지 않도록
  // 보장합니다. 불일치가 감지되면 손상 오류가 보고되며, 이는 RocksDB
  // 버전 7.3부터 MANIFEST에서 고유 ID를 추적하는 경우에만 발생합니다. 추적되는
  // 내부 고유 ID는 `GetUniqueIdFromTableProperties`에서 반환된 것과 관련이
  // 있지만, 이는 변경될 수 있습니다. 참고: 검증은 현재 블록 기반 테이블 형식을
  // 사용하는 SST 파일에서만 수행됩니다.
  //
  // false로 설정하는 것은 예기치 않은 문제 발생 시에만 필요합니다.
  //
  // 이 옵션의 초기 버전은 DB::Open에서 모든 SST 파일을 검증하려고 했지만,
  // 이제는 보장되지 않습니다. 그러나 위 옵션에 문서화된 대로 `max_open_files`가
  // -1이면 DB는 DB::Open에서 모든 파일을 엽니다.
  //
  // 기본값: true
  bool verify_sst_unique_id_in_manifest = true;

  // 지정된 객체를 사용하여 환경과 상호작용합니다.
  // 예: 파일을 읽거나 쓸 때, 백그라운드 작업을 예약할 때 등.
  // 기본값: Env::Default()
  Env* env = Env::Default();

  // 내부 파일 읽기/쓰기 대역폭을 제한합니다:
  //
  // - 플러시 요청은 `Env::IOPriority::IO_HIGH`에서 쓰기 대역폭을 사용합니다.
  // - 컴팩션 요청은 `Env::IOPriority::IO_LOW`에서 읽기 및 쓰기 대역폭을
  // 사용합니다.
  // - `ReadOptions`와 관련된 읽기는 `ReadOptions::rate_limiter_priority`에서
  // 요금이 부과될 수 있습니다.
  //   (사용법과 제한 사항은 해당 옵션의 API 문서를 참조하세요).
  // - `WriteOptions`와 관련된 쓰기는 `WriteOptions::rate_limiter_priority`에서
  // 요금이 부과될 수 있습니다.
  //   (사용법과 제한 사항은 해당 옵션의 API 문서를 참조하세요).
  //
  // 속도 제한기가 비활성화되면 nullptr로 설정됩니다. 속도 제한기가 활성화되면,
  // bytes_per_sync는 기본값으로 1MB로 설정됩니다.
  //
  // 기본값: nullptr
  std::shared_ptr<RateLimiter> rate_limiter = nullptr;

  // SST 파일을 추적하고 해당 파일 삭제 속도를 제어하는 데 사용됩니다.
  //
  // 기능:
  //  - SST 파일의 삭제 속도를 제한합니다.
  //  - 모든 SST 파일의 총 크기를 추적합니다.
  //  - SST 파일에 대한 최대 허용 공간 제한을 설정하여, 이 한도에 도달하면
  //    DB는 더 이상 플러시나 컴팩션을 수행하지 않으며, 백그라운드 오류를
  //    설정합니다.
  //  - 여러 DB 간에 공유할 수 있습니다.
  // 제한 사항:
  //  - 첫 번째 db_path에서만 SST 파일 삭제를 추적하고 제한합니다.
  //    (db_paths가 비어 있으면 db_name이 사용됩니다).
  //
  // 기본값: nullptr
  std::shared_ptr<SstFileManager> sst_file_manager = nullptr;

  // DB에서 생성된 모든 내부 진행/오류 정보는 info_log가 nullptr이 아닐 경우
  // info_log에 기록되며, info_log가 nullptr이면 DB 내용이 저장된 동일한
  // 디렉토리에 파일로 저장됩니다. 기본값: nullptr
  std::shared_ptr<Logger> info_log = nullptr;

  // info_log에 로그 메시지를 보내는 최소 레벨입니다. 기본값은
  // RocksDB가 릴리스 모드로 컴파일될 때 INFO_LEVEL이며, 디버그 모드로 컴파일될
  // 때는 DEBUG_LEVEL입니다.
  InfoLogLevel info_log_level = Logger::kDefaultLogLevel;

  // DB에서 사용할 수 있는 열린 파일의 수입니다. 데이터베이스에 큰 작업 세트가
  // 있으면 이 값을 늘려야 할 수 있습니다. 값 -1은 열린 파일이 항상 열린 상태로
  // 유지됨을 의미합니다. level 기반 컴팩션의 경우 target_file_size_base와
  // target_file_size_multiplier를 기반으로 파일 수를 추정할 수 있습니다.
  // 유니버설 스타일 컴팩션의 경우 보통 -1로 설정할 수 있습니다.
  //
  // 이 옵션의 높은 값이나 -1은 높은 메모리 사용을 초래할 수 있습니다.
  // 블록 기반 테이블 형식의 경우 메모리 사용을 제어하려면
  // BlockBasedTableOptions::cache_usage_options를 참조하세요.
  //
  // 기본값: -1
  //
  // SetDBOptions() API를 통해 동적으로 변경 가능
  int max_open_files = -1;

  // max_open_files가 -1인 경우, DB는 DB::Open()에서 모든 파일을 엽니다. 이
  // 옵션을 사용하여 파일을 여는 데 사용되는 스레드 수를 늘릴 수 있습니다.
  // 기본값: 16
  int max_file_opening_threads = 16;

  // 쓰기 앞 기록(WAL)의 크기가 이 크기를 초과하면, 가장 오래된 활성 WAL 파일에
  // 의해 백업된 memtable을 가진 컬럼 패밀리를 강제로 플러시합니다. (즉, 공간
  // 증폭을 일으키는 컬럼 패밀리들입니다). 0으로 설정하면 (기본값), WAL 크기
  // 제한은 [모든 write_buffer_size * max_write_buffer_number의 합] * 4로
  // 동적으로 선택됩니다.
  //
  // 예를 들어, 15개의 컬럼 패밀리가 있고 각각에 대해
  // write_buffer_size = 128 MB
  // max_write_buffer_number = 6
  // max_total_wal_size는 [15 * 128MB * 6] * 4 = 45GB로 계산됩니다.
  //
  // RocksDB 위키에는 WAL이 memtable 및 컬럼 패밀리의 플러시와 어떻게
  // 상호작용하는지에 대한 논의가 있습니다.
  // https://github.com/facebook/rocksdb/wiki/Column-Families
  //
  // 이 옵션은 컬럼 패밀리가 1개 이상일 때만 적용됩니다. 그렇지 않으면 wal
  // 크기는 write_buffer_size에 의해 결정됩니다.
  //
  // 기본값: 0
  //
  // SetDBOptions() API를 통해 동적으로 변경 가능
  uint64_t max_total_wal_size = 0;

  // nullptr이 아닌 경우, 데이터베이스 작업에 대한 메트릭을 수집해야 합니다.
  std::shared_ptr<Statistics> statistics = nullptr;

  // 기본적으로 안정적인 저장소에 대한 쓰기는 fdatasync를 사용합니다 (이 함수가
  // 사용 가능한 플랫폼에서). 이 옵션이 true인 경우, 대신 fsync가 사용됩니다.
  //
  // fsync와 fdatasync는 우리의 용도에 대해 동일하게 안전하며, fdatasync가 더
  // 빠르므로 이 옵션을 설정할 필요는 드뭅니다. 이 옵션은 커널/파일 시스템
  // 버그에 대한 우회 방법으로 제공됩니다. 예를 들어, 3.7 이전 버전의 커널에서
  // ext4와 관련된 fdatasync의 문제가 있었습니다.
  bool use_fsync = false;

  // SST 파일을 넣을 수 있는 경로 목록과 해당 경로의 대상 크기입니다.
  // 최신 데이터는 벡터의 앞부분에 지정된 경로에 배치되며, 오래된 데이터는 점차
  // 벡터의 뒷부분에 지정된 경로로 이동합니다.
  //
  // 예를 들어, 10GB의 공간을 할당한 플래시 장치와 2TB의 하드 드라이브가 있을
  // 경우, 다음과 같이 설정해야 합니다:
  //   [{"/flash_path", 10GB}, {"/hard_drive", 2TB}]
  //
  // 시스템은 각 경로 아래의 데이터가 목표 크기에 가깝지만 그보다 크지 않도록
  // 보장하려고 시도합니다. 하지만 파일을 배치할 위치를 결정하는 현재 및 미래의
  // 파일 크기는 최선의 노력에 기반한 추정값이므로, 일부 작업 부하에서는 실제
  // 크기가 경로 아래에서 목표 크기보다 약간 더 클 수 있습니다. 이런 경우를
  // 대비해 사용자에게 약간의 여유 공간을 제공해야 합니다.
  //
  // 만약 경로 중 어느 곳에도 파일을 배치할 충분한 공간이 없다면, 마지막 경로에
  // 파일이 배치됩니다.
  //
  // 최신 데이터를 더 앞에 지정된 경로에 배치하는 것도 최선의 노력에 의한
  // 것입니다. 극단적인 경우에는 사용자가 더 높은 레벨에 사용자 파일이 배치될 수
  // 있다는 점을 예상해야 합니다.
  //
  // 비어 있으면, 하나의 경로만 사용되며, 이는 DB를 열 때 전달된 db_name입니다.
  // 기본값: 비어 있음
  std::vector<DbPath> db_paths;

  // info 로그 디렉토리를 지정합니다.
  // 비어 있으면 로그 파일은 데이터와 동일한 디렉토리에 저장됩니다.
  // 비어 있지 않으면, 로그 파일은 지정된 디렉토리에 저장되며,
  // DB 데이터 디렉토리의 절대 경로가 로그 파일 이름의 접두사로 사용됩니다.
  std::string db_log_dir = "";

  // 쓰기 앞 기록(WAL)을 위한 절대 디렉토리 경로를 지정합니다.
  // 비어 있으면 로그 파일은 데이터와 동일한 디렉토리에 저장됩니다.
  // 기본적으로 dbname이 데이터 디렉토리로 사용됩니다.
  // 비어 있지 않으면 로그 파일은 지정된 디렉토리에 보관됩니다.
  // DB를 삭제할 때, wal_dir 내의 모든 로그 파일과 디렉토리 자체가 삭제됩니다.
  std::string wal_dir = "";

  // 오래된 파일이 삭제되는 주기. 기본값은 6시간입니다.
  // 컴팩션 프로세스에 의해 범위를 벗어난 파일은 이 설정에 관계없이
  // 모든 컴팩션에서 자동으로 삭제됩니다.
  //
  // 기본값: 6시간
  //
  // SetDBOptions() API를 통해 동적으로 변경 가능
  uint64_t delete_obsolete_files_period_micros = 6ULL * 60 * 60 * 1000000;

  // 동시 백그라운드 작업(컴팩션 및 플러시)의 최대 수입니다.
  //
  // 기본값: 2
  //
  // SetDBOptions() API를 통해 동적으로 변경 가능
  int max_background_jobs = 2;

  // 더 이상 사용되지 않음: RocksDB는 max_background_jobs 값을 기준으로 자동으로
  // 결정합니다. 호환성을 위해 사용자 설정에 따라 `max_background_jobs =
  // max_background_compactions + max_background_flushes` 로 설정됩니다.
  // (하나라도 설정되지 않으면 -1을 1로 교체합니다).
  //
  // 기본 LOW 우선순위 스레드 풀에 제출된 동시 백그라운드 컴팩션 작업의 최대
  // 수입니다.
  //
  // 이 값을 늘리려면 LOW 우선순위 스레드 풀의 스레드 수도 늘리는 것을
  // 고려하세요. 자세한 내용은 Env::SetBackgroundThreads를 참조하세요.
  //
  // 기본값: -1
  //
  // SetDBOptions() API를 통해 동적으로 변경 가능
  int max_background_compactions = -1;

  // 이 값은 컴팩션 작업을 여러 개의 작은 작업으로 나누어 동시에 실행하는
  // 최대 스레드 수를 나타냅니다.
  // 기본값: 1 (즉, 서브컴팩션 없음)
  //
  // SetDBOptions() API를 통해 동적으로 변경 가능
  uint32_t max_subcompactions = 1;

  // 더 이상 사용되지 않음: RocksDB는 max_background_jobs 값을 기준으로 자동으로
  // 결정합니다. 호환성을 위해 사용자 설정에 따라 `max_background_jobs =
  // max_background_compactions + max_background_flushes` 로 설정됩니다.
  //
  // 기본 HIGH 우선순위 스레드 풀에 제출된 동시 백그라운드 memtable 플러시
  // 작업의 최대 수입니다. HIGH 우선순위 스레드 풀이 0 스레드로 설정되어 있으면,
  // 플러시 작업은 LOW 우선순위 스레드 풀에서 컴팩션 작업과 함께 공유됩니다.
  //
  // 여러 DB 인스턴스에서 동일한 Env를 공유할 때 두 개의 스레드 풀을 사용하는
  // 것이 중요합니다. 별도의 풀 없이 긴 시간 동안 실행되는 컴팩션 작업이 다른 DB
  // 인스턴스의 memtable 플러시 작업을 차단할 수 있어 불필요한 Put 지연을 초래할
  // 수 있습니다.
  //
  // 이 값을 늘리려면 HIGH 우선순위 스레드 풀의 스레드 수도 늘리는 것을
  // 고려하세요. 자세한 내용은 Env::SetBackgroundThreads를 참조하세요. 기본값:
  // -1
  int max_background_flushes = -1;

  // info 로그 파일의 최대 크기를 지정합니다. 로그 파일이
  // `max_log_file_size`보다 크면 새로운 info 로그 파일이 생성됩니다.
  // max_log_file_size가 0이면 모든 로그가 하나의 로그 파일에 기록됩니다.
  size_t max_log_file_size = 0;

  // info 로그 파일의 롤링 주기(초 단위)입니다.
  // 값이 0이 아닌 값으로 지정되면, 로그 파일은 `log_file_time_to_roll`보다 더
  // 오랜 시간 동안 활성화되면 롤링됩니다. 기본값: 0 (비활성화)
  size_t log_file_time_to_roll = 0;

  // 유지할 최대 info 로그 파일 수입니다.
  // 기본값: 1000
  size_t keep_log_file_num = 1000;

  // 로그 파일을 재활용합니다.
  // 값이 0이 아닌 경우, 이전에 작성된 로그 파일을 새로운 로그를 위해 재사용하며
  // 오래된 데이터를 덮어씁니다. 이 값은 언제든지 나중에 사용할 수 있도록 유지할
  // 로그 파일 수를 나타냅니다. 이미 할당된 블록을 재사용하고, fdatasync는 각
  // 쓰기 후 inode를 업데이트할 필요가 없으므로 더 효율적입니다. 기본값: 0
  size_t recycle_log_file_num = 0;

  // 매니페스트 파일이 이 한도에 도달하면 롤링됩니다.
  // 오래된 매니페스트 파일은 삭제됩니다.
  // 기본값은 1GB로, 매니페스트 파일이 커지지만 저장 용량 한도에 도달하지 않도록
  // 설정됩니다.
  uint64_t max_manifest_file_size = 1024 * 1024 * 1024;

  // 테이블 캐시에서 사용할 샤드 수입니다.
  int table_cache_numshardbits = 6;

  // 다음 두 필드는 WAL이 아카이브되고 삭제되는 시점에 영향을 미칩니다.
  //
  // 둘 다 0이면, 오래된 WAL은 아카이브되지 않고 즉시 삭제됩니다.
  // 그렇지 않으면, 오래된 WAL은 삭제되기 전에 아카이브됩니다.
  //
  // `WAL_size_limit_MB`가 0이 아니면, 가장 이른 WAL부터 아카이브된 WAL이
  // 삭제되어 아카이브의 총 크기가 이 한도 이하로 떨어지게 됩니다. 모든 빈 WAL도
  // 삭제됩니다.
  //
  // `WAL_ttl_seconds`가 0이 아니면, `WAL_ttl_seconds`보다 오래된 아카이브된
  // WAL이 삭제됩니다.
  //
  // `WAL_ttl_seconds`만 0이 아닌 경우, 아카이브된 WAL이 삭제되는 주기는
  // `WAL_ttl_seconds / 2`초입니다. `WAL_size_limit_MB`만 0이 아닌 경우, 삭제
  // 주기는 10분마다 이루어집니다. 두 값이 모두 0이 아니면, 삭제 주기는 두 값 중
  // 최소값이 됩니다.
  uint64_t WAL_ttl_seconds = 0;
  uint64_t WAL_size_limit_MB = 0;

  // 매니페스트 파일을 미리 할당할 바이트 수(fallocate를 통해)입니다.
  // 기본값은 4MB로, 이는 무작위 I/O를 줄이고
  // 대용량 데이터를 미리 할당하는 마운트(예: xfs의 allocsize 옵션)에서 과할당을
  // 방지하는 데 합리적입니다.
  size_t manifest_preallocation_size = 4 * 1024 * 1024;

  // OS가 SST 테이블을 읽기 위해 파일을 mmap하도록 허용합니다.
  // 32비트 OS에서는 권장하지 않습니다.
  // 이 옵션이 true로 설정되고 압축이 비활성화되면, 블록은 복사되지 않고 mmap된
  // 메모리 영역에서 직접 읽히며, 블록은 블록 캐시에 삽입되지 않습니다. 그러나
  // `ReadOptions.verify_checksums`가 true로 설정되면 여전히 체크섬 검사가
  // 이루어집니다. 이는 블록을 읽을 때마다 체크섬 검사를 의미하며, 옵션이
  // false로 설정되고 블록 캐시가 사용될 때보다 더 많은 검사 횟수를 초래할 수
  // 있습니다. 이 옵션의 일반적인 사용 사례는 RocksDB를 ramfs에서 실행하는
  // 것입니다. 여기서는 체크섬 검증이 보통 필요하지 않습니다. 기본값: false
  bool allow_mmap_reads = false;

  // OS가 파일을 mmap하여 쓰기를 허용합니다.
  // DB::SyncWAL()은 이 옵션이 false로 설정되어야만 작동합니다.
  // 기본값: false
  bool allow_mmap_writes = false;

  // 읽기/쓰기에 대해 직접 I/O 모드를 활성화합니다.
  // 사용 사례에 따라 성능을 개선할 수도, 그렇지 않을 수도 있습니다.
  //
  // 파일은 "직접 I/O" 모드로 열립니다.
  // 즉, 디스크에서의 데이터 읽기/쓰기는 캐시되거나 버퍼링되지 않습니다.
  // 그러나 장치의 하드웨어 버퍼는 여전히 사용될 수 있습니다. 메모리 매핑된
  // 파일은 이 파라미터의 영향을 받지 않습니다.

  // 사용자 및 컴팩션 읽기에 O_DIRECT를 사용합니다.
  // 기본값: false
  bool use_direct_reads = false;

  // 백그라운드 플러시 및 컴팩션에서 쓰기에 O_DIRECT를 사용합니다.
  // 기본값: false
  bool use_direct_io_for_flush_and_compaction = false;

  // false이면 fallocate() 호출이 우회되어 파일 미리 할당이 비활성화됩니다.
  // 파일 공간 미리 할당은 파일 쓰기/추가 성능을 높이는 데 사용됩니다.
  // 기본적으로 RocksDB는 WAL, SST, 매니페스트 파일에 대해 공간을 미리 할당하며,
  // 파일이 기록될 때 추가된 공간은 잘립니다.
  // 경고: btrfs를 사용하는 경우, 미리 할당을 비활성화하려면
  // `allow_fallocate=false`로 설정하는 것이 좋습니다. btrfs에서 추가로 할당된
  // 공간은 해제할 수 없으며, 많은 파일을 가지고 있을 경우 큰 영향을 미칠 수
  // 있습니다. 이 제한에 대한 자세한 내용:
  // https://github.com/btrfs/btrfs-dev-docs/blob/471c5699336e043114d4bca02adcd57d9dab9c44/data-extent-reference-counts.md
  bool allow_fallocate = true;

  // 자식 프로세스가 열린 파일을 상속하지 않도록 설정합니다. 기본값: true
  bool is_fd_close_on_exec = true;

  // 0이 아니면, 매 `stats_dump_period_sec` 초마다 RocksDB 통계를 LOG에
  // 덤프합니다.
  //
  // 기본값: 600 (10분)
  //
  // SetDBOptions() API를 통해 동적으로 변경 가능
  unsigned int stats_dump_period_sec = 600;

  // 0이 아니면, 매 `stats_persist_period_sec` 초마다 RocksDB 통계를 디스크에
  // 저장합니다. 기본값: 600
  unsigned int stats_persist_period_sec = 600;

  // true인 경우, 매 `stats_persist_period_sec` 초마다 통계를 숨겨진 컬럼
  // 패밀리(`___rocksdb_stats_history___`)에 자동으로 저장합니다. 그렇지 않으면,
  // 메모리 내 구조체에 기록됩니다. 사용자는 `GetStatsHistory` API를 통해 이를
  // 쿼리할 수 있습니다. 만약 사용자가 동일한 이름의 컬럼 패밀리를 DB에
  // 생성하려고 하면, 컬럼 패밀리 생성이 실패합니다. 하지만 숨겨진 컬럼 패밀리는
  // 살아남으며, 이전에 저장된 통계도 유지됩니다. 디스크에 통계를 저장할 때,
  // 통계 이름은 최대 100바이트로 제한됩니다. 기본값: false
  bool persist_stats_to_disk = false;

  // 0이 아니면, 주기적으로 통계 스냅샷을 찍어 메모리에 저장합니다.
  // 통계 스냅샷에 대한 메모리 크기는 stats_history_buffer_size로 제한됩니다.
  // 기본값: 1MB
  size_t stats_history_buffer_size = 1024 * 1024;

  // true로 설정하면, SST 파일이 열릴 때 파일 시스템에 접근 패턴이 랜덤임을
  // 힌트로 제공합니다. 기본값: true
  bool advise_random_on_open = true;

  // 디스크에 데이터를 쓸 때 모든 컬럼 패밀리에서 memtable에 축적되는 데이터
  // 양입니다.
  //
  // 이것은 단일 memtable에 대한 제한을 설정하는 write_buffer_size와는 다릅니다.
  //
  // 이 기능은 기본적으로 비활성화되어 있습니다. 비활성화된 경우 0을 설정하고,
  // 활성화하려면 0이 아닌 값을 설정해야 합니다.
  //
  // 기본값: 0 (비활성화)
  size_t db_write_buffer_size = 0;

  // memtable의 메모리 사용량을 이 객체에 보고합니다. 동일한 객체는 여러 DB에
  // 전달될 수 있으며, 모든 DB의 크기 합계를 추적합니다. 만약 모든 DB의 활성
  // memtable 크기의 총합이 한도를 초과하면, 다음 쓰기가 발생하는 DB에서
  // 플러시가 트리거됩니다. 단, 이미 플러시 중인 컬럼 패밀리가 하나 이상 없어야
  // 합니다.
  //
  // 이 객체가 하나의 DB에만 전달되면 db_write_buffer_size와 동일하게
  // 동작합니다. write_buffer_manager가 설정되면, 설정된 값이
  // db_write_buffer_size를 덮어씁니다.
  //
  // 이 기능은 기본적으로 비활성화되어 있습니다. 비활성화된 경우 0을 설정하고,
  // 활성화하려면 0이 아닌 값을 설정해야 합니다.
  //
  // 기본값: null
  std::shared_ptr<WriteBufferManager> write_buffer_manager = nullptr;

  // 0이 아니면, 컴팩션 중에 더 큰 읽기를 수행합니다.
  // 만약 스핀 디스크에서 RocksDB를 실행하는 경우, 최소한 2MB로 설정하는 것이
  // 좋습니다. 이 값으로 RocksDB의 컴팩션은 랜덤 읽기 대신 순차 읽기를
  // 수행합니다.
  //
  // 기본값: 2MB
  //
  // SetDBOptions() API를 통해 동적으로 변경 가능
  size_t compaction_readahead_size = 2 * 1024 * 1024;

  // 이는 WinMmapReadableFile이 비버퍼링된 디스크 I/O 모드에서 사용하는 최대
  // 버퍼 크기입니다. 읽기를 위한 정렬된 버퍼를 유지해야 합니다. 버퍼는 지정된
  // 크기까지 성장할 수 있으며, 그 이후에는 더 큰 요청에 대해 일회성 버퍼를
  // 할당합니다. 비버퍼링 모드에서는 ReadaheadRandomAccessFile에서 읽기 예비
  // 버퍼를 우회합니다. 예비 읽기가 필요한 경우, compaction_readahead_size 값을
  // 사용하여 항상 예비 읽기를 시도합니다. 예비 읽기에서는 버퍼 크기를 제한에
  // 맞게 성장시키는 대신 미리 할당합니다.
  //
  // 이 옵션은 현재 Windows에서만 적용됩니다.
  //
  // 기본값: 1MB
  //
  // 특별 값: 0 - 인스턴스별 버퍼를 유지하지 않음을 의미합니다. 요청별로 버퍼를
  // 할당하고 잠금을 피합니다.
  size_t random_access_max_buffer_size = 1024 * 1024;

  // 이것은 WritableFileWriter에서 사용되는 최대 버퍼 크기입니다.
  // 직접 I/O에서는 쓰기 요청이 정렬될 수 있도록 정렬된 버퍼를 유지해야 합니다.
  // 우리는 버퍼가 한도에 도달할 때까지 크기를 키울 수 있도록 허용하고,
  // 직접 I/O를 사용할 때는 쓰기 요청의 정렬을 보장하기 위해 버퍼 크기를
  // 고정합니다. (논리적 섹터 크기가 비정상적인 경우)
  //
  // 기본값: 1024 * 1024 (1MB)
  //
  // SetDBOptions() API를 통해 동적으로 변경 가능
  size_t writable_file_max_buffer_size = 1024 * 1024;

  // 사용자 공간에서 회전한 후 커널로 넘어가는 적응형 뮤텍스를 사용합니다.
  // 뮤텍스가 많이 경합하지 않을 경우 컨텍스트 전환을 줄일 수 있습니다.
  // 그러나 뮤텍스가 뜨겁다면 회전 시간을 낭비할 수 있습니다.
  // 기본값: false
  bool use_adaptive_mutex = false;

  // 기본값으로 모든 필드에 대해 DBOptions를 생성합니다.
  DBOptions();
  // Options로부터 DBOptions를 생성합니다.
  explicit DBOptions(const Options& options);

  void Dump(Logger* log) const;

  // 파일이 디스크에 비동기적으로 백그라운드에서 기록되는 동안 OS가 파일을
  // 점진적으로 동기화하도록 허용합니다. 이 작업은 시간에 따라 쓰기 I/O를
  // 부드럽게 만들 수 있습니다. 사용자는 이를 지속성 보장에 의존해서는 안
  // 됩니다. `bytes_per_sync` 바이트가 기록될 때마다 하나의 요청을 발생시킵니다.
  // 0이면 비활성화됩니다.
  //
  // 디바이스에 대한 쓰기 속도를 조절하기 위해 rate_limiter를 사용하는 것을
  // 고려할 수 있습니다. rate_limiter가 활성화되면 자동으로 `bytes_per_sync`가
  // 1MB로 설정됩니다.
  //
  // 이 옵션은 테이블 파일에 적용됩니다.
  //
  // 기본값: 0, 비활성화됨
  //
  // 주의: WAL 파일에는 적용되지 않습니다. 대신 wal_bytes_per_sync를
  // 참조하십시오. SetDBOptions() API를 통해 동적으로 변경 가능
  uint64_t bytes_per_sync = 0;

  // bytes_per_sync와 동일하지만, WAL 파일에 적용됩니다.
  // 이 옵션은 WAL이 생성된 순서대로 동기화된다고 보장하지 않습니다.
  // 새 WAL은 이전 WAL이 동기화되지 않은 상태에서 동기화될 수 있습니다.
  // 따라서 시스템 충돌 시, WAL 데이터에서 빈 부분이 발생할 수 있어 일부 데이터
  // 손실이 발생할 수 있습니다.
  //
  // 기본값: 0, 비활성화됨
  //
  // SetDBOptions() API를 통해 동적으로 변경 가능
  uint64_t wal_bytes_per_sync = 0;

  // true일 경우, 각 주어진 시간에 WAL 파일은 최대 `wal_bytes_per_sync` 바이트,
  // SST 파일은 최대 `bytes_per_sync` 바이트가 기록 대기 중인 상태로 보장됩니다.
  // 이 옵션은 파일 생성 중 처리 속도가 I/O 속도를 초과할 때 유용할 수 있으며,
  // 이로 인해 파일이 완료될 때 대규모 동기화가 발생하는 것을 방지합니다.
  //
  //  - `sync_file_range`가 지원되면, 이는 이전 `sync_file_range`가 완료될
  //  때까지 기다린 후 진행합니다.
  //    이렇게 하면 처리(압축 등)는 `sync_file_range` 간의 간격에서 방해 없이
  //    진행될 수 있으며, I/O가 뒤처질 때만 차단됩니다.
  //  - 그렇지 않으면 `WritableFile::Sync` 메서드가 사용됩니다. 이 메커니즘은
  //  항상 차단되므로
  //    I/O와 처리가 교차되는 것을 방지합니다.
  //
  // 주의: 이 옵션을 활성화해도 추가적인 지속성 보장이 제공되지 않습니다.
  //       왜냐하면 `sync_file_range`가 메타데이터를 기록하지 않기 때문입니다.
  //
  // 기본값: false
  bool strict_bytes_per_sync = false;

  // 특정 RocksDB 이벤트가 발생할 때 콜백 함수가 호출될 EventListener의
  // 벡터입니다.
  std::vector<std::shared_ptr<EventListener>> listeners;

  // true이면, 이 DB에 관련된 스레드의 상태가 추적되고
  // GetThreadList() API를 통해 이용할 수 있게 됩니다.
  //
  // 기본값: false
  bool enable_thread_tracking = false;

  // soft_pending_compaction_bytes_limit 또는
  // level0_slowdown_writes_trigger가 발생하거나, 마지막 memtable에 쓰고 있고
  // 3개 이상의 memtable을 허용하는 경우, DB에 대한 제한된 쓰기 속도입니다.
  // 이 값은 압축 전 사용자 쓰기 요청의 크기를 사용하여 계산됩니다.
  // 컴팩션이 더 뒤처지면 RocksDB가 더 느리게 쓸 수 있습니다.
  // 값이 0이면, `rate_limiter` 값이 비어 있지 않으면 그 값에서 유추하고,
  // 비어 있으면 16MB로 설정됩니다. DB가 열린 후 사용자가 `rate_limiter`에서
  // 속도를 변경하면, `delayed_write_rate`는 조정되지 않습니다.
  //
  // 단위: 초당 바이트
  //
  // 기본값: 0
  //
  // SetDBOptions() API를 통해 동적으로 변경 가능
  uint64_t delayed_write_rate = 0;

  // 기본적으로 하나의 쓰기 스레드 큐가 유지됩니다. 큐의 맨 앞에 있는 스레드는
  // 쓰기 배치 그룹 리더가 되어 WAL과 memtable에 배치 그룹을 쓰는 역할을 합니다.
  //
  // enable_pipelined_write가 true인 경우, WAL 쓰기와 memtable 쓰기를 위한
  // 별도의 쓰기 스레드 큐가 유지됩니다. 쓰기 스레드는 먼저 WAL 작성자 큐에
  // 들어가고, 그 후에 memtable 작성자 큐에 들어갑니다. 따라서 WAL 작성자 큐에서
  // 대기 중인 스레드는 이전 쓰기 작업이 WAL 작성을 마칠 때까지 기다리지만,
  // memtable 작성을 기다릴 필요는 없습니다. 이 기능을 활성화하면 쓰기 처리량이
  // 개선되고, 두 단계 커밋의 준비 단계의 지연 시간이 줄어들 수 있습니다.
  //
  // 기본값: false
  bool enable_pipelined_write = false;

  // unordered_write를 true로 설정하면 스냅샷의 불변성 보장을 희생하여 더 높은
  // 쓰기 처리량을 얻을 수 있습니다. 이는 스냅샷에서 ::Get뿐만 아니라
  // ::MultiGet과 Iterator의 일관된 시점 보기 속성에서 기대되는 반복 가능성을
  // 위반합니다. 애플리케이션이 이러한 완화된 보장을 용납할 수 없다면, 더 높은
  // 처리량을 얻으면서 이를 해결할 수 있는 자체 메커니즘을 구현할 수 있습니다.
  // 예를 들어, WRITE_PREPARED 쓰기 정책과 two_write_queues=true를 사용하는
  // TransactionDB를 통해 unordered_write에도 불구하고 불변 스냅샷을 달성할 수
  // 있습니다.
  //
  // 기본적으로, 즉 false일 때, RocksDB는 모든 하위 시퀀스 번호를 가진 쓰기가
  // 완료되지 않으면 새로운 스냅샷을 위한 시퀀스 번호를 증가시키지 않습니다.
  // 이는 우리가 스냅샷에서 기대하는 불변성을 제공합니다. 또한 Iterator와
  // MultiGet은 내부적으로 스냅샷에 의존하기 때문에, 스냅샷의 불변성은
  // Iterator와 MultiGet이 일관된 시점 보기를 제공하게 만듭니다. true로
  // 설정하면, Read-Your-Own-Write 속성은 여전히 제공되지만, 스냅샷의 불변성
  // 속성은 완화됩니다: 스냅샷을 얻은 후에 발생한 쓰기(더 큰 시퀀스 번호를 가진
  // 쓰기)는 여전히 그 스냅샷에서 읽은 내용에는 표시되지 않지만, 여전히 대기
  // 중인 쓰기(더 작은 시퀀스 번호를 가진 쓰기)가 메모리 테이블에 적용되면
  // 스냅샷에서 볼 수 있는 상태를 변경할 수 있습니다.
  //
  // 기본값: false
  bool unordered_write = false;

  // true이면, 여러 쓰기 스레드가 동시에 memtable을 업데이트할 수 있도록
  // 허용합니다. 일부 memtable_factory만 동시 쓰기를 지원합니다. 현재는
  // SkipListFactory에서만 구현되어 있습니다. 동시 memtable 쓰기는
  // inplace_update_support나 filter_deletes와 호환되지 않습니다. 이 기능을
  // 사용하려면 enable_write_thread_adaptive_yield를 설정하는 것이 강력히
  // 권장됩니다.
  //
  // 기본값: true
  bool allow_concurrent_memtable_write = true;

  // true이면, 쓰기 배치 그룹 리더와 동기화하는 스레드는 mutex를 차단하기 전에
  // 최대 write_thread_max_yield_usec만큼 기다립니다. 이 기능은 동시 작업
  // 부하에서 처리량을 상당히 개선할 수 있습니다.
  // enable_concurrent_memtable_write가 활성화되었는지에 관계없이 적용됩니다.
  //
  // 기본값: true
  bool enable_write_thread_adaptive_yield = true;

  // WAL이나 memtable 쓰기의 단일 배치에서 작성되는 바이트 수의 최대 한도입니다.
  // 리더의 쓰기 크기가 이 한도의 1/8보다 크면 이를 따릅니다.
  //
  // 기본값: 1MB
  uint64_t max_write_batch_group_size_bytes = 1 << 20;

  // 쓰기 작업이 mutex를 차단하기 전에 쓰기 스레드가 다른 쓰기 스레드와 조정을
  // 위해 회전하는 데 사용할 최대 마이크로초입니다.
  // (write_thread_slow_yield_usec가 제대로 설정되어 있다고 가정) 이 값을
  // 증가시키면 CPU 사용량이 증가하는 대신 RocksDB의 처리량이 증가할 수
  // 있습니다.
  //
  // 기본값: 100
  uint64_t write_thread_max_yield_usec = 100;

  // std::this_thread::yield 호출이 다른 프로세스나 스레드가 현재 코어를
  // 사용하려는 신호로 간주되는 마이크로초 단위의 대기 시간입니다. 이 값을
  // 증가시키면 쓰기 스레드가 회전하면서 CPU를 더 많이 사용할 가능성이 높아지며,
  // 이는 강제로 발생한 컨텍스트 전환 횟수 증가로 나타납니다.
  //
  // 기본값: 3
  uint64_t write_thread_slow_yield_usec = 3;

  // true이면, DB::Open()에서 많은 파일에서 테이블 속성을 로드하여 컴팩션 결정을
  // 최적화하는 데 사용하는 통계를 업데이트하지 않습니다. 이 기능을 끄면 디스크
  // 환경에서 DB 열기 시간이 개선됩니다.
  //
  // 기본값: false
  bool skip_stats_update_on_db_open = false;

  // true이면, DB::Open()에서 모든 sst 파일의 크기를 가져와서 확인하지 않습니다.
  // sst 파일이 많을 경우, 특히 GetFileSize()가 비싼 비기본 Env를 사용하는 경우
  // 시작 시간이 크게 단축될 수 있습니다. 여전히 모든 필수 sst 파일이 존재하는지
  // 확인합니다. paranoid_checks가 false이면 이 옵션은 무시되고 sst 파일은 전혀
  // 확인되지 않습니다.
  //
  // 기본값: false
  bool skip_checking_sst_file_sizes_on_db_open = false;

  // WAL을 재생할 때 일관성을 제어하는 복구 모드입니다.
  // 기본값: kPointInTimeRecovery
  WALRecoveryMode wal_recovery_mode = WALRecoveryMode::kPointInTimeRecovery;

  // false로 설정되면, WAL에서 준비된 트랜잭션이 발견되면 복구가 실패합니다.
  bool allow_2pc = false;

  // 테이블 수준 행을 위한 전역 캐시입니다.
  // Get() 쿼리 속도를 높이는 데 사용됩니다.
  // 참고: 아직 DeleteRange()와는 작동하지 않습니다.
  // 기본값: nullptr (비활성화)
  std::shared_ptr<RowCache> row_cache = nullptr;

  // 복구 중 WAL(쓰기 앞 기록)을 처리할 때 호출되는 필터 객체입니다.
  // 이 필터는 로그 기록을 검사하고 특정 기록을 무시하거나 재생을 건너뛸 수 있는
  // 방법을 제공합니다. 이 필터는 시작 시 호출되며 현재는 단일 스레드에서만
  // 호출됩니다.
  WalFilter* wal_filter = nullptr;

  // 더 이상 사용되지 않음: 이 옵션은 향후 릴리스에서 제거될 수 있습니다.
  //
  // true이면, 옵션 파일이 제대로 영속화되지 않은 경우 DB::Open,
  // CreateColumnFamily, DropColumnFamily, SetOptions가 실패합니다.
  //
  // 기본값: true
  bool fail_if_options_file_error = true;

  // true이면, rocksdb.stats와 함께 malloc 통계를 LOG에 출력합니다.
  // 기본값: false
  bool dump_malloc_stats = false;

  // 기본적으로 RocksDB는 WAL 로그를 재생하고 DB를 열 때 이를 플러시합니다. 이로
  // 인해 매우 작은 SST 파일이 생성될 수 있습니다. 이 옵션이 활성화되면,
  // RocksDB는 복구 중에 플러시를 피하려고 시도합니다(보장하지는 않음). 또한
  // 기존의 WAL 로그는 유지되며, 플러시 전에 충돌이 발생하면 복구할 수 있는
  // 로그가 여전히 존재합니다.
  //
  // 기본값: false
  bool avoid_flush_during_recovery = false;

  // 기본적으로 RocksDB는 DB를 닫을 때 모든 memtable을 플러시합니다. (WAL이
  // 비활성화된 경우) 플러시를 건너뛰어 DB 종료 속도를 높일 수 있습니다.
  // 플러시되지 않은 데이터는 손실됩니다.
  //
  // 기본값: false
  //
  // SetDBOptions() API를 통해 동적으로 변경 가능
  bool avoid_flush_during_shutdown = false;

  // 데이터베이스 생성 시 이 옵션을 true로 설정하면, IngestExternalFile()을
  // 사용하여 이미 존재하는 키를 건너뛰고 (매칭되는 키를 덮어쓰지 않고) 외부
  // 파일을 수집할 수 있습니다. 이 옵션을 true로 설정하면 다음과 같은 영향을
  // 미칩니다: 1) SST 파일 압축에 대한 일부 내부 최적화가 비활성화됩니다. 2)
  // 마지막 레벨을 수집된 파일 전용으로 예약합니다. 3) 컴팩션은 마지막 레벨의
  // 파일을 포함하지 않습니다. 이 기능은 오직 유니버설 컴팩션에서만 지원됩니다.
  // `num_levels`는 이 옵션을 활성화하면 3 이상이어야 합니다.
  //
  // 기본값: false
  // 불변
  bool allow_ingest_behind = false;

  // 활성화하면 쓰기를 위한 두 개의 큐를 사용합니다. 하나는 disable_memtable이
  // 설정된 쓰기, 다른 하나는 memtable에도 쓰는 쓰기를 위한 큐입니다. 이를 통해
  // memtable 쓰기가 다른 쓰기보다 뒤처지지 않도록 할 수 있습니다. 이는 MySQL
  // 2PC 최적화에 사용될 수 있으며, 여기서는 오직 커밋만이 (직렬로) memtable에
  // 쓰여집니다.
  bool two_write_queues = false;

  // true이면, 각 쓰기 후 WAL이 자동으로 플러시되지 않습니다. 대신, FlushWAL을
  // 수동으로 호출하여 WAL 버퍼를 파일에 쓰게 됩니다.
  bool manual_wal_flush = false;

  // 활성화하면 WAL 기록이 쓰기 전에 압축됩니다. 현재 지원되는 압축 방식은
  // ZSTD(= kZSTD)뿐입니다. (다른 압축 유형에 대한 스트리밍 지원이 추가될
  // 때까지). 압축된 WAL 기록은 해당 WAL이 읽힐 때 (RocksDB 7.4.0 이상에서 ZSTD
  // 지원) 이 설정에 관계없이 읽힙니다.
  CompressionType wal_compression = kNoCompression;

  // true로 설정하면, 이전 동작을 재설정하여 완료된 동기화된 WAL 파일을 배경
  // 스레드에서 삭제되기 전까지 쓰기용으로 열어 둡니다. 파일 Close()에 성능
  // 문제가 없다면 이 옵션을 활성화할 필요는 없습니다. true로 설정하면,
  // Checkpoint가 여전히 쓰기용으로 열려 있는 WAL에 대해 LinkFile을 호출할 수
  // 있습니다. 이는 일부 파일 시스템 구현에서는 지원되지 않을 수 있습니다. 이
  // 옵션은 임시 비활성화 스위치로 의도되었으므로 이미 DEPRECATED입니다.
  bool background_close_inactive_wals = false;

  // true이면, RocksDB는 여러 컬럼 패밀리를 플러시하고 그 결과를 MANIFEST에
  // 원자적으로 커밋하는 것을 지원합니다. WAL이 항상 활성화된 경우에는
  // atomic_flush를 true로 설정할 필요가 없으며, WAL은 DB를 마지막 영속 상태로
  // 복구할 수 있도록 합니다. 이 옵션은 WAL로 보호되지 않는 쓰기가 있는 컬럼
  // 패밀리가 있을 때 유용합니다. 수동 플러시에서는 애플리케이션이 DB::Flush에서
  // 원자적으로 플러시할 컬럼 패밀리를 지정해야 합니다. 자동으로 트리거된
  // 플러시에서는 RocksDB가 모든 컬럼 패밀리를 원자적으로 플러시합니다.
  //
  // 현재, atomic_flush 이후의 모든 WAL 활성화된 쓰기는 프로세스가 충돌하고
  // 복구하려 할 때 독립적으로 재생될 수 있습니다.
  bool atomic_flush = false;

  // true이면, 작업 스레드는 불필요하고 긴 지연 시간을 초래하는 작업(예:
  // 불필요한 파일 삭제 또는 memtable 삭제)을 피하고 대신 백그라운드 작업으로
  // 예약합니다. 지연 시간에 민감한 경우에 사용하세요. true로 설정되면,
  // ReadOptions::background_purge_on_iterator_cleanup보다 우선합니다.
  bool avoid_unnecessary_blocking_io = false;

  // DB 고유 ID는 DB 매니페스트(선호됨, 이 옵션) 또는 IDENTITY 파일(역사적, 더
  // 이상 사용되지 않음), 또는 둘 다에 기록될 수 있습니다. 이 옵션이 false로
  // 설정되면(구 버전) write_identity_file을 true로 설정해야 합니다.
  // 매니페스트가 선호되는 이유는
  // 1. IDENTITY 파일은 체크섬이 없으므로 손상에 대해 덜 안전합니다.
  // 2. IDENTITY 파일은 DB와 함께 복사되지 않을 수 있습니다(예: BackupEngine에서
  // 복사하지 않음),
  //    따라서 DB의 출처를 신뢰할 수 없습니다.
  // 이 옵션은 결국 구식이 되어 Identity 파일이 폐지될 때 제거될 수 있습니다.
  bool write_dbid_to_manifest = true;

  // Identity 파일은 매니페스트에 DB ID를 기록하는 것으로 폐지될 것으로
  // 예상됩니다. 이를 true로 설정하면 Identity 파일을 기록하는 이전 동작을
  // 유지하며, false로 설정하면 미래의 기본값으로 설정될 것으로 예상됩니다. 이
  // 옵션은 결국 구식이 되어 Identity 파일이 폐지될 때 제거될 수 있습니다.
  bool write_identity_file = true;

  // 활성화하면 prefix_extractor가 nullptr이 아닐 때, 반복자는 불행히도
  // *가능하게* 같은 접두사 내에서만 데이터를 반환하는 기본 동작을 합니다.
  // "거리에서의 유령 같은 작용(spooky action at a distance)"을 피하기 위해,
  // 반복자 범위는 반복자를 생성하거나 탐색할 때 설정해야 하며, 변경 가능한 컬럼
  // 패밀리 옵션에서 유래된 것이 아니어야 합니다.
  //
  // true로 설정하면, 모든 반복자가 total_order_seek=true로 생성된 것처럼
  // 취급되며, auto_prefix_mode=true와 prefix_same_as_start=true만 접두사 탐색
  // 최적화를 활용할 수 있습니다.
  bool prefix_seek_opt_in_only = false;

  // 로그를 읽을 때 미리 가져올 바이트 수입니다. 이는 원격 위치에 있는 로그를
  // 읽을 때 유용하며, 왕복 횟수를 줄일 수 있습니다. 0이면 미리 가져오기가
  // 비활성화됩니다.
  //
  // 기본값: 0
  size_t log_readahead_size = 0;

  // 사용자가 체크섬 생성기 팩토리를 제공하지 않으면, 파일 체크섬은 사용되지
  // 않습니다. 새로운 파일 체크섬 생성기 객체는 SST 파일이 생성될 때마다
  // 만들어집니다. 따라서 각 생성된 FileChecksumGenerator는 단일 스레드에서만
  // 사용되므로 스레드 안전성을 필요로 하지 않습니다.
  //
  // 기본값: nullptr
  std::shared_ptr<FileChecksumGenFactory> file_checksum_gen_factory = nullptr;

  // 기본적으로, RocksDB는 DB 파일에서 데이터 손실이나 손상을 감지하여
  // 사용자에게 오류를 반환하려고 시도합니다. 이 정책의 예외는 WAL 파일로, 해당
  // 복구는 wal_recovery_mode 옵션에 의해 제어됩니다.
  //
  // Best-efforts 복구(이 옵션이 true로 설정됨)는 각 컬럼 패밀리의 시점에서
  // 유효한 상태로 DB를 여는 것을 선호하는 방식입니다. 기본적으로는 WAL이 아닌
  // 데이터 손실을 사용자에게 오류로 반환하지만, 이 옵션을 통해 빈/새로운 상태를
  // 포함한 시점에 유효한 상태를 복구하려고 시도합니다. RocksDB 사용자 데이터
  // 측면에서 이는 WALRecoveryMode::kPointInTimeRecovery를 각 컬럼 패밀리에
  // 적용하는 것과 같습니다.
  //
  // "AtomicGroup"이 MANIFEST에 존재하면, 이는 현재 `atomic_flush == true`인
  // 경우에만 적용됩니다. 그 경우, 모든 기존 CF가 해당 그룹을 복구해야만 해당
  // 그룹이 모두 일괄적으로 적용될 수 있습니다. 이로 인해 유효하지 않은 파일
  // 시스템 상태를 가진 비활성 CF가 있으면 해당 CF들의 복구를 차단할 수
  // 있습니다.
  //
  // Best-efforts 복구(BER)는 DB 파일이 누락되거나 크기가 잘리면서 일부 크기만
  // 남은 경우를 복구하도록 설계되었습니다. BER은 SST 파일이 다른 파일로
  // 교체되었는지도 감지할 수 있습니다(단, DB 매니페스트에서 SST 고유 ID가
  // 추적되는 경우). BER은 DB 파일의 다른 손상(일반적으로 DB::VerifyChecksum()로
  // 감지 가능)에는 대응하지 않으며, WAL 파일의 복구도 시도하지 않습니다.
  //
  // 예를 들어, MANIFEST에서 참조된 SST 또는 blob 파일이 누락된 경우, BER은 해당
  // 컬럼 패밀리의 "시점" 버전과 일치하는 파일 집합을 찾을 수 있습니다. 이
  // 버전은 이전 MANIFEST 파일에서 올 수 있습니다. 또한, L0 파일의 접미사만
  // 누락된 불완전한 버전도 복구할 수 있습니다. 사용자의 관점에서 보면, L0
  // 파일의 접미사 누락은 사용자가 최근에 쓴 데이터를 놓친 것을 의미합니다.
  // 하지만 남은 파일들은 여전히 유효한 시점의 뷰를 제공합니다. atomic
  // flush에서는 모든 컬럼 패밀리에서 일관된 뷰가 보장되지만, 불완전한 버전
  // 복구에서는 그 보장이 되지 않습니다. `ldb repair`와는 달리 BER은 적어도
  // 하나의 유효한 MANIFEST 파일이 있어야 복구가 가능합니다.
  //
  // 기본값: false
  bool best_efforts_recovery = false;

  // 배경에서 재시도 가능한 I/O 오류가 발생할 때 별도의 스레드에서
  // DB::Resume()을 호출하는 횟수를 정의합니다. 배경에서 재시도 가능한 I/O
  // 오류가 발생하면 SetBGError가 호출되어 오류를 처리합니다. 오류가 자동으로
  // 복구될 수 있으면(예: Flush 또는 WAL 쓰기 중 재시도 가능한 I/O 오류), DB는
  // 배경에서 오류를 복구하기 위해 DB::Resume을 호출합니다. 이 값이 0 또는
  // 음수이면 DB::Resume()은 자동으로 호출되지 않습니다.
  //
  // 기본값: INT_MAX
  int max_bgerror_resume_count = INT_MAX;

  // max_bgerror_resume_count가 2 이상이면 DB가 여러 번 재개됩니다.
  // 이 옵션은 이전 재개가 실패하고 재개 조건을 만족할 경우, 다음 재개를 얼마나
  // 기다릴지 결정합니다.
  //
  // 기본값: 1000000 (마이크로초)
  uint64_t bgerror_resume_retry_interval = 1000000;

  // 손상된 키/값을 포함하는 오류 메시지를 받도록 사용자가 선택할 수 있게
  // 합니다. 손상된 키와 값은 메시지/로그/상태에 기록되며, 사용자에게 영향을
  // 받는 데이터에 관한 유용한 정보를 제공합니다. 기본값은 false로 설정되어 있어
  // 사용자 데이터를 로그/메시지에 노출시키지 않도록 방지합니다.
  //
  // 기본값: false
  bool allow_data_in_errors = false;

  // DB가 호스팅된 머신을 식별하는 문자열입니다. 이 문자열은 DB가 쓴 모든 SST
  // 파일에 속성으로 기록됩니다. 이 속성은 파일을 쓸 때 실패하는 호스트가 메모리
  // 손상으로 인해 문제가 발생한 경우, 해당 호스트로 추적하여 문제를 해결하는 데
  // 유용할 수 있습니다. 이러한 손상은 체크섬으로는 포착되지 않기 때문에, 실제
  // 호스트 이름으로 이 속성을 대체하여 SST 파일을 기록합니다. 기본값으로 두면,
  // 테이블 작성자는 실제 호스트 이름으로 이를 대체하여 SST 파일을 기록합니다.
  // 빈 문자열로 설정하면 이 속성은 SST 파일에 기록되지 않습니다.
  //
  // 기본값: hostname
  std::string db_host_id = kHostnameForDbHostId;

  // DB에서 특정 파일 유형의 쓰기 시 체크섬 이양을 활성화하려면 이 옵션을
  // 사용하세요. 사용하는 파일 시스템이 crc32c 체크섬 검증을 지원하는지
  // 확인하세요. 현재 지원되는 파일 유형: kWALFile, kTableFile, kDescriptorFile.
  // NOTE: 현재 RocksDB는 이양을 위한 crc32c 기반 체크섬만 생성합니다.
  // 저장소 계층이 다른 체크섬을 지원하는 경우, 사용자는 이 설정을 비워두어야
  // 합니다. 그렇지 않으면 예기치 않은 쓰기 실패가 발생할 수 있습니다.
  FileTypeSet checksum_handoff_file_types;

  // 실험적
  // CompactionService는 사용자가 다른 호스트나 프로세스에서 컴팩션을 실행할 수
  // 있는 기능으로, 기본 호스트의 백그라운드 로드를 분산시킵니다. 이 기능은
  // 실험적이며, 인터페이스는 현재 후방/전방 호환성 없이 변경될 수 있습니다.
  // 일부 알려진 문제는 아직 개발 중입니다.
  std::shared_ptr<CompactionService> compaction_service = nullptr;

  // 특정 DB에 대해 어떤 최하위 캐시 계층을 사용할지 나타냅니다.
  // 현재 volatile_tier와 non_volatile_tier가 지원됩니다. 이들은 계층화되어
  // 있습니다. kVolatileTier로 설정하면, 현재 구현된 volatile_tier인 블록 캐시만
  // 사용됩니다. 따라서 캐시 항목은 보조 캐시(non_volatile_tier)에 넘겨지지
  // 않으며, 블록 캐시 조회 실패는 보조 캐시에서 조회되지 않습니다.
  // kNonVolatileBlockTier가 사용되면, 블록 캐시와 보조 캐시를 모두 사용합니다.
  //
  // 기본값: kNonVolatileBlockTier
  CacheTier lowest_used_cache_tier = CacheTier::kNonVolatileBlockTier;

  // 더 이상 사용되지 않음: 이 옵션은 향후 릴리스에서 제거될 수 있습니다.
  //
  // false로 설정되면, 컴팩션이나 플러시가 동일한 사용자 키에 대해 SingleDelete
  // 후 Delete가 발견되면 컴팩션 작업이 실패하지 않습니다. 그렇지 않으면 컴팩션
  // 작업이 실패합니다. 이는 기존 사용 사례가 마이그레이션할 수 있도록 돕는 임시
  // 옵션이며, 향후 릴리스에서 제거될 예정입니다. 경고: 이 값을 false로 설정하지
  // 마세요. 단, SingleDelete 계약이 강제되지 않아
  // (https://github.com/facebook/rocksdb/wiki/Single-Delete) 동일한 사용자 키에
  // 대해 Delete와 SingleDelete가 혼합된 기존 데이터를 마이그레이션하려는
  // 경우에만 설정하십시오. 계약 위반은 예기치 않은 동작을 초래하며 데이터
  // 불일치가 발생할 수 있습니다. 예를 들어, 삭제된 이전 데이터가 다시 보이게
  // 되는 등의 문제가 발생할 수 있습니다.
  bool enforce_single_del_contracts = true;

  // RocksDB에서 비수기 시간 인식 구현. 여기서 "비수기 시간"은 다른 시간에 비해
  // 읽기와 쓰기 활동이 상당히 적은 기간을 의미합니다. 이 지식을 활용하여 TTL
  // 기반 컴팩션과 같은 낮은 우선순위 작업이 피크 시간대 동안 읽기 및 쓰기
  // 작업과 경쟁하지 않도록 할 수 있습니다. 본질적으로 이러한 작업을 피크 주기가
  // 시작되기 전 비수기 시간에 미리 처리합니다. 예를 들어, TTL이 25일로 설정되어
  // 있으면 24일의 비수기 시간에 파일을 컴팩트할 수 있습니다.
  //
  // UTC 기준 하루 중 시간, 시작 시간-끝 시간 포함.
  // 형식 - HH:mm-HH:mm (00:00-23:59)
  // 시작 시간이 끝 시간보다 크면, 이 기간이 다음 날로 이어진다고 간주됩니다
  // (예: 23:30-04:00). 하루를 비수기로 만들려면 "0:00-23:59"를 사용하세요.
  // 비수기 시간을 설정하지 않으려면 이 필드를 비워두세요. 기본값: 빈 문자열
  // (비수기 없음)
  std::string daily_offpeak_time_utc = "";

  // 실험적

  // RocksDB 데이터베이스가 팔로워 모드로 열릴 때, 이 옵션은 사용자가 팔로워가
  // 리더의 상태를 새로 고치는 빈도를 요청하는 데 사용됩니다. RocksDB는
  // 데이터베이스 상태에 변화가 감지되면 더 자주 동기화를 시도할 수 있습니다.
  // 기본값은 10초마다.
  uint64_t follower_refresh_catchup_period_ms = 10000;

  // 주어진 동기화 시도에서, 이 옵션은 새로운 일관된 버전을 설치하려고 시도하는
  // 횟수를 지정합니다. 매우 드물지만, 리더가 LSM을 매우 높은 속도로 변형하고
  // 팔로워가 일관된 뷰를 얻을 수 없는 경우 동기화가 실패할 수 있습니다.
  // 기본값은 10번 시도
  uint64_t follower_catchup_retry_count = 10;

  // 연속된 동기화 시도 간의 대기 시간
  // 기본값 100ms
  uint64_t follower_catchup_retry_wait_ms = 100;

  // SST, blob, WAL 파일 외의 DB 파일이 생성될 때, 이 파일 시스템 온도를
  // 사용합니다. (또한 `wal_write_temperature`와 다양한 `*_temperature` CF
  // 옵션도 참조하세요.) `kUnknown`이 아닌 값으로 설정하면, 이는
  // OptimizeForManifestWrite 함수에서 설정한 온도를 덮어씁니다.
  Temperature metadata_write_temperature = Temperature::kUnknown;

  // WAL 파일을 생성할 때 이 파일 시스템 온도를 사용합니다.
  // `kUnknown`이 아닌 값으로 설정하면, 이는 OptimizeForLogWrite 함수에서 설정한
  // 온도를 덮어씁니다.
  Temperature wal_write_temperature = Temperature::kUnknown;
  // 실험적 끝
};

// 데이터베이스의 동작을 제어하는 옵션 (DB::Open에 전달)
struct Options : public DBOptions, public ColumnFamilyOptions {
  // 모든 필드에 대한 기본값을 사용하여 Options 객체를 생성합니다.
  Options() : DBOptions(), ColumnFamilyOptions() {}

  Options(const DBOptions& db_options,
          const ColumnFamilyOptions& column_family_options)
      : DBOptions(db_options), ColumnFamilyOptions(column_family_options) {}

  // 이전 버전에서 일부 기본값을 변경합니다.
  // 더 이상 유지되지 않음: 이 함수는 유지되지 않으며 향후 릴리스에서 제거될 수
  // 있습니다. 더 이상 사용되지 않음: 이 함수는 향후 릴리스에서 제거될 수
  // 있습니다. 일반적으로 기본값은 광범위한 관심에 맞게 변경됩니다. 업그레이드
  // 시 변경을 선택적으로 적용하려면 신중히 고려해야 합니다.
  Options* OldDefaults(int rocksdb_major_version = 4,
                       int rocksdb_minor_version = 6);

  void Dump(Logger* log) const;

  void DumpCFOptions(Logger* log) const;

  // RocksDB 최적화를 쉽게 할 수 있는 일부 함수들

  // 대량 로딩에 적합한 매개변수를 설정합니다.
  // 이 함수가 "this"를 반환하는 이유는 미래에 유사한 여러 호출을 체이닝할 수
  // 있도록 하기 위함입니다.

  // 모든 데이터가 레벨 0에 있으며 자동 컴팩션 없이 저장됩니다.
  // 데이터베이스에서 읽기 전에 CompactRange(NULL, NULL)을 수동으로 호출하는
  // 것이 좋습니다. 그렇지 않으면 읽기가 매우 느릴 수 있습니다.
  Options* PrepareForBulkLoad();

  // DB가 매우 작고 (예: 1GB 이하) memtables에 많은 메모리를 소비하고 싶지 않은
  // 경우 사용합니다.
  Options* OptimizeForSmallDb();

  // 소프트웨어 논리 오류나 CPU+메모리 하드웨어 오류가 없는 경우 불필요한 일부
  // 검사를 비활성화합니다. 이는 쓰기 속도를 개선할 수 있지만, 임시 용도로만
  // 사용해야 합니다. 저장소의 손상에 대한 보호는 변경되지 않습니다 (예:
  // verify_checksums).
  Options* DisableExtraChecks();
};

// 애플리케이션은 읽기 요청(Get/Iterator)을 발행할 수 있으며,
// 해당 읽기가 지정된 캐시 레벨에 이미 존재하는 데이터를 처리해야 하는지 여부를
// 지정할 수 있습니다. 예를 들어, 애플리케이션이 kBlockCacheTier를 지정하면, Get
// 호출은 이미 memtable이나 블록 캐시에 처리된 데이터를 처리합니다. OS 캐시에서
// 데이터를 가져오거나 저장소에 있는 데이터를 페이지로 가져오지 않습니다.
enum ReadTier {
  kReadAllTier =
      0x0,  // memtable, block cache, OS cache 또는 저장소에 있는 데이터
  kBlockCacheTier = 0x1,  // memtable 또는 block cache에 있는 데이터
  kPersistedTier =
      0x2,  // 영속적인 데이터. WAL이 비활성화되면 이 옵션은 memtable의 데이터를
            // 건너뜁니다. 현재 이 ReadTier는 Get과 MultiGet만 지원하고,
            // iterators는 지원하지 않습니다.
  kMemtableTier =
      0x3  // memtable에 있는 데이터. memtable 전용 iterator에서 사용됩니다.
};

// 읽기 작업의 동작을 제어하는 옵션들
struct ReadOptions {
  // *** 포인트 조회 및 스캔에 관련된 옵션들 ***

  // "snapshot"이 nullptr이 아니면, 지정된 스냅샷으로 읽기를 수행합니다.
  // (스냅샷은 읽고 있는 DB에 속해야 하며, 이미 해제되지 않아야 합니다).
  // "snapshot"이 nullptr이면, 이 읽기 작업의 시작 시점에서 암시적 스냅샷을
  // 사용합니다.
  const Snapshot* snapshot = nullptr;

  // 작업의 타임스탬프입니다. 읽기는 지정된 타임스탬프에서 볼 수 있는 최신
  // 데이터를 반환해야 합니다. 동일한 데이터베이스의 모든 타임스탬프는 같은
  // 길이와 형식을 가져야 합니다. 사용자는 Comparator를 통해 <key, timestamp>
  // 튜플을 비교하는 맞춤 비교 함수를 제공해야 합니다. iterator의 경우,
  // iter_start_ts는 하한선(더 오래된 데이터)이고, timestamp는 상한선 역할을
  // 합니다. 타임스탬프 범위에 포함되는 동일한 레코드 버전들이 반환됩니다.
  // iter_start_ts가 nullptr이면, 타임스탬프에서 볼 수 있는 가장 최근 버전만
  // 반환됩니다. 사용자 지정 타임스탬프 기능은 현재 활성 개발 중이며, API는
  // 변경될 수 있습니다.
  const Slice* timestamp = nullptr;
  const Slice* iter_start_ts = nullptr;

  // API 호출(Get/MultiGet/Seek/Next)의 완료 기한을 마이크로초 단위로
  // 설정합니다. 이는 epoch 이후 마이크로초로 설정되어야 하며, 즉, gettimeofday
  // 또는 동등한 방식에 허용된 시간을 더한 값입니다. 최선의 방법은
  // env->NowMicros() + 일부 타임아웃을 사용하는 것입니다. 이는 최선의 노력이며,
  // 파일 시스템이 기한을 지원하지 않거나 배치 처리 시 모든 키에 대해 기한을
  // 확인하지 않으면 기한을 초과할 수 있습니다.
  std::chrono::microseconds deadline = std::chrono::microseconds::zero();

  // 파일 시스템에 전달할 읽기 타임아웃을 마이크로초 단위로 설정합니다.
  // deadline과 달리, 이 값은 각 개별 파일 읽기 요청에 대한 타임아웃을
  // 설정합니다. MultiGet/Get/Seek/Next 호출이 여러 개의 읽기를 결과로 낳는
  // 경우, 각 읽기는 최대 io_timeout까지 지속될 수 있습니다.
  std::chrono::microseconds io_timeout = std::chrono::microseconds::zero();

  // 이 읽기 요청이 특정 캐시에서 이미 존재하는 데이터를 처리해야 하는지
  // 지정합니다. 지정된 캐시에서 필요한 데이터를 찾을 수 없으면
  // Status::Incomplete가 반환됩니다.
  ReadTier read_tier = kReadAllTier;

  // 이 옵션과 관련된 파일 읽기에 대해 지정된 우선순위로 내부 속도 제한기를
  // 차지합니다. 특수 값 `Env::IO_TOTAL`은 속도 제한기 차지를 비활성화합니다.
  //
  // 속도 제한은 일반 테이블(이때 `ColumnFamilyOptions::table_factory`가
  // `PlainTableFactory`인 경우) 및 큐쿠 테이블(이때
  // `ColumnFamilyOptions::table_factory`가 `CuckooTableFactory`인 경우)의 파일
  // 읽기에는 우회됩니다.
  //
  // 속도 제한기로 차지된 바이트 수는 파일 읽기 바이트와 정확히 일치하지 않을 수
  // 있습니다. 예를 들어, 파일 헤더/푸터와 같은 일부 미미한 읽기는 현재 속도
  // 제한기로 차지하지 않습니다.
  Env::IOPriority rate_limiter_priority = Env::IO_TOTAL;

  // It limits the maximum cumulative value size of the keys in batch while
  // reading through MultiGet. Once the cumulative value size exceeds this
  // soft limit then all the remaining keys are returned with status Aborted.
  uint64_t value_size_soft_limit = std::numeric_limits<uint64_t>::max();

  // 병합 연산자가 적용된 수가 이 임계값을 초과하면
  // 성공적인 쿼리 중에 연산은 kMergeOperandThresholdExceeded 하위 코드와 함께
  // 특별한 OK 상태를 반환합니다. 현재는 포인트 조회에만 적용되며 기본적으로
  // 비활성화되어 있습니다.
  std::optional<size_t> merge_operand_count_threshold;

  // true로 설정되면, 기본 저장소에서 읽은 모든 데이터는
  // 해당 체크섬과 비교하여 검증됩니다.
  bool verify_checksums = true;

  // 이 반복에서 읽은 "데이터 블록"/"인덱스 블록"이
  // 블록 캐시에 저장되어야 하는지 여부를 지정합니다.
  // 호출자는 대량 스캔에 대해 이 필드를 false로 설정할 수 있습니다.
  // 이렇게 하면 기존 항목의 캐시 제거 순서가 변경되지 않도록 할 수 있습니다.
  bool fill_cache = true;

  // true로 설정되면, 키 조회 경로에서 범위 tombstone 처리가 생략됩니다.
  // DeleteRange() 호출을 사용하지 않는 DB 인스턴스의 경우, 이 설정은
  // 읽기 성능을 최적화하는 데 사용될 수 있습니다.
  // 이 가정(이전 DeleteRange() 호출 없음)이 깨지면, 오래된 키가 읽기 경로에서
  // 제공될 수 있습니다.
  bool ignore_range_deletions = false;

  // async_io가 활성화된 경우, RocksDB는 일부 데이터를 비동기적으로 미리
  // 가져옵니다. RocksDB는 읽기가 순차적일 때 자동으로 미리 가져오기를
  // 적용합니다.
  bool async_io = false;

  // 실험적
  //
  // async_io가 설정되면, 이 플래그는 우리가 SST 파일을 여러 레벨에서
  // 비동기적으로 읽을지 여부를 제어합니다. 이 플래그를 활성화하면 MultiGet
  // 배치의 키가 다른 레벨에 있을 경우, SST 파일을 병렬로 최대한 많이 읽어
  // MultiGet 지연 시간을 줄이는 데 도움이 될 수 있습니다. 다만 약간 더 높은 CPU
  // 오버헤드가 발생할 수 있습니다.
  bool optimize_multiget_for_io = true;

  // *** 포인트 조회 및 스캔과 관련된 옵션 끝 ***
  // *** 반복자나 스캔에만 관련된 옵션 시작 ***

  // RocksDB는 테이블 파일에 대해 두 번 이상의 읽기가 발생하면 자동으로
  // 리드어헤드를 시작합니다. 리드어헤드는 8KB에서 시작하여 각 추가 읽기마다 두
  // 배씩 증가하여 최대 256KB까지 확장됩니다. 이 옵션은 대부분의 범위 스캔이
  // 크고, 자동 리드어헤드에서 설정된 것보다 더 큰 리드어헤드가 필요한 경우에
  // 도움이 될 수 있습니다. 일반적으로 리드어헤드 크기(> 2MB)를 사용하면
  // 회전하는 디스크에서 전방 순차 반복 성능을 개선할 수 있습니다.
  size_t readahead_size = 0;

  // 반복자 탐색이 불완전한 상태로 실패하기 전에 건너뛸 수 있는 키의 수에 대한
  // 임계값입니다. 기본값 0은 키를 너무 많이 건너뛰어도 요청이 불완전한 상태로
  // 실패하지 않도록 설정됩니다.
  uint64_t max_skippable_internal_keys = 0;

  // `iterate_lower_bound`는 역방향 반복자가 항목을 반환할 수 있는 가장 작은
  // 키를 정의합니다. 경계값을 지나면 Valid()는 false가 됩니다.
  // `iterate_lower_bound`는 포함됩니다(즉, 경계 값은 유효한 항목입니다).
  //
  // prefix_extractor가 null이 아니면, Seek 대상과 `iterate_lower_bound`는 같은
  // 접두사를 가져야 합니다. 이는 접두사 도메인 외부에서는 순서가 보장되지 않기
  // 때문입니다.
  //
  // 사용자 정의 타임스탬프가 활성화된 경우, `iterate_lower_bound`는 타임스탬프
  // 부분이 없는 키를 가리켜야 합니다.
  const Slice* iterate_lower_bound = nullptr;

  // "iterate_upper_bound"는 순방향 반복자가 항목을 반환할 수 있는 범위를
  // 정의합니다. 경계가 도달하면, Valid()는 false가 됩니다.
  // "iterate_upper_bound"는 배타적(exclusive)입니다. 즉, 경계 값은 유효한
  // 항목이 아닙니다. prefix_extractor가 null이 아니면:
  // 1. options.auto_prefix_mode = true일 경우, iterate_upper_bound는
  //    RocksDB에서 접두사 반복(예: 접두사 bloom 필터 적용)을 사용할 수 있는지
  //    여부를 유추하는 데 사용됩니다. 이는 iterate_upper_bound와 seek 키를
  //    비교하여 수행됩니다.
  // 2. options.auto_prefix_mode = false일 경우, iterate_upper_bound는
  //    seek 키와 동일한 접두사를 가질 때만 영향을 미칩니다. 만약
  //    iterate_upper_bound가 seek 키의 접두사 범위를 벗어나면, 접두사 범위를
  //    벗어난 키는 정의되지 않으며, 마치 iterate_upper_bound = null인 것처럼
  //    동작합니다.
  // iterate_upper_bound가 null이 아니면, SeekToLast()는 반복자를
  // iterate_upper_bound보다 작은 첫 번째 키로 위치시킵니다.
  //
  // 사용자 정의 타임스탬프가 활성화된 경우, iterate_upper_bound는 타임스탬프
  // 부분이 없는 키를 가리켜야 합니다.
  const Slice* iterate_upper_bound = nullptr;

  // 테일링 반복자를 생성하도록 지정합니다. 테일링 반복자는 데이터베이스 전체에
  // 대한 뷰를 가지며 (즉, 새로 추가된 데이터도 읽을 수 있음) 순차적 읽기를
  // 최적화한 특별한 반복자입니다. 이 반복자는 반복자가 생성된 이후에
  // 데이터베이스에 삽입된 레코드를 반환합니다.
  bool tailing = false;

  // 이 옵션은 더 이상 사용되지 않습니다. 제거된 기능을 켜기 위한
  // 옵션이었습니다. 더 이상 사용되지 않음.
  bool managed = false;

  // 테이블에서 사용된 인덱스 형식(예: 해시 인덱스)에 관계없이 총 순서 검색을
  // 활성화합니다. 일부 테이블 형식(예: 평범한 테이블)은 이 옵션을 지원하지 않을
  // 수 있습니다. Get()을 호출할 때 true이면, 블록 기반 테이블에서 읽을 때
  // 접두사 bloom을 건너뛰며, 이는 Get() 성능에만 영향을 미칩니다.
  bool total_order_seek = false;

  // true일 경우 기본적으로 total_order_seek = true를 사용하고, RocksDB는
  // 검색 키와 반복자 상한선에 따라 결과가 달라지지 않으면 접두사 검색 모드를
  // 선택적으로 활성화할 수 있습니다. 버그:
  // Comparator::IsSameLengthImmediateSuccessor와
  // SliceTransform::FullLengthEnabled를 사용하여
  // 반복자 상한선의 접두사가 검색 키의 접두사와 다를 경우 접두사 모드를
  // 활성화하는 데 결함이 있습니다. DB에 존재하는 경우, "짧은 키"(전체 길이
  // 접두사보다 짧은)는 auto_prefix_mode 반복에서 제외될 수 있으며, 이는
  // total_order_seek 반복에서 나타날 수 있습니다. 이러한 짧은 키가 DB에
  // 추가되지 않았거나 그러한 반복자에서 반환될 것으로 예상되지 않으면 이 문제는
  // 발생하지 않습니다. (새로운 IsSameLengthImmediateSuccessor 조건이 만족된다고
  // 가정합니다. 버그 예시는 DBTest2::AutoPrefixMode1에서 찾을 수 있습니다).
  bool auto_prefix_mode = false;

  // 반복자가 검색한 동일한 접두사만 반복하도록 강제합니다.
  // 이는 반복자 범위가 열고 있는 컬럼 패밀리의 현재 prefix_extractor에
  // 의존하도록 만듭니다. SST 파일이 동일한 접두사 추출기로 생성되면, 접두사
  // 필터링 최적화가 Seek과 SeekForPrev 모두에 사용됩니다.
  bool prefix_same_as_start = false;

  // 반복자가 삭제되지 않는 한 반복자가 로드한 블록을 메모리에 고정시킵니다.
  // BlockBasedTableOptions::use_delta_encoding = false로 생성된 테이블을 읽을
  // 때 사용하면, 반복자의 속성 "rocksdb.iterator.is-key-pinned"가 1을 반환하는
  // 것이 보장됩니다.
  bool pin_data = false;

  // 반복자에 대해, RocksDB는 테이블 파일에 대해 두 번 이상의 순차 읽기가
  // 발생하면 자동으로 리드어헤드를 수행합니다. 사용자가 readahead_size를
  // 제공하지 않으면 리드어헤드는 8KB에서 시작하여 추가적인 읽기마다 두 배씩
  // 증가하여 최대 max_auto_readahead_size에 도달합니다. 단, 읽기가 순차적일
  // 때만 적용됩니다. 그러나 각 레벨에서 반복자가 다음 파일로 이동하면,
  // readahead_size는 다시 8KB에서 시작합니다.
  //
  // 이 옵션을 활성화하면, RocksDB는 데이터를 미리 가져오는 최적화 기능을
  // 제공합니다.
  bool adaptive_readahead = false;

  // true로 설정되면, PurgeObsoleteFile이 CleanupIteratorState에서 호출될 때,
  // 백그라운드 작업을 플러시 작업 큐에 예약하고 백그라운드에서 불필요한 파일을
  // 삭제합니다.
  bool background_purge_on_iterator_cleanup = false;

  // 반복 시, 테이블의 속성을 기반으로 이 스캔에 해당하는 키가 주어진 테이블에
  // 존재하는지 여부를 결정하는 콜백 함수입니다. 콜백은 각 테이블의 속성을
  // 반복할 때마다 전달됩니다. 콜백이 false를 반환하면 해당 테이블은 스캔되지
  // 않습니다. 이 옵션은 반복자에만 영향을 미치며 포인트 조회에는 영향을 미치지
  // 않습니다. 기본값: 빈 값 (모든 테이블이 스캔됩니다)
  std::function<bool(const TableProperties&)> table_filter;

  // auto_readahead_size가 true로 설정되면, 블록 캐시가 활성화된 경우 블록 캐시
  // 데이터를 기반으로 스캔 중에 내부적으로 readahead_size를 자동 조정합니다. 이
  // 옵션은 `iterate_upper_bound != nullptr`와 `prefix_same_as_start == true`일
  // 때만 효과를 봅니다.
  //
  // 블록 캐시를 활성화하는 것 외에도 이 옵션이 적용되려면 `iterate_upper_bound
  // != nullptr` 또는 `prefix_same_as_start == true`이어야 합니다.
  //
  // 구체적으로 다음과 같이 동작합니다:
  // (1) `iterate_upper_bound`가 지정되면, 반복자의 상한을 초과하지 않도록
  // readahead를 잘라냅니다. (2) `prefix_same_as_start`가 true로 설정되면,
  // Seek()의 검색 키와 동일한 접두사에 포함되지 않은 키가 있는
  //     데이터 블록은 미리 가져오지 않도록 readahead를 잘라냅니다.
  //     - 제한 사항: 이 잘라내기 효과가 적용되려면 `Seek(key)`를 호출해야 하며
  //     `SeekToFirst()`는 사용하지 않아야 합니다.
  //
  // 참고: - 이 옵션은 순방향 스캔에만 사용됩니다.
  //       - 역방향 스캔이 있을 경우, 이 옵션은 내부적으로 비활성화되며 순방향
  //       스캔이 다시 발행되더라도 다시 활성화되지 않습니다.
  //
  // 기본값: true
  bool auto_readahead_size = true;

  // 설정되면, 반복자가 다른 항목으로 이동할 때 값을 로드하거나 준비하는 작업을
  // 지연시킬 수 있습니다. (예:
  // SeekToFirst/SeekToLast/Seek/SeekForPrev/Next/Prev 작업 중). 이는 특정 키와
  // 연관된 값이 애플리케이션에서 사용되지 않을 경우 I/O와 CPU 자원을 절약할 수
  // 있습니다. IteratorBase::PrepareValue()도 참조하십시오.
  //
  // 주의: 이 옵션은 현재 1) BlobDB를 사용하여 blob 파일에 저장된 큰 값과 2)
  // 다중 컬럼 패밀리 반복자 (CoalescingIterator 및 AttributeGroupIterator)에만
  // 적용됩니다. 그 외의 경우에는 아무 효과가 없습니다.
  //
  // 기본값: false
  bool allow_unprepared_value = false;

  // *** 반복자나 스캔과 관련된 옵션 끝 ***

  // *** RocksDB 내부 사용 전용 옵션 시작 ***

  // 실험적
  Env::IOActivity io_activity = Env::IOActivity::kUnknown;

  // *** RocksDB 내부 사용 전용 옵션 끝 ***

  ReadOptions() {}
  ReadOptions(bool _verify_checksums, bool _fill_cache);
  explicit ReadOptions(Env::IOActivity _io_activity);
};

// 쓰기 작업을 제어하는 옵션
struct WriteOptions {
  // true로 설정하면, 쓰기 작업이 완료되기 전에 운영 체제의 버퍼 캐시에서
  // (WritableFile::Sync()를 호출하여) 플러시됩니다. 이 플래그가 true이면 쓰기가
  // 더 느려집니다.
  //
  // 이 플래그가 false로 설정되면, 머신이 크래시할 경우 일부 최근 쓰기가 손실될
  // 수 있습니다. 단, 프로세스만 크래시한 경우(즉, 머신이 재부팅되지 않은
  // 경우)에는 sync가 false여도 쓰기가 손실되지 않습니다.
  //
  // 즉, sync가 false인 DB 쓰기는 "write()" 시스템 호출과 유사한 크래시 의미를
  // 가집니다. sync가 true인 DB 쓰기는 "write()" 시스템 호출 뒤에
  // "fdatasync()"를 호출한 것과 유사한 크래시 의미를 가집니다.
  //
  // 기본값: false
  bool sync = false;

  // true로 설정하면, 쓰기 작업이 먼저 쓰기 앞서 로그(WAL)로 가지 않으며,
  // 크래시 후 쓰기가 손실될 수 있습니다. 백업 엔진은 쓰기 앞서 로그를 사용하여
  // memtable을 백업하므로, 쓰기 앞서 로그를 비활성화하면
  // flush_before_backup=true로 백업을 생성해야 미플러시된 memtable 데이터가
  // 손실되지 않습니다. 기본값: false
  bool disableWAL = false;

  // true로 설정하면, 사용자가 존재하지 않는 컬럼 패밀리에 쓰기를 시도할 때,
  // 해당 쓰기를 무시하고(오류를 반환하지 않음) 다른 쓰기는 성공합니다.
  // WriteBatch에 여러 쓰기가 포함되어 있으면 다른 쓰기도 성공합니다.
  // 기본값: false
  bool ignore_missing_column_families = false;

  // true로 설정하면, 쓰기 요청에 대해 대기하거나 잠자기 상태로 전환해야 할 경우
  // 즉시 Status::Incomplete()로 실패합니다.
  // 기본값: false
  bool no_slowdown = false;

  // true로 설정하면, compaction이 뒤처져 있는 경우 이 쓰기 요청의 우선순위가
  // 낮아집니다. 이 경우, no_slowdown = true일 때 요청은 즉시 취소되고
  // Status::Incomplete()가 반환됩니다. 그렇지 않으면, 쓰기는 속도가 느려집니다.
  // 느려짐의 정도는 RocksDB가 결정하여 높은 우선순위 쓰기에 최소한의 영향을
  // 주도록 보장합니다.
  //
  // 기본값: false
  bool low_pri = false;

  // true로 설정하면, 이 WriteBatch는 각 memtable의 마지막 삽입 위치를 힌트로
  // 저장합니다. 이 옵션은 동시에 여러 쓰기가 있을 때 성능을 향상시킬 수
  // 있습니다. 비동기적(memtable_writes가 false일 경우) 쓰기에는 무시됩니다.
  //
  // 기본값: false
  bool memtable_insert_hint_per_batch = false;

  // 이 옵션과 관련된 쓰기 작업에 대해, 내부 rate limiter(참조:
  // `DBOptions::rate_limiter`)가 지정된 우선순위로 과금됩니다. 특별한 값
  // `Env::IO_TOTAL`은 rate limiter 과금을 비활성화합니다.
  //
  // 현재 이 지원은 자동 WAL 플러시를 포함하며, 이는 `WriteOptions::disableWAL
  // == false`이고 `DBOptions::manual_wal_flush == false`일 때 발생합니다.
  //
  // 현재는 `Env::IO_USER`와 `Env::IO_TOTAL`만 허용됩니다.
  //
  // 기본값: `Env::IO_TOTAL`
  Env::IOPriority rate_limiter_priority = Env::IO_TOTAL;

  // `protection_bytes_per_key`는 각 키 항목에 대해 보호 정보를 저장하는 데
  // 사용되는 바이트 수입니다. 현재 지원되는 값은 0 (비활성화)과 8입니다.
  //
  // 기본값: 0 (비활성화).
  size_t protection_bytes_per_key = 0;

  // RocksDB 내부 사용 전용
  //
  // 기본값: Env::IOActivity::kUnknown.
  Env::IOActivity io_activity = Env::IOActivity::kUnknown;

  WriteOptions() {}
  explicit WriteOptions(Env::IOActivity _io_activity);
  explicit WriteOptions(
      Env::IOPriority _rate_limiter_priority,
      Env::IOActivity _io_activity = Env::IOActivity::kUnknown);
};

// 플러시 작업을 제어하는 옵션
struct FlushOptions {
  // true로 설정하면, 플러시가 완료될 때까지 대기합니다.
  // 기본값: true
  bool wait;

  // true로 설정하면, 플러시가 즉시 진행되며, 이로 인해 플러시 동안 쓰기가 일시
  // 중지될 수 있습니다. false로 설정하면, 플러시는 쓰기가 일시 중지되지 않거나
  // 다른(백그라운드 또는 포그라운드 호출) 플러시 작업이 완료될 때까지
  // 대기합니다. 기본값: false
  bool allow_write_stall;

  FlushOptions() : wait(true), allow_write_stall(false) {}
};

// 제공된 DBOptions를 사용하여 Logger를 생성합니다.
Status CreateLoggerFromOptions(const std::string& dbname,
                               const DBOptions& options,
                               std::shared_ptr<Logger>* logger);

// CompactionOptions는 CompactFiles() 호출에서 사용됩니다.
struct CompactionOptions {
  // DEPRECATED: 이 옵션은 사용자가 `CompressionType`을 임의로 설정할 수 있기
  // 때문에 안전하지 않습니다. 항상 `ColumnFamilyOptions`에서 제공된
  // `CompressionOptions`를 사용합니다. 이로 인해 `CompressionType`과
  // `CompressionOptions`가 일관되지 않게 될 수 있습니다.
  //
  // Compaction 출력의 압축 유형
  //
  // 기본값: `kDisableCompressionOption`
  //
  // `kDisableCompressionOption`으로 설정하면, RocksDB는 `ColumnFamilyOptions`에
  // 따라 압축 유형을 선택합니다. RocksDB는 `ColumnFamilyOptions`에 레벨별
  // 설정이 있을 경우 출력 레벨도 고려합니다.
  CompressionType compression;

  // Compaction은 `output_file_size_limit` 크기의 파일을 생성합니다.
  // 기본값: MAX, 즉 compaction은 하나의 파일만 생성합니다.
  uint64_t output_file_size_limit;

  // 0보다 크면, 이 값은 해당 compaction에 대해 DBOptions에서 이 옵션을
  // 대체합니다.
  uint32_t max_subcompactions;

  CompactionOptions()
      : compression(kDisableCompressionOption),
        output_file_size_limit(std::numeric_limits<uint64_t>::max()),
        max_subcompactions(0) {}
};

// 레벨 기반 컴팩션에서는 하위 레벨 컴팩션을 건너뛰거나 강제로 실행할지 여부를
// 설정할 수 있습니다.
enum class BottommostLevelCompaction {
  // 하위 레벨 컴팩션을 건너뜁니다.
  kSkip,
  // 컴팩션 필터가 있을 경우에만 하위 레벨을 컴팩션합니다.
  // 이는 기본 옵션입니다.
  // kForceOptimized와 유사하게, 하위 레벨을 컴팩션할 때 동일한 수동 컴팩션에서
  // 생성된 파일을 중복 컴팩션하지 않도록 합니다.
  kIfHaveCompactionFilter,
  // 항상 하위 레벨을 컴팩션합니다.
  kForce,
  // 항상 하위 레벨을 컴팩션하되, 하위 레벨에서 동일한 컴팩션에서 생성된 파일의
  // 중복 컴팩션을 피합니다.
  kForceOptimized,
};

// 수동 컴팩션에서, blob 파일의 가비지 컬렉션을 건너뛰거나 강제로 실행할지
// 여부를 설정할 수 있습니다.
enum class BlobGarbageCollectionPolicy {
  // blob 파일의 가비지 컬렉션을 강제로 실행합니다.
  kForce,
  // blob 파일의 가비지 컬렉션을 건너뜁니다.
  kDisable,
  // ColumnFamilyOptions에서 blob 파일 가비지 컬렉션 정책을 상속합니다.
  kUseDefault,
};

// CompactRangeOptions는 CompactRange() 호출에서 사용됩니다.
struct CompactRangeOptions {
  // true로 설정하면, 이 수동 컴팩션과 동시에 다른 컴팩션이 실행되지 않습니다.
  // 기본값: false
  bool exclusive_manual_compaction = false;

  // true로 설정하면, 컴팩션된 파일은 데이터를 담을 수 있는 최소 레벨로
  // 이동하거나 주어진 레벨(target_level)로 이동합니다.
  bool change_level = false;
  // change_level이 true이고 target_level이 비음수 값이면, 컴팩션된 파일은
  // target_level로 이동합니다.
  int target_level = -1;
  // 컴팩션 출력은 options.db_paths[target_path_id]에 배치됩니다.
  // target_path_id가 범위를 벗어나면 동작이 정의되지 않습니다.
  uint32_t target_path_id = 0;
  // 기본적으로 레벨 기반 컴팩션은 컴팩션 필터가 있을 경우에만 하위 레벨
  // 컴팩션을 진행합니다.
  BottommostLevelCompaction bottommost_level_compaction =
      BottommostLevelCompaction::kIfHaveCompactionFilter;
  // true로 설정하면, DB가 쓰기 일시 중지 모드에 들어가더라도 즉시 실행됩니다.
  // 그렇지 않으면, 부하가 낮아질 때까지 대기합니다.
  bool allow_write_stall = false;
  // 0보다 크면, 이 값은 해당 compaction에 대해 DBOptions에서 이 옵션을
  // 대체합니다.
  uint32_t max_subcompactions = 0;
  // 사용자 정의 타임스탬프 하한선을 설정합니다. 이 하한선보다 오래된 데이터는
  // 컴팩션에 의해 가비지 컬렉션될 수 있습니다. 기본값: nullptr
  const Slice* full_history_ts_low = nullptr;

  // 진행 중인 수동 컴팩션을 취소할 수 있습니다.
  //
  // `exclusive_manual_compaction == true`와 함께 사용할 경우, 자동 컴팩션이
  // 진행 중이라도 취소가 지연될 수 있습니다.
  std::atomic<bool>* canceled = nullptr;
  // 참고: DisableManualCompaction()을 호출하면 CompactRangeOptions에서 제공된
  // canceled 변수를 덮어씁니다. 일반적으로, CompactRange가 하나의
  // 스레드(t1)에서 canceled = false로 호출되고, DisableManualCompaction이 다른
  // 스레드(t2)에서 호출되면, 수동 컴팩션은 정상적으로 비활성화됩니다. 컴팩션
  // 이터레이터가 몇 개의 항목을 스캔할 수 있지만 *canceled가 true로 설정되기
  // 전까지는 완료되지 않습니다.

  // kForce로 설정되면, RocksDB는 enable_blob_file_garbage_collection을 true로
  // 강제 설정합니다. kDisable로 설정되면, 이를 false로 강제 설정하고,
  // kUseDefault는 설정을 그대로 유지합니다. 이 옵션을 사용하면 CompactRange
  // 호출 시 GC를 강제 활성화하거나 비활성화할 수 있습니다.
  BlobGarbageCollectionPolicy blob_garbage_collection_policy =
      BlobGarbageCollectionPolicy::kUseDefault;

  // 0보다 크면, 사용자 제공 설정을 덮어쓰고, ColumnFamilyOptions의
  // blob_garbage_collection_age_cutoff 설정을 그대로 사용합니다. 이 옵션을
  // 사용하면 고객이 선택적으로 나이 컷오프를 덮어쓸 수 있습니다.
  double blob_garbage_collection_age_cutoff = -1;
};

// IngestExternalFileOptions는 IngestExternalFile()에서 사용됩니다.
struct IngestExternalFileOptions {
  // true로 설정하면 파일을 복사하는 대신 이동합니다.
  // 입력 파일은 성공적으로 삽입된 후에 연결이 끊어집니다.
  // 구현은 전통적인 이동(RenameFile) 대신 하드 링크(LinkFile)를 사용하여
  // 실패 시 원래 상태로 복원될 가능성을 극대화합니다.
  bool move_files = false;

  // move_files와 동일하지만 입력 파일은 연결이 끊어지지 않습니다.
  // `move_files`와 `link_files`는 동시에 설정할 수 없습니다.
  bool link_files = false;

  // 하드 링크가 실패하면 복사로 대체하도록 설정하면 true입니다.
  // 이는 `move_files`와 `link_files` 모두에 적용됩니다.
  bool failed_move_fall_back_to_copy = true;

  // true로 설정하면, 파일을 삽입한 후 기존 스냅샷에 삽입된 파일 키가 나타날 수
  // 없습니다. 파일이 삽입되기 전에 생성된 스냅샷에 나타날 수 없습니다.
  bool snapshot_consistency = true;

  // false로 설정하면, 파일 키 범위가 기존 키 또는 tombstone과 겹치거나
  // 진행 중인 컴팩션의 출력과 겹치는 경우 IngestExternalFile()가 실패합니다.
  // (이 조건에서 전역 seqno를 삽입된 파일에 할당해야 합니다).
  bool allow_global_seqno = true;

  // false로 설정하고 파일 키 범위가 memtable 키 범위와 겹치면
  // (memtable 플러시 필요), IngestExternalFile은 실패합니다.
  bool allow_blocking_flush = true;

  // 삽입된 파일에 중복된 키가 있으면 해당 키를 덮어쓰는 대신 건너뛰도록
  // 설정하면 true입니다. 사용 사례: 기존 데이터는 덮어쓰지 않고 데이터베이스에
  // 일부 이력 데이터를 백필할 때 사용됩니다. 이 옵션은 DB가
  // allow_ingest_behind=true로 실행된 상태에서만 사용할 수 있습니다. 모든
  // 파일은 seqno=0으로 하위 레벨에 삽입됩니다.
  bool ingest_behind = false;

  // DEPRECATED - 삽입 시 외부 SST 파일에 global_seqno를 작성하려면 true로
  // 설정합니다. 이는 RocksDB 5.16.0 이전의 호환성을 위해서 사용됩니다. 구버전
  // RocksDB는 global_seqno가 DB 매니페스트에 기록되는 대신 SST 파일에
  // 작성되기를 기대합니다. 이 기능은 (a) 랜덤 쓰기가 일부 파일 시스템에서 비쌀
  // 수 있거나 지원되지 않을 수 있고, (b) 이러한 쓰기로 인해 파일 체크섬이
  // 변경되므로 deprecated 되었습니다.
  bool write_global_seqno = false;

  // 외부 SST 파일을 삽입하기 전에 각 블록의 체크섬을 확인하려면 true로
  // 설정합니다. 경고: 이것을 true로 설정하면 외부 SST 파일을 읽어야 하기 때문에
  // 파일 삽입 속도가 느려집니다.
  bool verify_checksums_before_ingest = false;

  // verify_checksums_before_ingest = true일 때 RocksDB는 기본 readahead 설정을
  // 사용하여 파일을 스캔하면서 체크섬을 확인합니다. 사용자는 이 옵션을 사용하여
  // 기본값을 재정의할 수 있습니다. 큰 readahead 크기(> 2MB)는 회전하는
  // 디스크에서 순차적 반복 성능을 향상시킬 수 있습니다.
  size_t verify_checksums_readahead_size = 0;
  // 사용자 설정에 따라, 삽입된 파일의 SST 파일 체크섬을 검증하려면 TRUE로
  // 설정합니다. DB의 체크섬 함수는 삽입된 각 파일의 체크섬을 생성하고, 체크섬
  // 함수 이름과 체크섬을 삽입된 체크섬 정보와 비교합니다.
  //
  // 이 옵션이 True로 설정되면: 1) DB가 체크섬을 활성화하지 않으면
  // (file_checksum_gen_factory == nullptr) 삽입된 체크섬 정보는 무시됩니다;
  // 2) DB가 체크섬 함수를 활성화하면, 파일이 이동하거나 복사된 후 SST 파일
  // 체크섬을 계산하고 체크섬과 체크섬 함수 이름을 비교합니다. 체크섬이나 체크섬
  // 함수 이름이 일치하지 않으면 삽입이 실패합니다. 검증이 성공하면 체크섬과
  // 체크섬 함수 이름이 매니페스트에 저장됩니다. 이 옵션이 FALSE로 설정되면: 1)
  // DB가 체크섬을 활성화하지 않으면 삽입된 체크섬 정보는 무시됩니다; 2) DB가
  // 체크섬을 활성화하면, 우리는 삽입된 체크섬 함수 이름만 검증하고 삽입된
  // 체크섬을 신뢰합니다. 체크섬 함수 이름이 일치하면, 매니페스트에 체크섬을
  // 저장합니다. 그러나 삽입된 파일에 체크섬 정보가 제공되지 않으면 DB는
  // 체크섬을 생성하여 매니페스트에 저장합니다.
  bool verify_file_checksum = true;

  // 사용자가 파일을 마지막 레벨에 삽입하고 싶으면 TRUE로 설정합니다.
  // DB::IngestExternalFile()/DB::IngestExternalFiles()를 호출할 때 파일이
  // 마지막 레벨에 맞지 않으면 Status::TryAgain() 오류가 반환됩니다. 사용자는
  // 재시도 전에 겹치는 범위에서 마지막 레벨을 지워야 합니다.
  //
  // ingest_behind는 fail_if_not_bottommost_level보다 우선합니다.
  //
  // XXX: "bottommost"는 마지막 레벨을 지칭하는 혼란스러운/구식 용어입니다.
  bool fail_if_not_bottommost_level = false;

  // EXPERIMENTAL
  // SstFileWriter로 생성되지 않은 파일의 삽입을 활성화합니다. true로 설정하면:
  // - 삽입하려는 CF와 CF ID가 일치하지 않는 파일의 삽입을 허용합니다.
  // 요구 사항:
  // - 삽입된 파일이 기존 키와 겹치지 않아야 합니다.
  // - `write_global_seqno`는 false여야 합니다.
  // - 삽입된 파일의 모든 키는 시퀀스 번호가 0이어야 합니다. 하나라도 시퀀스
  // 번호가 0이 아니면 삽입이 실패합니다. 경고: DB가 다른 DB/CF에서 생성된
  // 삽입된 파일을 포함하고 있으면, RepairDB()는 이 파일을 올바르게 복구하지
  // 못할 수 있어 데이터 손실이 발생할 수 있습니다.
  bool allow_db_generated_files = false;

  // 파일 삽입 중에 데이터와 메타데이터 블록(예: 인덱스, 필터)을 읽을 때
  // 블록 캐시에 추가될지 여부를 제어합니다.
  // 읽을 수 없는 CF로 대량 로딩할 때 이 옵션을 false로 설정할 수 있습니다.
  // 여러 CF로 삽입할 때 이 옵션은 삽입 옵션 간에 동일해야 합니다.
  bool fill_cache = true;
};

enum TraceFilterType : uint64_t {
  // 모든 작업을 추적합니다.
  kTraceFilterNone = 0x0,
  // get 작업을 추적하지 않습니다.
  kTraceFilterGet = 0x1 << 0,
  // write 작업을 추적하지 않습니다.
  kTraceFilterWrite = 0x1 << 1,
  // `Iterator::Seek()` 작업을 추적하지 않습니다.
  kTraceFilterIteratorSeek = 0x1 << 2,
  // `Iterator::SeekForPrev()` 작업을 추적하지 않습니다.
  kTraceFilterIteratorSeekForPrev = 0x1 << 3,
  // `MultiGet()` 작업을 추적하지 않습니다.
  kTraceFilterMultiGet = 0x1 << 4,
};

// TraceOptions는 StartTrace에 사용되는 옵션 구조체입니다.
struct TraceOptions {
  // 트레이스 파일 크기가 저장소 공간보다 커지는 것을 방지하기 위해,
  // 사용자는 최대 트레이스 파일 크기를 바이트 단위로 설정할 수 있습니다.
  // 기본값은 64GB입니다.
  uint64_t max_trace_file_size = uint64_t{64} * 1024 * 1024 * 1024;

  // 트레이스 샘플링 옵션을 설정합니다. 즉, 몇 개의 요청당 하나씩 캡처할지
  // 지정합니다. 기본값은 1 (모든 요청을 캡처)입니다.
  uint64_t sampling_frequency = 1;

  // 필터링은 샘플링 전에 발생합니다.
  uint64_t filter = kTraceFilterNone;

  // true로 설정하면, 트레이스 내의 쓰기 기록 순서가 WAL에 있는 순서와
  // 일치하도록 보장됩니다. 이 순서를 보존하는 데 성능 저하가 있을 수 있습니다.
  //
  // 기본값: false. 이 경우 트레이스 내 쓰기 기록 순서는 WAL의 순서와 다를 수
  // 있습니다.
  bool preserve_write_order = false;
};

// ImportColumnFamilyOptions는 ImportColumnFamily()에 사용됩니다.
struct ImportColumnFamilyOptions {
  // 파일을 복사하는 대신 이동하려면 true로 설정합니다.
  bool move_files = false;
};

// DB::GetApproximateSizes()와 함께 사용되는 옵션입니다.
struct SizeApproximationOptions {
  // 반환된 크기에 최근에 기록된 메모리 테이블 데이터를 포함할지 여부를
  // 정의합니다. false로 설정되면, include_files는 true여야 합니다.
  bool include_memtables = false;

  // 반환된 크기에 디스크에 직렬화된 데이터를 포함할지 여부를 정의합니다.
  // false로 설정되면, include_memtables는 true여야 합니다.
  bool include_files = true;

  // DB::GetApproximateSizes를 사용하여 키 범위를 저장하는 데 사용되는 파일의 총
  // 크기를 근사할 때, 파일 크기 오류 한도 내에서 근사할 수 있도록 허용합니다.
  // 이는 파일 크기 근사에서 일부 단축을 허용하여 더 나은 성능을 보장하면서
  // 결과 오류가 합리적인 범위 내에 있도록 합니다.
  // 예를 들어, 값이 0.1이면 반환된 파일 크기 근사의 오류 한도는 10% 이내입니다.
  // 값이 0보다 작거나 같으면 더 정확하지만 CPU 집약적인 추정이 수행됩니다.
  double files_size_error_margin = -1.0;
};

struct CompactionServiceOptionsOverride {
  Env* env = Env::Default();
  std::shared_ptr<FileChecksumGenFactory> file_checksum_gen_factory = nullptr;

  const Comparator* comparator = BytewiseComparator();
  std::shared_ptr<MergeOperator> merge_operator = nullptr;
  const CompactionFilter* compaction_filter = nullptr;
  std::shared_ptr<CompactionFilterFactory> compaction_filter_factory = nullptr;
  std::shared_ptr<const SliceTransform> prefix_extractor = nullptr;
  std::shared_ptr<TableFactory> table_factory;
  std::shared_ptr<SstPartitionerFactory> sst_partitioner_factory = nullptr;

  // Only subsets of events are triggered in remote compaction worker, like:
  // `OnTableFileCreated`, `OnTableFileCreationStarted`,
  // `ShouldBeNotifiedOnFileIO` `OnSubcompactionBegin`,
  // `OnSubcompactionCompleted`, etc. Worth mentioning, `OnCompactionBegin` and
  // `OnCompactionCompleted` won't be triggered. They will be triggered on the
  // primary DB side.
  std::vector<std::shared_ptr<EventListener>> listeners;

  // statistics is used to collect DB operation metrics, the metrics won't be
  // returned to CompactionService primary host, to collect that, the user needs
  // to set it here.
  std::shared_ptr<Statistics> statistics = nullptr;

  // Only compaction generated SST files use this user defined table properties
  // collector.
  std::vector<std::shared_ptr<TablePropertiesCollectorFactory>>
      table_properties_collector_factories;
};

struct OpenAndCompactOptions {
  // Allows cancellation of an in-progress compaction.
  std::atomic<bool>* canceled = nullptr;
};

struct LiveFilesStorageInfoOptions {
  // Whether to populate FileStorageInfo::file_checksum* or leave blank
  bool include_checksum_info = false;
  // Flushes memtables if total size in bytes of live WAL files is >= this
  // number (and DB is not read-only).
  // Default: always force a flush without checking sizes.
  uint64_t wal_size_for_flush = 0;
};

struct WaitForCompactOptions {
  // A boolean to abort waiting in case of a pause (PauseBackgroundWork()
  // called) If true, Status::Aborted will be returned immediately. If false,
  // ContinueBackgroundWork() must be called to resume the background jobs.
  // Otherwise, jobs that were queued, but not scheduled yet may never finish
  // and WaitForCompact() may wait indefinitely (if timeout is set, it will
  // expire and return Status::TimedOut).
  bool abort_on_pause = false;

  // A boolean to flush all column families before starting to wait.
  bool flush = false;

  // A boolean to wait for purge to complete
  bool wait_for_purge = false;

  // A boolean to call Close() after waiting is done. By the time Close() is
  // called here, there should be no background jobs in progress and no new
  // background jobs should be added. DB may not have been closed if Close()
  // returned Aborted status due to unreleased snapshots in the system. See
  // comments in DB::Close() for details.
  bool close_db = false;

  // Timeout in microseconds for waiting for compaction to complete.
  // Status::TimedOut will be returned if timeout expires.
  // when timeout == 0, WaitForCompact() will wait as long as there's background
  // work to finish.
  std::chrono::microseconds timeout = std::chrono::microseconds::zero();
};

}  // namespace ROCKSDB_NAMESPACE
