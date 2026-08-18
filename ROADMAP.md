# OSTEP → xv6 학습 로드맵

> 목표: 정답 암기가 아니라 **"왜"를 설명할 수 있는 멘탈 모델** 구축 → xv6 커널 코드 분석까지.
> 학습 방식: 각 챕터는 (1) 개념 직관 → (2) 책의 코드/homework 직접 실행 → (3) 변형 문제로 재현 확인.

---

## 진단 요약 (2026-07-16 작성, 2026-08-16 갱신)

- **현재 위치**: **Phase 1~3 완료 + malloc lab 완료(94/100).** 다음은 **Phase 4 xv6 커널 분석.**
- **강점**:
  - 논리적 추론력 중상급. 반례 생성·논리 반박 잘함. 알고리즘은 골드 수준.
  - **스스로 실험을 설계해 검증하는 습관**이 확실히 자리잡음 (VSZ/RSS 실측, SIZESORT+ 반례, measure.c TLB 절벽).
  - **코치의 설명도 의심하고 반증함** — Ch27에서 "단일 코어는 최대 1 손실"이라는 내 설명을 2억 번 실행으로 반박. Ch28 `-i≥3이면 안전` 규칙도 스스로 반례를 물음. **이게 이 공부의 핵심 근육.**
  - 책이 대충 넘어가는 지점을 먼저 의심함 (하이브리드 페이징 저장 비용, TLB 미스 핸들러 무한 재귀, 세마포어 `empty = N − fill` 관계, explicit list에서 푸터가 왜 필요한가).
  - ★ **신규: 구조적 한계를 계산으로 확인하고 멈출 줄 앎** — malloc lab에서 binary-bal 55%가 워크로드 구조상 한계임을 산술로 증명하고 최적화를 중단. "더 하면 되겠지"가 아니라 "왜 안 되는지"를 확인.
  - ★ **신규: 측정 규율** — 처리량 잡음(65는 이상치)을 여러 번 실행으로 판별. util은 결정적, thru는 변동이라는 것도 스스로 관찰.
- **약점 / 주의**:
  1. C 시스템 프로그래밍 관용구 경험 부족 → **malloc lab으로 크게 보강됨.** 포인터 산술·캐스팅·매크로를 손에 익힘.
  2. **앞선 챕터 내용을 미리 물으면 위축됨.** 순서 건너뛰지 않기.
  3. **비트/자릿수 계산, 주소 산술에서 자주 미끄러짐.** 검산 습관으로 보완 중(효과 확인).
  4. **여러 질문을 한꺼번에 받으면 무너짐** (Ch30에서 확인). → 코치는 **한 번에 하나만** 물을 것.
  5. **오래 쉬면 세부를 잊음** — 개념 골격은 남지만 코드/수식 디테일이 날아감. **재개 시 노트의 요약부터 훑는 게 효율적.**

### 반복 실수 패턴 (누적 복습 대상)

**해소됨:**
- ~~"같은 가상 주소 = 같은 물리 메모리" 착각~~ → **Ch18~20에서 해소 ✅**
- ~~visibility vs atomicity 혼동 (volatile이 race를 막는다)~~ → **Ch26에서 정면으로 매듭 ✅**
- ~~"고수준 한 줄 = 기계 명령 한 개"~~ → **Ch26에서 어셈블리로 확인 ✅**

**진행 중:**
- 타입을 "CPU가 읽는 라벨"로 오해 → 타입은 컴파일 타임 개념.
- 헤더 문제 — 누락뿐 아니라 **순서**도 (Ch27 `common.h`). → **self-contained header** 원칙.
- **메모리 소유권**: `free`는 재귀 아님. "이 할당 누가 소유/해제?" 자문.
- 시스템 함수 반환값 미확인 (fopen/stat/malloc/pthread_* 체크).
- **연산자 우선순위 괄호** — reverse·wish·measure.c·malloc lab에서 **다섯 번 반복.** → 복잡한 수식은 **중간 변수로 쪼갤 것.**
  - `(x = f()) != NULL` — 대입을 괄호로 (`==`가 `=`보다 우선)
  - `(a + b) / c` — 분자를 괄호로
  - ★ **`a == b == 0`은 "둘 다 0"이 아님** — `(a==b)==0`으로 결합. 수학·파이썬과 다름. **`&&`로 명시할 것**
