# OSTEP Ch 14 — Interlude: Memory API 정리

`malloc`/`free`의 정확한 동작, 흔한 메모리 버그, 그리고 진단 도구(gdb/valgrind/ASan).

---

## 스택 vs 힙

| | 스택(automatic) | 힙(manual) |
|--|------|------|
| 할당/해제 | **컴파일러가 자동** (함수 진입/종료) | **내가 malloc/free** |
| 수명 | 함수 끝나면 **사라짐** | free할 때까지 유지 |
| 크기 한도 | **8MB 기본** (`ulimit -s`) | 사실상 주소 공간 전체 |
| 용도 | 지역변수, 작은 배열 | 크거나 오래 살아야 할 데이터 |

- **큰 배열을 지역변수로 잡지 말 것** → 스택 오버플로우. malloc으로.
- **스택 한도가 작은 이유**: 무한 재귀가 시스템을 잡아먹지 않게 + 스레드마다 스택이 필요해서.
- **오래 유지할 데이터는 힙에** — reverse/wish에서 `strdup`한 이유. (스택/재사용 버퍼를 가리키면 dangling)

## 흔한 메모리 버그

| 버그 | 설명 | UB? |
|------|------|-----|
| **dangling / use-after-free** | 해제·소멸된 메모리를 가리키는 포인터 사용 | UB |
| **double free** | 같은 블록 두 번 free | UB |
| **buffer overflow** | 할당 범위 밖 접근 | UB |
| **uninitialized read** | malloc 후 초기화 없이 읽기 (calloc은 0으로 채움) | UB |
| **memory leak** | free 안 함 | **UB 아님** (낭비일 뿐) |

**double free가 위험한 이유:** `free`는 데이터를 **지우는 게 아니라** 할당자 장부(free list)에 "자유" 표시. 이미 자유인 걸 또 넣으면 **free list가 꼬여** → 나중에 같은 블록을 두 번 할당하거나 크래시. 힙 익스플로잇의 단골.
- `free(NULL)`은 **안전**(표준이 no-op으로 정의). → `free(p); p = NULL;` 습관이 방어책.

**컴파일러가 막아주나?**
- **막는 것**: 문법·타입 오류 등 **정적으로 확인 가능한 것**만.
- **경고**(-Wall): `=` vs `==`, 초기화 안 된 변수 등. `-Werror`로 에러 승격.
- **못 막는 것**: 메모리 버그 **전부**(런타임에 결정되니까). → C의 철학(검사 생략 = 속도). Rust는 이걸 컴파일 타임 소유권으로 막음.

## malloc의 내부 (brk / mmap)

`malloc`은 **시스템콜이 아니라 libc 함수.** 실제 메모리는 커널에 요청:
- **`brk`/`sbrk`**: 힙의 끝(break)을 밀어 힙을 연속 확장. **직접 호출 금지**(libc 장부가 깨짐).
- **`mmap`**: 주소 공간에 **독립된 영역**을 새로 매핑. 큰 할당(보통 128KB↑)에 사용.

**전략:** 작은 할당은 힙(brk)에서 잘라 쓰고, 큰 건 mmap으로 별도 영역.
```
네 코드: malloc(100)
   ↓
libc: 미리 받아둔 덩어리에서 100바이트 잘라줌 (시스템콜 없음! 빠름)
   ↓ (여유 없을 때만)
커널: brk 또는 mmap
```
- **매번 시스템콜 안 함** — 한 번에 크게 받아 사용자 공간에서 나눠 씀(시스템콜 172ns 아끼려고).
- **free해도 OS에 바로 반납 안 함** — libc의 free list에 넣어뒀다 재사용. 그래서 `free -m`이 안 늘 수 있음.

