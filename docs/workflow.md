# 크로스 플랫폼 워크플로

이 repository가 두 기기에서 개발되는 방식:

- **외부 (Ubuntu)** — 코드 편집, 문서화, git 작업, 프로토콜 설계
- **집 (Windows + STM32CubeIDE)** — `.ioc` 설정, 빌드, 플래시, 디버그

두 기기 모두 동일한 GitHub remote에서 pull하고 push한다.

---

## 황금 규칙

1. **한 번에 한 기기만.** 동시에 양쪽에서 편집하지 않는다. 작업 전에 `git pull`, 작업 후에 `git push`.
2. **`.ioc`는 CubeIDE 전용.** 텍스트 에디터로 `.ioc` 파일을 편집하지 않는다. 집 기기의 CubeMX/CubeIDE에서만 연다.
3. **사용자 코드는 `USER CODE` 블록 또는 별도 파일에.** CubeIDE는 모든 `.ioc` 저장 시 `main.c`와 HAL init 파일을 재생성한다. `/* USER CODE BEGIN */ ... /* USER CODE END */` 블록 밖의 코드는 삭제된다.
4. **중요하지 않은 로직에는 USER CODE 블록보다 별도의 `app_*.c/h` 파일을 선호한다.** 별도 파일은 번거로움 없이 재생성에서 살아남는다.

---

## 어디서 무엇을 편집할지

### Ubuntu (외부)에서 안전하게 편집 가능

- `Core/Src/main.c` — `USER CODE` 블록 안에서만
- `Core/Inc/main.h` — `USER CODE` 블록 안에서만
- `Core/Src/app_*.c`, `Core/Inc/app_*.h` — 별도 파일의 application code
- `Core/Src/freertos.c` — `USER CODE` 블록 안에서만
- `rpi/` 아래의 모든 Python 코드
- `docs/`, `README.md` 아래의 모든 문서
- `.gitignore`, `.gitattributes`, 이 파일

### Windows (집, CubeIDE)에서 편집

- `*.ioc` 파일 — 핀 할당, clock 설정, peripheral 설정
- 디버거 실행이 필요한 코드 변경
- 하드웨어에 최종 플래시 (`.bin`을 NODE_F446RE 드라이브로 drag & drop)

---

## 표준 세션 흐름

### 외부 (Ubuntu) 세션

```bash
cd ~/projects/multi-mcu-can
git pull origin main          # 항상 먼저 pull
# ... 코드 편집, 문서 작성 ...
git status                    # 변경사항 검토
git add -A
git commit -m "phase2: add CAN message dispatcher skeleton"
git push origin main
```

### 집 (Windows + CubeIDE) 세션

```
1. Git Bash 또는 CubeIDE의 git plugin 열기
2. git pull origin main
3. CubeIDE에서 프로젝트 열기
4. 다른 기기에서 .ioc가 변경되었으면, CubeIDE가 재생성 여부를 묻는다
5. 빌드 → 플래시 → 디버그
6. 코드 또는 .ioc를 수정했다면:
   git add -A
   git commit -m "..."
   git push origin main
```

---

## 가장 흔한 두 가지 실수 피하기

### 실수 1: 텍스트 에디터로 `.ioc` 편집

`.ioc` 파일은 INI 형식 텍스트라 손이 가기 쉽다. **하지 말 것.** CubeMX는 직접 편집 시 깨지는 내부 cross-reference와 checksum 같은 일관성을 유지한다. 증상은 "MX_*_Init()이 재생성되지 않음"부터 전체 프로젝트 corruption까지 다양하다.

**해결책:** Peripheral 변경이 필요하고 CubeIDE를 사용할 수 없다면, `docs/todo.md`에 TODO 메모를 작성하고 집 기기에서 처리한다.

### 실수 2: USER CODE 블록 밖의 코드

```c
/* USER CODE BEGIN Includes */
#include "app_can.h"      // ✅ 재생성에서 살아남음
/* USER CODE END Includes */

#include "stm32f4xx_hal.h"
#include "my_helper.h"    // ❌ USER CODE 블록 밖 — 다음 재생성 시 삭제됨
```

**해결책:** 사용자 추가 내용이 `BEGIN` / `END` 마커 사이에 있는지 항상 확인한다. 더 나은 방법: 중요하지 않은 코드는 `app_*.c/h` 파일에 두고 USER CODE 블록 안에서만 호출한다.

---

## 선택 사항: Ubuntu에서 빌드

집 기기 없이 컴파일을 검증하려면:

```bash
sudo apt install gcc-arm-none-eabi
# 그런 다음:
#   - CubeIDE의 내보낸 Makefile 사용 (Project Properties → C/C++ Build → ...),
#   - 또는 arm-none-eabi-gcc를 대상으로 하는 CMakeLists.txt 작성.
```

이를 통해 Ubuntu에서 compile error와 `.bin` output을 얻을 수 있다. 플래시와 디버그에는 Ubuntu에서 `openocd` + ST-Link 드라이버를 설정하지 않는 한 집 기기가 여전히 필요하다.

**권장:** 위의 워크플로가 편해질 때까지 이를 건너뛴다. 기본 git flow가 안정적이기 전에 두 번째 toolchain을 추가하는 것은 이 repository가 피하도록 구성된 "한 번에 두 개의 새 레이어" anti-pattern 그 자체다.

---

## 문제 발생 시

### "CubeIDE가 재생성된 파일에서 merge conflict를 표시한다"

재생성된 파일들(`main.c` 등)이 두 기기에서 약간 다르게 재생성되어 USER CODE 외부 섹션이 다를 수 있다. **해결 방법:** 집 기기 버전을 유지하고 (방금 CubeMX를 실행했으므로), 손실된 USER CODE 추가 내용이 있다면 재적용한다. 번거롭지만 복구 가능하다. 예방책: Ubuntu 쪽에서는 재생성하지 않는다 (어차피 `.ioc`를 열면 안 된다).

### "Line ending 지옥"

변경사항 없이 전체 파일이 변경된 것으로 표시되면, CRLF/LF 문제다. `.gitattributes` 파일이 이를 방지해야 한다. 한 번 발생했다면:

```bash
git rm --cached -r .
git reset --hard
```

이렇게 하면 새로운 normalization rule이 적용된 상태로 모든 것이 다시 체크아웃된다.

### "`.elf` / `Debug/` 폴더를 커밋했다"

```bash
git rm -r --cached Debug/ *.elf
git commit -m "remove build artifacts"
```

`.gitignore`가 다음 번에는 이를 방지한다.