- **"어디서"와 "왜"를 섞어 답함** → **"~때문에 ~가 된다"** 형태로 인과를 명시.
- **가상 크기 ≠ 물리 소비.**
- **문제 조건(정렬·헤더·페이지 크기)을 딴 문제에서 가져옴.**
- **우연의 일치를 증거로 착각** → 차이가 드러나는 반례를 직접 설계.
- **0부터 세기** — hex 덤프에서 `7f`가 8개면 0~7번. **8개씩 끊어 적기.**
- **식만 쓰고 숫자를 안 냄** — 계산까지가 답. (진행 규칙 6)
- ★ **신규: "문맥 교환이 모든 문제의 원인"** → 아님. 멀티코어는 문맥 교환 없이도 동시 실행.
  **진짜 원인 = 여러 실행 흐름의 순서가 보장되지 않는다** (버그가 아니라 동시성의 정의).
- ★ **신규: 락이 변수 자체를 보호한다고 오해** → **락은 강제가 아니라 규약.** 그 데이터를 건드리는 **모든 코드**가 같은 락을 잡아야 성립.
- ★ **신규: `while (cond) cond_wait()`을 스핀으로 오해** → `cond_wait`은 블로킹 호출. 깨어날 때만 루프가 돎. CPU 0.
- ★ **신규(malloc lab): 주소를 다룰 때 "무엇을 가리키는지" 혼동**
  - `GET_SIZE(bp)` vs `GET_SIZE(HDRP(bp))` — bp는 payload, 크기는 헤더에
  - `GET_ALLOC(next)` vs `GET_ALLOC(HDRP(next))` — 같은 실수 반복
  - `PREV_BLKP` 계산 시 **읽는 위치(bp-8)와 빼는 기준점(bp)을 혼동**
  - **매크로가 이런 실수를 막아줌** — 오프셋을 손으로 세지 말 것
- ★ **신규: 값 vs 주소** — `GET_SIZE`는 **주소를 받아 그 자리 값을 읽음**. 주소 자체를 연산하면 안 됨.
- ★ **신규: 물리적 인접 vs 논리적(리스트) 인접을 섞음** — 병합은 물리, 탐색은 리스트. 각각 다른 자료가 담당.

**검산 습관 (효과 확인됨)**: `0 ≤ offset < 페이지크기`, `VPN×크기+offset = 원주소`, `VPN+offset비트 = log₂(주소공간)`.
**주소 산술도 구체적 숫자로 검산** — `NEXT_BLKP(1004) = 1028이 맞나?`처럼.

---

## 전체 구조

| 기둥 | 핵심 질문 | 챕터 |
|------|-----------|------|
| **Virtualization** | 하나의 CPU/메모리를 어떻게 여러 프로세스가 독점하는 것처럼 쓰게 하나 | 4~23 |
| **Concurrency** | 여러 실행 흐름이 공유 자원을 어떻게 안전하게 다루나 | 26~34 |
| **Persistence** | 전원이 꺼져도 데이터를 어떻게 안전하게 보존하나 | 36~43 |

**추천 순서**: Virtualization → Concurrency → (xv6로 커널 분석) → Persistence.

---

## Phase 0 — Introduction ✅

- **Ch 2**: 가상화의 세 환상. `cpu.c`, `mem.c`, `threads.c`, `io.c`.
- **남겨둔 질문**: 가상→물리 번역에 왜 MMU가 필수인가 → **Ch15에서 답함 ✅**

## Phase 1 — CPU Virtualization (Ch 4~10) ✅

- Ch 4 Process / Ch 5 Process API / **Ch 6 LDE ⭐** / Ch 7 Scheduling / Ch 8 MLFQ / Ch 9 Lottery·Stride
- **Ch 10 Multiprocessor** — SQMS/MQMS, 캐시 친화성, work stealing. **Concurrency 후 재방문 ← 아직 안 함**
- **체크포인트 ✅**: "타이머 인터럽트 → 커널 진입 → 스케줄러 → context switch" 설명 가능.

## Phase 2 — Memory Virtualization (Ch 13~22) ✅

⭐ **2장에서 무너졌던 CR3 / 페이지 테이블 / 프로세스별 매핑 질문이 전부 해결됨.**

