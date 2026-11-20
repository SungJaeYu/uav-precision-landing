# macOS 개발 환경 설정·검증 — 2026-11-20

현재 README의 M1 C++ 판단 데모를 이 Mac에서 개발·빌드·검증할 수 있도록 설정했다. 제품 C++ 코드와 기존 테스트는 변경하지 않았다. Linux 실행 환경과 PX4/Gazebo 영상·비행 연결은 이번 로컬 검증 범위에 포함되지 않는다.

## 확인 환경

| 항목 | 실제 확인 결과 |
| --- | --- |
| 작업 폴더 | `/Users/yuseungjae/Desktop/uav-precision-landing` |
| 기준 브랜치·커밋 | `main`, `1630e52bc19e2589e0366573890616bf818d9298` |
| origin | `git@github.com:SungJaeYu/uav-precision-landing.git` |
| Git 연결 | SSH fetch 성공, 전체 이력 수신, `HEAD == origin/main`, `git fsck --full` 통과 |
| OS / 아키텍처 | macOS 26.6.2 (25G83), ARM64 |
| Xcode | 27.2 (27B52) |
| 컴파일러 | Apple Clang 21.0.0 (`clang-2100.4.1.1`), C++20 |
| 빌드 도구 | CMake 4.3.3, Ninja 1.13.2 |
| Git | 2.54.0 (Apple Git-157) |
| VS Code 확장 | CMake Tools 1.25.10, C/C++ 1.35.6 설치 확인 |

설정 중 Xcode 라이선스 미동의로 기본 Git·컴파일러가 실행되지 않았다. 사용자가 실제 터미널에서 라이선스에 동의한 뒤 기본 `git`, `clang++`, SDK 검색과 `xcodebuild -checkFirstLaunchStatus`가 정상 실행되는 것을 확인했다.

## 추가한 설정

- `CMakePresets.json`: `release`, `debug`, `sanitize`의 configure/build/test 설정. CMake 3.20에서 지원하는 schema version 2를 사용한다.
- 모든 preset에서 Ninja, C++ 컴파일 명령 데이터베이스 생성, 기존 테스트 실행을 사용한다. 테스트가 하나도 없으면 성공 처리하지 않는다.
- `sanitize`는 기존 `LANDING_SANITIZERS` 옵션으로 ASan/UBSan을 활성화하고, UBSan 오류 발생 시 실패하도록 설정한다.
- `.vscode/settings.json`: CMake preset 사용, 폴더를 열 때 configure, CMake Tools를 통한 IntelliSense 설정 제공.
- `.vscode/extensions.json`: CMake Tools와 C/C++ 확장 권장 목록.

## 실행 방법

저장소 폴더에서 실행한다.

```sh
cd /Users/yuseungjae/Desktop/uav-precision-landing
cmake --preset release
cmake --build --preset release
ctest --preset release
./build/landing_demo
```

디버그용 빌드:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build-debug/landing_demo
```

메모리 접근 오류·정의되지 않은 동작 검사:

```sh
cmake --preset sanitize
cmake --build --preset sanitize
ctest --preset sanitize
./build-sanitize/landing_demo
```

README의 기존 `cmake -S . -B build -G Ninja ...` 명령도 그대로 사용할 수 있다. 빌드 디렉터리와 그 안의 로그는 기존 `.gitignore`의 `/build*/` 규칙으로 제외된다.

VS Code에서 이 폴더를 열고 `CMake: Select Configure Preset`에서 `debug`를 선택한다. CMake configure 후 `CMake: Build`로 빌드하고 CMake/CTest 테스트를 실행할 수 있다. IntelliSense는 선택된 빌드의 실제 컴파일 설정을 사용한다. 이번에는 설정과 확장 설치, compile_commands 생성까지 확인했으며 GUI 디버거 세션은 실행하지 않았다.

## 실제 검증 결과

| 설정 | 빌드 | 기존 CTest | 데모 |
| --- | --- | --- | --- |
| Release | 성공 | 12/12 통과 | 11개 합성 입력 결과가 `docs/cloud-m1.md`의 CSV와 완전 일치 |
| Debug | 성공 | 12/12 통과 | 같은 CSV와 완전 일치 |
| Debug + ASan/UBSan | 성공 | 12/12 통과 | 같은 CSV와 완전 일치 |

같은 12개 테스트를 세 설정에서 실행한 결과다. `-std=c++20`, `-Werror`와 sanitizer 빌드의 `-fsanitize=address,undefined`를 실제 컴파일 명령에서 확인했다. 생성된 실행 파일은 ARM64이며 sanitizer 실행 파일의 ASan 런타임 연결도 확인했다. 별도의 누수 검사 통과를 의미하지 않는다.

로컬 원시 결과:

- `build/Testing/Temporary/LastTest.log`, `build/demo.csv`
- `build-debug/Testing/Temporary/LastTest.log`, `build-debug/demo.csv`
- `build-sanitize/Testing/Temporary/LastTest.log`, `build-sanitize/demo.csv`
- 각 빌드 디렉터리의 `compile_commands.json`

## 다음 단계와 검증 범위

현재 데모에는 OpenCV, MAVSDK, PX4, Gazebo, ROS 2가 필요하지 않으므로 이번 설정에서 추가하지 않았다. 카메라·기체에 연결하거나 실제 비행 요청을 보내지 않았다. 이 결과는 합성 입력을 사용하는 Mac의 판단 로직 실행 증거이며, Linux 재현·영상 처리·폐루프 착륙·실장비 성능을 증명하지 않는다.

M2/M3를 시작할 때 PX4 release/commit, Gazebo 버전, 하방 카메라 모델·토픽·시각 정보를 선택하고 실제 실행을 검증해야 한다. 확인 당시 PX4 공식 `main` 문서는 Apple Silicon macOS 환경과 Gazebo Harmonic 설치 경로를 안내한다. 이는 설치 가능 경로에 관한 문서 확인이며 이 Mac에서의 SITL 구동 성공 기록은 아니다. README의 Linux 목표를 유지할 경우 별도의 Linux 환경 검증도 필요하다.

참고한 공식 문서:

- [CMake 3.20 presets](https://cmake.org/cmake/help/v3.20/manual/cmake-presets.7.html)
- [VS Code CMake 설정](https://code.visualstudio.com/docs/cpp/cmake-quickstart)
- [PX4 macOS 개발 환경](https://docs.px4.io/main/en/dev_setup/dev_env_mac)
- [PX4 Gazebo 시뮬레이션](https://docs.px4.io/main/en/sim_gazebo_gz/)