**주소 공간에서의 위치 (pmap으로 확인):**
```
코드/데이터  →  brk 힙(증가 ↓)  →  [빈 공간 = mmap 영역]  ←  스택(감소 ↑)
0x558c...        0x558c...            0x7bd2... (mmap, libc)      0x7fff...
```
- **mmap 영역 = 힙과 스택 사이**. 공유 라이브러리, 큰 malloc, 파일 매핑이 여기.
- 큰 malloc이 힙에서 멀리 떨어진 이유 = **mmap으로 만들어져서**(별도 구역 배치). 큰 건 mmap 쓰는 이유: munmap으로 즉시 반납 + 힙 단편화 방지.
- **brk-mmap 사이 공간 보장은 없음.** 64비트 주소 공간(128TB)이 커서 현실적으로 안 부딪힘. 안전장치 = 스택 한도, 충돌 시 실패 반환(ENOMEM/NULL), 가드 갭.

## 기타 API / 실수

- `calloc(n, size)`: n×size 할당 + **0 초기화**.
- `realloc(ptr, newsize)`: 크기 조정, 내용 보존, **이동할 수 있으니 반환값 필수**(tmp 관용구).
- 흔한 sizeof 실수:
  ```c
  malloc(sizeof(int) * n)     // ⭕
  malloc(sizeof(p) * n)       // ❌ 포인터 크기(8)
  malloc(strlen(s))           // ❌ '\0' 자리 없음
  malloc(strlen(s) + 1)       // ⭕
  ```

---

## 숙제 — 도구로 버그 진단

**컴파일 옵션:**
- **`-g`**: 디버깅 정보(소스↔기계어 대응) 포함 → gdb/valgrind가 **줄 번호**를 알려줌. 필수.
- **`-fsanitize=address`**: AddressSanitizer. 컴파일러가 **모든 메모리 접근에 검사 코드 삽입**. 빠름(~2배 느림), 리포트 자세함. 개발/테스트용.
- **ASan과 valgrind는 같이 못 씀** (둘 다 메모리 접근을 가로챔) → "ASan runtime does not come first" 에러.

**세 도구 비교:**
| 도구 | 강점 | 특징 |
|------|------|------|
| **gdb** | 대화형 — 멈춘 지점에서 `print`, `bt`, 단계 실행 | 크래시 위치 즉시 |
| **valgrind** | **모든 에러 + 누수를 한 번에** 종합 리포트 | 느림(10~50배), 재컴파일 불필요 |
| **ASan** | 빠르고 자세(alloc/free 위치까지) | 기본은 **첫 에러에 abort** (`ASAN_OPTIONS=halt_on_error=0`으로 계속) |

**진단 메시지로 버그 구분 (실전 감각):**
| valgrind 메시지 | 버그 |
|-----------------|------|
| `Address ... inside a block ... **free'd**` | use-after-free |
| `... 0 bytes **after** a block of size N` | 버퍼 오버플로우 |
| `... not stack'd, malloc'd or free'd` | 엉뚱한 주소(널 등) |
| `**definitely lost**: N bytes` | 누수 |

**누수 4종(valgrind):** definitely lost(확실, 고쳐야) / indirectly lost(누수된 구조체가 가리키던 것) / possibly lost(중간 포인터만) / still reachable(포인터는 있는데 free 안 함, 보통 무해).

**ASan의 원리 — shadow memory:** 각 8바이트마다 상태를 1바이트로 기록.
- `00` 접근 가능 / **`fd` 해제된 힙** / **`fa` redzone(블록 앞뒤 완충지대 → 오버플로우 감지)**
- 매 접근마다 shadow 확인 → 즉시 판정.

**실측한 것:**
- 널 역참조: 컴파일 조용 → 실행 시 **SIGSEGV**. 주소 0은 어떤 매핑도 없어 MMU가 잡고 OS가 종료 → **Ch13 protection이 작동한 증거.** (주소 0을 일부러 비워둬 널 버그를 빨리 드러냄 = fail fast)
- use-after-free / 오버플로우 / 누수 3개를 valgrind가 **한 번에** 진단, ASan은 첫 에러에서 중단하되 **alloc/free 위치까지** 추적.

---

## 다음
- **Ch 15 Address Translation** — base/bounds, 하드웨어 주소 변환 본격
- 전체 로드맵: [ROADMAP.md](../ROADMAP.md)
