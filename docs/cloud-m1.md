# 첫 클라우드 실행 — 2026-11-19

## 확인한 범위

맥북·STM32 없이 클라우드의 Linux에서 C++ 프로그램을 빌드하고 실행했다. 이번 단계는 상태 판단 로직을 구현·검증하는 작은 출발점이다. Linux 컨테이너 안의 성공이 실제 임베디드 장비의 성능·I/O 검증을 대신하지 않는다.

| 항목 | 확인 환경·결과 |
| --- | --- |
| OS / 아키텍처 | Debian GNU/Linux 13 (13.6), x86_64 |
| 컴파일러 | GCC 14.2.0 (`Debian 14.2.0-19`), C++20 |
| 빌드 | CMake 4.4.5, Ninja 1.13.2.git.kitware.jobserver-pipe-1 |
| Release | 빌드 성공, CTest 12/12 통과 |
| Debug + ASan/UBSan | 빌드 성공, 누수 검사 제외 후 CTest 12/12 통과 |
| 데모 | 합성 입력 11개, 정상→지연→소실→복귀→재전송→비활성화 |
| 외부 연결 | 없음. stdout 출력만 사용 |

Release에서도 테스트 판정은 `assert`에 의존하지 않아 비활성화되지 않는다. ASan의 메모리 접근 검사와 UBSan을 켜서 재실행했으며, 클라우드 환경에서 LeakSanitizer는 `does not work under ptrace` 오류로 동작하지 않았다. `detect_leaks=0`으로 누수 검사만 제외했다. 누수 검사 통과를 주장하지 않는다. 최초 테스트 빌드 중 전역 함수명 `clock`이 표준 라이브러리와 충돌하여 `clock_bounds`로 고쳤다.

## 프로그램이 하는 일

`Manager::step()`은 현재 시각, 새 관측 하나 또는 관측 없음, 임무 활성 플래그를 받는다. 상태, 명령 의도, 판단 이유, 근거 관측 ID, 유효 시간 구간을 반환한다. 데모가 이 결과를 출력한다.

| 입력/출력 | M1 계약 |
| --- | --- |
| 위치 | 기체 FRD 좌표의 패드 상대 거리: 전방·우측, 단위 m. 영상 좌표 변환은 아직 없음 |
| 시각 | 하나의 합성 단조 시계, 정수 ms. 획득 시각과 판단 시각의 clock domain이 같다고 가정 |
| 관측 순서 | 유효하게 접수한 ID와 획득 시각보다 모두 커야 함. 순환·worker epoch는 후속 |
| 관측 신선도 | 나이 0 이상, **200ms 미만**. 정확히 200ms부터 거부 |
| 정렬 | 수평 거리 `hypot(forward, right)`가 **0.2m 이하**, 연속 새 관측 3개 필요 |
| 연속성 | 거부·관측 없음·비활성화·정렬 이탈 시 횟수 초기화. 호출/획득 간격 200ms 이상도 초기화 |
| 명령 의도 | HOLD / ALIGN_HORIZONTAL / DESCEND_INTENT. 실제 속도·자세·비행 명령이 아님 |
| 명령 만료 | 생성 후 최대 100ms, 근거 관측이 만료되는 시각을 넘지 않음. 유효 구간은 시작 포함·끝 제외 |
| 임무 활성 | 호출자가 제공하는 합성 bool. 실제 기체 연결·고도·비행 모드의 적합성 판정은 아직 없음 |

위 숫자는 동작과 경계 시험을 위한 **데모 설정**이다. 실제 기체의 제어 주기·착륙 허용 오차·안전 요구에서 도출한 값이 아니다.

부적합한 관측에서는 이전 하강 의도를 보존하지 않고 HOLD 판단을 반환한다. 거부 응답의 `expires_ms == generated_ms`는 유효한 이동 의도가 없다는 뜻이다. 화면의 HOLD는 실제 기체가 호버링했다는 뜻이 아니다.

새 관측이 없는 주기에는 호출자가 `nullopt`로 `step()`을 호출해야 한다. 이 클래스는 타이머나 송신 스레드를 실행하지 않는다. 호출 중단 시 실제 명령을 해제하는 watchdog·만료 소비·차량 adapter 동작은 후속 연결에서 구현해야 한다. 동일 관측을 여러 제어 tick에서 재사용하는 interface도 이번 초안에는 없다.