- ✅ Ch 13 Address Spaces (lazy allocation 실측: VSZ 502MB vs RSS 1.25MB)
- ✅ Ch 14 Memory API (버그 5종·UB, brk/mmap, gdb/valgrind/ASan)
- ✅ Ch 15 Address Translation ("동적"=실행 시점 번역, HW vs OS 책임, 신뢰 경계)
- ✅ Ch 16 Segmentation (감소 세그먼트 오프셋, 코드 공유, 외부 단편화)
- ✅ Ch 17 Free Space (탐색 vs 병합 트레이드오프, SIZESORT++FIRST=BEST 반례)
- ✅ Ch 18 Paging (VPN/offset 분해, valid 비트, 선형 테이블 4MB)
- ✅ Ch 19 TLB (measure.c로 절벽 실측: 1.4ns → 9.9ns, 2단계 TLB 확인)
- ✅ Ch 20 Advanced Page Tables (멀티레벨 walk 손으로 완주, CR3 vs ASID)
- ✅ Ch 21 Swapping 메커니즘 (present/dirty/reference, page fault, swap daemon, thrashing)
- ✅ Ch 22 Swapping 정책 (세 워크로드 실험, Belady's anomaly)
- **체크포인트 ✅**: **2장 미해결 질문 회수 완료.**

### Phase 2 관통 원리
```
Ch18 페이징    → 외부 단편화 해결.  대가: 테이블 4MB + 번역 접근 1번
Ch20 멀티레벨  → 공간 4MB→16KB.    대가: 번역 접근 2~4번 (더 악화)
Ch19 TLB      → 히트 시 번역 0번.   위 두 대가를 한꺼번에 갚음
```
- **모든 층이 "아래층은 백업, 위층은 캐시"** (TLB↔페이지테이블, 메모리↔스왑).
- **메커니즘은 자기 자신에 의존하면 안 됨** (walk는 물리 주소, 미스 핸들러는 wired). → **Ch28 락 구현에서 세 번째로 재등장 ✅**
- **공간 ↔ 시간 트레이드오프**가 매 장 반복.
- **정보가 있는 층이 결정해야 함** (malloc이 압축 못 함, DB가 자기 버퍼 풀).

## Phase 3 — Concurrency (Ch 26~33) ✅ 개념 완료

- ✅ **Ch 26 Intro** — race condition, `counter++`=명령 3개, **volatile은 무력**(가시성≠원자성). x86.py 실측(`-i 4`에서 깨지는 반례를 스스로 발견).
- ✅ **Ch 27 Thread API** — 함수 포인터 읽기, `void*` 캐스팅 2단계, create는 주소/join은 값, **락은 규약**. race.c로 멀티코어 50% 손실 실측, helgrind(거짓 양성 포함).
- ✅ **Ch 28 Locks** — 부트스트랩 문제, TestAndSet/CAS/FetchAndAdd, 티켓락, park/unpark, lost wakeup. x86.py로 flag.s vs test-and-set.s 대조.
- ✅ **Ch 29 Lock-based Data Structures** — sloppy counter, hand-over-hand, **"더 병렬적 ≠ 더 빠름"**, 세분화가 이득인 조건.
- ✅ **Ch 30 Condition Variables** — wait이 뮤텍스를 받는 이유, 상태 변수 필수, **while(Mesa semantics)**, 생산자-소비자.
- ✅ **Ch 31 Semaphores** — 초기값이 용도를 결정(1=락/0=순서/N=제한), 리더-라이터, 철학자.
- ✅ **Ch 32 Concurrency Bugs** — 비데드락 70%(원자성·순서 위반), **데드락 4조건**과 각 조건을 깨는 기법.
- ✅ **Ch 33 Event-based** — 이벤트 루프, select/epoll, 논블로킹 I/O, **stack ripping → async/await**, C10K.
- **체크포인트 ✅**: 데드락 4조건 + 코드에서 경쟁 상태 짚기.
- ✅ **Ch 10 재방문 완료** — 멀티프로세서 스케줄링을 락·캐시 일관성 관점에서 (Ch10 노트에 "📌 재방문" 섹션 추가)
- ✅ **`threads-bugs` 숙제 완료** — 데드락 4조건 해법을 실제 코드로 비교
- **남은 것**: ⬜ Ch29·30·31 숙제(`threads-locks-usage`, `threads-cv`, `threads-sema`) — 개념 확인용이라 우선순위 낮음

### Phase 3 관통 원리
```
"확인하고 행동하기(check-then-act)"가 모든 동시성 버그의 뿌리 — 네 번 반복
  Ch26 flag 확인 → 설정          (깨진 락)
  Ch28 flag 읽기 → 판단           (flag.s, 낡은 레지스터)
  Ch30 done 확인 → 잠들기         (lost wakeup)
  Ch32 NULL 검사 → 사용           (원자성 위반)
```
- **원자성이 필요한 건 "읽고 쓰는 그 순간"뿐** — 그 뒤 판단은 느긋해도 됨(Ch28 xchg).
- **더 정교한 게 더 빠른 건 아님** — hand-over-hand, rwlock 모두 단순 mutex보다 느릴 수 있음. **측정 후 최적화.**
- **독립적으로 나눌 수 있는 축을 찾아라** — 해시 버킷·per-CPU 카운터는 잘 쪼개지고, 리스트 노드는 안 됨.
- **락은 규약이지 강제가 아님** — 컴파일러가 검사 못 함. (Rust `Mutex<T>`가 타입으로 해결)
- **워크로드가 답을 정한다** — I/O 바운드면 이벤트, CPU 바운드면 스레드. (Ch22 "정책 순위는 워크로드가 결정"과 같은 사고)

## ★ CSAPP Malloc Lab ✅ (Ch17 심화 프로젝트, 2026-08-16 완료)

**94/100** (util 54/60 + thru 40/40). Ch17을 통째로 손으로 구현.

```
① implicit free list        67점   기본 동작
② realloc 제자리 확장        75점   복사 회피 → realloc util 27%→100%
③ explicit free list        89점   free 블록만 연결 → thru 만점 달성
④ best fit                  91점   실제 프로그램 트레이스 99~100% 회복
⑤ CHUNKSIZE 4096→64         94점   과할당 축소
```

**핵심 산출물:**
- `mm_check` 불변식 검사기 — **implicit 때부터 잠복하던 8바이트 블록 버그**를 explicit 전환 시 포착
- **binary-bal 55%가 구조적 한계임을 산술로 증명**하고 최적화 중단 (구멍 456 < 요청 520)
- 측정 규율: 처리량 잡음 판별, "점수판이 전략을 바꾼다", 튜닝의 과적합 인식

**남은 카드**(나중에): segregated free list / segregated storage / 할당 블록 푸터 제거

## Phase 4 — xv6 커널 분석 (프로젝트 최종 목표) ← **지금 여기**

전제: Phase 1~3 완료 ✅ + C 실력 보강(malloc lab) ✅

- **xv6 book (riscv)** + 소스 병행. MIT 6.1810/6.S081 자료.
- 분석 순서:
  1. 환경 세팅 (QEMU, riscv toolchain) + xv6 book Ch 1
  2. 부팅 → `main()` → 첫 프로세스
  3. 프로세스/스케줄러 (`proc.c`, `swtch.S`) — Phase 1 대조
  4. 시스템 콜/트랩 (`trap.c`, `syscall.c`) — Ch 6 대조
  5. **가상 메모리 (`vm.c`, page table) — Phase 2 대조** ★ 멀티레벨·`__va()`/`__pa()`
  6. **커널 할당기 (`kalloc.c`) — malloc lab 대조** ★ free list의 커널 버전
  7. **락 (`spinlock.c`, `sleeplock.c`) — Phase 3 대조** ★ Ch28의 TestAndSet 실물, `push_off()`는 Ch28 인터럽트 끄기
  8. 파일 시스템 (`fs.c`, `log.c`) — Phase 5와 함께
- **방식**: "OSTEP 개념 X가 커널 어느 코드로 구현됐나" 매핑하며 읽기.

**미리 알아둘 대조표:**
| OSTEP 개념 | xv6 코드 |
|--|--|
| Ch6 트랩·문맥 교환 | `trap.c`, `swtch.S` |
| Ch9 비례 배분 | (랩) `scheduling-xv6-lottery` |
| Ch20 멀티레벨 페이지 테이블 | `vm.c`의 `walk()` |
| Ch17 free list | `kalloc.c` (고정 크기 페이지 단위라 더 단순) |
| Ch28 TestAndSet 스핀락 | `spinlock.c`의 `__sync_lock_test_and_set` |
| Ch28 인터럽트 끄기 | `spinlock.c`의 `push_off()`/`pop_off()` |
| Ch10 per-CPU 구조 | `struct cpu`, `mycpu()` |
| Ch30 조건 변수 | `sleep()`/`wakeup()` |

## Phase 5 — Persistence (Ch 36~43, xv6 FS와 병행)

- **Ch 36 I/O Devices**, **Ch 37 Hard Disk Drives** (Ch21 "모아 쓰기가 이득"의 이유가 여기).
- **Ch 39 Files and Directories** — fd, inode, `open/read/write`.
- **Ch 40 File System Implementation** — ⭐ VSFS. xv6 `fs.c` 대조.
- **Ch 41 FFS** — 지역성 최적화.
- **Ch 42 Crash Consistency & Journaling** — ⭐ xv6 `log.c` 대조.
- (선택) Ch 43 LFS, RAID/분산.

---

## 프로젝트 배치

| 프로젝트 | 시점 | 상태 |
|----------|------|------|
| reverse, unix-utilities, sort | 워밍업 | ✅ |
| **processes-shell (wish)** | Phase 1 끝 | ✅ fork/exec/wait + path + `>` + `&` |
| **measure.c (TLB 실측)** | Ch19 | ✅ 함정 3개(page fault/컴파일러/타이머) 처리 |
| **race.c + helgrind** | Ch27 | ✅ 멀티코어 50% 손실, 단일코어 2억 번 반례 |
| **threads-bugs 숙제** | Ch32 | ✅ 데드락 4조건 해법 5종 실측 비교 |
| **CSAPP malloc lab** | Ch17 심화 | ✅ **94/100.** implicit→explicit→best fit→튜닝 |
| **mini-pmap** (/proc 파서) | Phase 2 복습 | 🔶 **설계 중단** — maps/smaps/pagemap 파악, `VPN=VA/4096`·`offset=VPN*8` 도출까지. 남은 것: sscanf 파싱 / 큰 구간 출력 / 권한 실패 대응 → 코딩 |
| concurrency-pzip | Phase 3 | ⬜ 멀티스레드 압축 |
| concurrency-webserver | Phase 3 | ⬜ **스레드 방식 vs 이벤트 방식 직접 비교** (Ch33) |
| concurrency-mapreduce | Phase 3 | ⬜ |
| xv6 랩 (lottery 스케줄러 등) | Phase 4 | ⬜ |
| fs checker | Phase 5 | ⬜ |

---

## 숙제 폴더명 (헷갈리는 것 메모)

| 챕터 | 폴더 |
|--|--|
| Ch15 Relocation | `vm-mechanism` |
| Ch16 Segmentation | `vm-segmentation` |
| Ch17 Free Space | `vm-freespace` |
| Ch18 Paging | `vm-paging` |
| Ch19 TLB | 폴더 없음 — **직접 C 코드** |
| **Ch20 Multi-level** | **`vm-smalltables`** ⚠ |
| Ch21 / Ch22 Swapping | `vm-beyondphys` / `vm-beyondphys-policy` |
| Ch26 Intro | `threads-intro` (x86.py) |
| Ch27 Thread API | `threads-api` (helgrind) |
| Ch28 Locks | `threads-locks` (x86.py + xchg/fetchadd) |
| Ch29 Lock-based DS | `threads-locks-usage` ⬜ 안 함 |
| Ch30 CV / Ch31 Sema | `threads-cv` / `threads-sema` ⬜ 안 함 |
| **Ch32 Bugs** | **`threads-bugs`** ✅ 완료 |
| Ch17 심화 | CSAPP `malloclab-handout` ✅ 완료 (94/100) |

---

## 스케줄 (2026-07-16 시작, ~한 달 경과)

> 진도보다 **깊이 우선**.

### Week 1 ✅ 기초 + CPU 가상화 (Ch 2~10)
### Week 2 ✅ wish 셸 + 메모리 가상화 ① (Ch 13~17)
### Week 3 ✅ 메모리 가상화 ② (Ch 18~22) — 체크포인트 통과
### (1주 휴식) — 7월 말~8월 초. **문제 없음.** 원래 페이스가 빨랐던 것.
### Week 4 ✅ 동시성 (Ch 26~33) — 체크포인트 통과
### Week 5 ✅ Ch10 재방문 + threads-bugs 숙제 + **malloc lab (94/100)**
### Week 6 ← 지금 — **Phase 4 xv6 착수**
### Week 7~ — xv6 심화 + Persistence

> **버퍼 규칙**: 밀리면 흡수. 2학년 여름방학이라 시간 여유 있음.
> **어려운 장(18~20, 28~31)은 나중에 한 번 더 돌 것.** 노트가 그때를 위해 쓰여 있음.

---

## 다음 할 일

**① Phase 4 xv6 착수** ← 지금
```
환경 세팅(QEMU + riscv toolchain) → xv6 book Ch1 → boot/main → proc.c/swtch.S
```
**전제가 다 갖춰졌음** — Phase 1~3 개념 + malloc lab으로 C 실력 보강 완료.

**언제든 끼워 넣을 수 있는 것 (독립적):**
- **concurrency-webserver** — Ch33의 스레드 vs 이벤트를 직접 비교. 재밌을 것
- **mini-pmap** — 설계 중단분. Phase 2 복습
- **segregated free list** — malloc lab 이어서 (OSTEP 더 공부한 뒤에 하기로)
- Ch29~31 숙제 — 개념 확인용, 우선순위 낮음

---

## 진행 규칙 (코치와의 약속)

1. **한 번에 한 챕터**, 순서대로.
2. 각 챕터: 개념 → 숙제 실행 → 변형 문제로 마무리.
3. 막히면 정답 대신 단계별 힌트. "직답"이라고 말하면 그때만 바로 답.
4. 챕터 끝날 때마다 이 로드맵 갱신.
5. 위축되면 페이스 조절 신호. 실력 문제 아님 — 순서 문제.
6. **답할 때 식만 쓰지 말고 숫자까지 낼 것.** 검산도 함께.
7. ★ **코치는 한 번에 하나만 물을 것.** 세 개를 동시에 던지면 무너짐(Ch30에서 확인).
8. ★ **코치의 설명도 의심할 것.** 실제로 두 번 반증에 성공했음(Ch27 단일코어 손실, Ch28 인터럽트 간격).
9. ★ **오래 쉬고 돌아오면 노트 요약부터 훑기.** 개념은 남지만 디테일은 날아감. 처음부터 다시 하지 말 것.
10. ★ **큰 프로젝트는 "동작 → 검사기 → 최적화" 순서로.** 검사기 없이 최적화하면 버그를 못 찾음 (malloc lab에서 확인).

---

## 진행 현황 트래커

- [x] Ch 2 Introduction
- [x] 워밍업: reverse / unix-utilities / ssort
- [x] **Phase 1: CPU Virtualization (Ch 4~10)** — Ch10 재방문까지 ✅
- [x] 프로젝트: **wish 셸**
- [x] **Phase 2: Memory Virtualization (Ch 13~22)** ✅ 체크포인트 통과
- [x] **Phase 3: Concurrency (Ch 26~33)** ✅ 체크포인트 통과
  - [x] Ch 26 Intro (x86.py) / Ch 27 Thread API (race.c, helgrind) / Ch 28 Locks (x86.py)
  - [x] Ch 29 Lock-based DS / Ch 30 CV / Ch 31 Semaphores / Ch 32 Bugs (threads-bugs) / Ch 33 Event-based
  - [x] Ch 10 재방문 (락·캐시 일관성 관점)
  - [ ] Ch 29~31 숙제 (`threads-locks-usage`, `threads-cv`, `threads-sema`) — 우선순위 낮음
- [x] **CSAPP malloc lab** ✅ 94/100 (implicit → explicit → best fit → 튜닝)
- [ ] **Phase 4: xv6 커널 분석** ← **지금**
- [ ] 밀린 것: mini-pmap(설계 중단), segregated free list
- [ ] Phase 3 프로젝트: pzip / webserver / mapreduce
- [ ] Phase 5: Persistence (Ch 36~43)