## 실제 데모 출력

```csv
scenario,sensor_ms,state,intent,reason,source_id,expires_ms
pad_right,1000,ALIGN,ALIGN_HORIZONTAL,offset,1,1100
center_1,1100,ALIGN,HOLD,confirming_alignment,2,1200
center_2,1200,ALIGN,HOLD,confirming_alignment,3,1300
center_3,1300,DESCEND,DESCEND_INTENT,aligned,4,1400
delayed_frame,1500,HOLD,HOLD,stale,,1500
observation_lost,1600,HOLD,HOLD,missing,,1600
recovered_1,1700,ALIGN,HOLD,confirming_alignment,6,1800
recovered_2,1800,ALIGN,HOLD,confirming_alignment,7,1900
recovered_3,1900,DESCEND,DESCEND_INTENT,aligned,8,2000
replayed_frame,1950,HOLD,HOLD,repeated_or_reversed_id,,1950
mission_disabled,2000,HOLD,HOLD,disabled,,2000
```

세 번의 새 중심 관측 후 하강 의도를 내고, 지연·소실에서 보류한다. 관측 복귀 직후 바로 하강하지 않고 다시 세 번 확인한다. 행의 시각은 입력 데이터이며 실행 지연을 측정한 수치가 아니다.

## 테스트

| CTest 이름 | 검사 |
| --- | --- |
| normal | 정렬 요청, 중심 1·2·3회 전이, 근거 ID, 정상 지속 |
| recovery | 하강 의도 중 관측 소실·지연, 복귀 후 재확인 |
| freshness | 나이 0/199/200/201ms, 미래·음수 획득 시각 |
| duplicate | 동일·역행 ID가 새 관측으로 집계되지 않는지 |
| out_of_order | ID만 새롭고 획득 시각이 같거나 과거인 관측 |
| invalid | 두 위치축 각각 NaN·양/음의 무한대, valid=false |
| alignment | 두 축·양/음 부호·반경 경계와 인접 float, 대각선 거리, 이탈 후 재확인 |
| disabled | 활성 해제, 이전 관측 재사용 차단, 재활성 시 재확인 |
| clock | 음수 현재 시각·역행·int64 상한과 인접값, 덧셈 overflow 방지 |
| expiry | 출력 의도 만료가 근거 관측보다 늦어지지 않는지 |
| initial_loss | 처음부터 관측 없음, 최초 관측에서 하강하지 않는지 |
| gap | 호출/획득 간격 경계, 긴 공백 후 기존 정렬 횟수 재사용 방지 |

저장소 루트에서 일반 빌드·실행은 [README](../README.md#실행-방법)를 따른다. 추가 검사는 다음과 같다.

```sh
cmake -S . -B build-sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLANDING_SANITIZERS=ON
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
```

이번 클라우드처럼 LeakSanitizer가 ptrace 오류로 실행되지 않는 경우에만 누수 검사를 제외한 결과를 따로 기록한다.

```sh
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-sanitize --output-on-failure
```

## 다음 연결과 한계

- 구현: 상태 판단 library, 고정 합성 입력 demo, 경계·실패 대응 테스트. AI 코딩 보조로 만든 초안이며 사용자 직접 구현 학습 완료와 구분한다.
- 미구현: 카메라·영상 인식·좌표 변환, 가짜/실제 차량 adapter, 수치 명령, 접근·착륙 확인·중단을 포함한 전체 상태 전이, 비행 모드/고도/연결 확인.
- 미검증: PX4/Gazebo 구동, 실제 영상 폐루프, Linux ARM/M1, 임베디드 보드·GPU·전력·발열·실시간 성능.
- 다음 작은 단위: 실제 영상 연결에 앞서 PX4/Gazebo 실행 가능 환경과 버전을 확인한다. 이 판단기를 실제 차량에 연결할 때에는 관측 수신과 주기 제어를 분리하고, 의도 만료·기체 상태·보류/중단 정책을 명시한다.
