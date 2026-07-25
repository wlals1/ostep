# OSTEP → xv6 학습 로드맵

> 목표: 정답 암기가 아니라 **"왜"를 설명할 수 있는 멘탈 모델** 구축 → xv6 커널 코드 분석까지.
> 학습 방식: 각 챕터는 (1) 개념 직관 → (2) 책의 코드/homework 직접 실행 → (3) 변형 문제로 재현 확인.

---

## 진단 요약 (2026-07-16 작성, 2026-07-26 갱신)

- **현재 위치**: **Phase 2 Memory Virtualization 완료 (Ch 13~22).** 다음은 Phase 3 Concurrency (Ch 26~).
- **강점**: 논리적 추론력 중상급. 반례 생성·논리 반박 잘함. 알고리즘은 골드 수준.
  - **최근 성장**: wish 셸을 처음부터 완성(개념→코드 간극 해소). VSZ/RSS 실측, SIZESORT+ 반례 설계, measure.c로 TLB 절벽 측정 등 **스스로 실험을 설계해 검증**하는 습관이 자리잡음.
  - 책이 대충 넘어가는 지점을 먼저 의심하는 경향이 생김 (하이브리드 페이징의 저장 비용, TLB 미스 핸들러 무한 재귀 등).
- **약점 / 주의**:
  1. C 시스템 프로그래밍 관용구 경험 부족 (헤더, 포인터, 캐스팅). → 코드 예제를 많이 돌려 몸으로 익히기.
  2. **앞선 챕터 내용을 미리 물으면 위축됨.** 순서 절대 건너뛰지 않기.
  3. **비트/자릿수 계산에서 자주 미끄러짐.** 검산 습관으로 보완 중(효과 있음).
- **반복 실수 패턴 (누적 복습 대상)**:
  - ~~"같은 가상 주소 = 같은 물리 메모리" 착각~~ → **Ch18~20에서 해소 ✅**
  - 타입을 "CPU가 읽는 라벨"로 오해 → 타입은 컴파일 타임 개념. C 나올 때마다 재확인.
  - **visibility(가시성) vs atomicity(원자성) 혼동** → volatile이 race를 막는다고 착각. **락(28장)에서 재점검 예정.**
  - **"고수준 한 줄 = 기계 명령 한 개" 착각** (counter++는 load-add-store 3개). **Ch26~28에서 바로 나옴.**
  - 헤더 누락 반복 (stddef.h, pthread.h, unistd.h, string.h, sys/stat.h). 컴파일 에러=선언/헤더, 링크 에러=라이브러리.
  - **메모리 소유권**: `free`는 재귀 아님. alias된 포인터 이른 free → use-after-free. "이 할당 누가 소유/해제?" 자문.
  - 시스템 함수 반환값 미확인 (fopen/stat/malloc NULL·-1 체크).
  - **연산자 우선순위 괄호** — `(x = f()) != NULL`, `(a + b) / c`. reverse·wish·measure.c에서 **세 번 반복됨.** → 복잡한 수식은 **중간 변수로 쪼갤 것.**
  - **"어디서 일어나는가"와 "왜 일어나는가"를 섞어 답함** (lazy allocation 질문에 "mmap 영역에 할당됨"으로 답, base/bounds 내부 단편화 원인을 malloc 조각 선택으로 답). → **"~때문에 ~가 된다"** 형태로 인과를 명시적으로 쓰기.
  - **가상 크기 ≠ 물리 소비** (주소 공간 = 번지 범위, 물리 소비 0). Ch13 VSZ/RSS, Ch18 "16KB vs 9KB"에서 반복 확인.
  - **문제 조건(정렬·헤더·페이지 크기)을 딴 문제에서 가져옴** — offset 비트는 **그 문제의 페이지 크기**가 정함. 시뮬레이터마다 조건 먼저 확인.
  - **우연의 일치를 증거로 착각** — 두 설정 출력이 같다고 "같은 것"이라 결론(Ch17 SIZESORT+). → **차이가 드러나는 반례를 직접 설계**해 검증. 이게 핵심 근육.
  - **0부터 세기** — hex 덤프에서 `7f`가 8개면 그게 0~7번, 8번은 그 다음. **8개씩 끊어 적기.**
  - **검산 습관 (효과 확인됨)**: `0 ≤ offset < 페이지크기`, `VPN×크기+offset = 원주소`, `VPN+offset비트 = log₂(주소공간)`. Ch18 손번역에서 스스로 오류를 잡아냄.

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

- **Ch 2**: 가상화의 세 환상 맛보기. `cpu.c`, `mem.c`, `threads.c`, `io.c`.
- **남겨둔 질문**: 가상→물리 번역에 왜 MMU가 필수인가 → **Ch15에서 답함 ✅**

## Phase 1 — CPU Virtualization (Ch 4~10) ✅

- **Ch 4 Process** — 프로세스란, 상태(Running/Ready/Blocked), PCB.
- **Ch 5 Process API** — fork/exec/wait.
- **Ch 6 Limited Direct Execution** — user/kernel mode, trap, 타이머 인터럽트. ⭐
- **Ch 7 Scheduling** — FIFO, SJF, STCF, 반환 ↔ 응답 트레이드오프.
- **Ch 8 MLFQ** — 실전 스케줄러의 원형.
- **Ch 9 Lottery/Stride** — 비례 배분. (CFS는 stride의 실전판)
- **Ch 10 Multiprocessor** — SQMS/MQMS, 캐시 친화성, work stealing. **Concurrency 후 재방문.**
- **체크포인트 ✅**: "타이머 인터럽트 → 커널 진입 → 스케줄러 → context switch" 설명 가능.

## Phase 2 — Memory Virtualization (Ch 13~22) ✅

⭐ **2장에서 무너졌던 CR3 / 페이지 테이블 / 프로세스별 매핑 질문이 전부 해결됨.**

- ✅ **Ch 13 Address Spaces** — 세 목표(투명성/효율/보호), lazy allocation 실측(VSZ 502MB vs RSS 1.25MB).
- ✅ **Ch 14 Memory API** — 메모리 버그 5종과 UB, brk/mmap, gdb/valgrind/ASan.
- ✅ **Ch 15 Address Translation** — base/bounds, "동적"=실행 시점 번역, HW vs OS 책임, 신뢰 경계.
- ✅ **Ch 16 Segmentation** — 세그먼트별 base/bounds, 감소 세그먼트 오프셋, 코드 공유, 외부 단편화.
- ✅ **Ch 17 Free Space** — 헤더·임베디드 free list, BEST/WORST/FIRST, **탐색 vs 병합 트레이드오프**.
- ✅ **Ch 18 Paging** — VPN/offset 분해, valid 비트, 선형 테이블 4MB 문제.
- ✅ **Ch 19 TLB** — 지역성, reach, 문맥 교환(flush/ASID), **measure.c로 TLB 절벽 실측**.
- ✅ **Ch 20 Advanced Page Tables** — 멀티레벨, CR3, walk가 전부 물리 주소라 재귀 없음.
- ✅ **Ch 21 Swapping 메커니즘** — present/dirty/reference 비트, page fault, swap daemon, thrashing.
- ✅ **Ch 22 Swapping 정책** — OPT/FIFO/LRU/CLOCK, Belady's anomaly, **워크로드가 순위를 뒤집음**.
- **체크포인트 ✅**: "두 프로세스가 같은 가상 주소를 찍는데 값이 안 섞이는 이유" — 프로세스마다 페이지 디렉터리, 문맥 교환 시 CR3 교체. **2장 미해결 질문 회수 완료.**

### Phase 2에서 얻은 관통 원리 (Phase 3에서도 쓰임)

```
Ch18 페이징    → 외부 단편화 해결.  대가: 테이블 4MB + 번역 접근 1번
Ch20 멀티레벨  → 공간 4MB→16KB.    대가: 번역 접근 2~4번 (더 악화)
Ch19 TLB      → 히트 시 번역 0번.   위 두 대가를 한꺼번에 갚음
```
- **모든 층이 "아래층은 백업, 위층은 캐시"** (TLB↔페이지테이블, 메모리↔스왑).
- **메커니즘은 자기 자신에 의존하면 안 됨** (walk는 전부 물리 주소, 미스 핸들러는 wired). → **락 구현이 락을 쓰면 안 되는 것과 같음 (Ch28에서 재등장).**
- **공간 ↔ 시간 트레이드오프**가 매 장 반복.
- **정보가 있는 층이 결정해야 함** (malloc이 압축 못 함, DB가 자기 버퍼 풀 운영).

## Phase 3 — Concurrency (Ch 26~34) ← 다음

- **Ch 26 Concurrency: Intro** — 스레드, 경쟁 상태, 원자성. ★ 반복 실수 "한 줄 = 명령 하나" 정면 등장.
- **Ch 27 Thread API** — `pthread` 생성/조인.
- **Ch 28 Locks** — spinlock, 하드웨어 원자 명령(test-and-set, CAS). ★ volatile vs 원자성 재점검.
- **Ch 29 Lock-based Data Structures**.
- **Ch 30 Condition Variables**.
- **Ch 31 Semaphores** — ⭐ 생산자-소비자, 리더-라이터. 면접 단골.
- **Ch 32 Concurrency Bugs** — atomicity/order violation, **deadlock 4조건**.
- 끝나면 **Ch 10 재방문** (멀티프로세서 스케줄링을 락·캐시 일관성 관점에서).
- **체크포인트**: 데드락 4조건을 대고, 주어진 코드에서 경쟁 상태를 짚어내면 통과.

## Phase 4 — xv6 커널 분석 (프로젝트 최종 목표)

전제: Phase 1~3 완료.

- **xv6 book (riscv)** + 소스 병행. MIT 6.1810/6.S081 자료.
- 분석 순서:
  1. 부팅 → `main()` → 첫 프로세스
  2. 프로세스/스케줄러 (`proc.c`, `swtch.S`) — Phase 1 대조
  3. 시스템 콜/트랩 (`trap.c`, `syscall.c`) — Ch 6 대조
  4. **가상 메모리 (`vm.c`, page table) — Phase 2 대조** ★ 멀티레벨·`__va()`/`__pa()` 확인
  5. 락 (`spinlock.c`, `sleeplock.c`) — Phase 3 대조
  6. 파일 시스템 (`fs.c`, `log.c`) — Phase 5와 함께
- **방식**: "OSTEP 개념 X가 커널 어느 코드로 구현됐나" 매핑하며 읽기.

## Phase 5 — Persistence (Ch 36~43, xv6 FS와 병행)

- **Ch 36 I/O Devices**, **Ch 37 Hard Disk Drives** (Ch21 "모아 쓰기가 이득"의 이유가 여기).
- **Ch 39 Files and Directories** — fd, inode, `open/read/write`.
- **Ch 40 File System Implementation** — ⭐ VSFS. xv6 `fs.c` 대조.
- **Ch 41 FFS** — 지역성 최적화.
- **Ch 42 Crash Consistency & Journaling** — ⭐ xv6 `log.c` 대조.
- (선택) Ch 43 LFS, RAID/분산.

---

## 프로젝트 배치 (ostep-projects)

| 프로젝트 | 시점 | 상태 |
|----------|------|------|
| reverse, unix-utilities, sort | 워밍업 | ✅ |
| **processes-shell (wish)** | Phase 1 끝 | ✅ fork/exec/wait + path 탐색 + `>` + `&` |
| **measure.c (TLB 실측)** | Ch19 | ✅ 절벽 측정, 함정 3개(page fault/컴파일러/타이머) 처리 |
| memory allocator (CSAPP malloc lab도 후보) | Phase 3 전후 | 예정 |
| concurrency 계열 (pzip 등) | Phase 3 | 예정 |
| xv6 랩 (lottery 스케줄러 등) | Phase 4 | 예정 |
| fs checker | Phase 5 | 예정 |

---

## 숙제 폴더명 (헷갈리는 것 메모)

| 챕터 | 폴더 |
|--|--|
| Ch15 Relocation | `vm-mechanism` |
| Ch16 Segmentation | `vm-segmentation` |
| Ch17 Free Space | `vm-freespace` |
| Ch18 Paging | `vm-paging` |
| Ch19 TLB | 폴더 없음 — **직접 C 코드 작성** |
| **Ch20 Multi-level** | **`vm-smalltables`** ⚠ (챕터 제목이 "Smaller Tables") |
| Ch21 Swapping 메커니즘 | `vm-beyondphys` |
| Ch22 Swapping 정책 | `vm-beyondphys-policy` |

---

## 스케줄 (2026-07-16 시작, ~10일 경과)

> 진도보다 **깊이 우선** — 체크포인트에서 막히면 그 자리에서 복습.

### Week 1 ✅ — 기초 + CPU 가상화 (Ch 2~10)
Ch 2 실습 → Ch 4~6(LDE ⭐) → Ch 7~9 스케줄링 → Ch 10
**체크포인트 ✅** 타이머 인터럽트 → context switch 설명

### Week 2 ✅ — wish 셸 + 메모리 가상화 ① (Ch 13~17)
**wish 셸 프로젝트**(계획 밖이었지만 개념→코드 간극 해소로 추가, 잘한 선택)
Ch 13~15 → Ch 16~17

### Week 3 ✅ — 메모리 가상화 ② (Ch 18~22)
Ch 18 Paging → Ch 19 TLB(measure.c 실측) → Ch 20 멀티레벨 → Ch 21~22 스와핑
**체크포인트 ✅** CR3/페이지테이블 질문 회수 완료

### Week 4 ← 지금 — 동시성 (Ch 26~32)
- Ch 26 Intro → Ch 27 Thread API → Ch 28 Locks(CAS/test-and-set)
- Ch 29 Lock-based DS → Ch 30 Condition Vars → Ch 31 Semaphores ⭐ → Ch 32 Bugs/Deadlock
- 끝나면 Ch 10 재방문
- **체크포인트**: 데드락 4조건 + 주어진 코드에서 경쟁 상태 짚기

### Week 5 — xv6 착수
환경 세팅(QEMU, riscv toolchain) + xv6 book Ch 1 → boot/main → proc.c/swtch.S → trap.c/syscall.c

### Week 6 — xv6 심화 + Persistence + 버퍼
vm.c(Phase 2 대조) → spinlock.c(Phase 3 대조) → Ch 36~37, 39~40, 42 + fs.c/log.c 대조

> **버퍼 규칙**: 매주 하루는 예비일. 다 따라가면 xv6 랩(6.1810) 추가.
> 2학년 여름방학이라 시간 여유 있음 — **어려운 장(18~20)은 나중에 한 번 더 돌 것.**

---

## 진행 규칙 (코치와의 약속)

1. **한 번에 한 챕터**, 순서대로.
2. 각 챕터: 개념 → 숙제 실행 → 변형 문제로 마무리.
3. 막히면 정답 대신 단계별 힌트. "직답"이라고 말하면 그때만 바로 답.
4. 챕터 끝날 때마다 이 로드맵 갱신.
5. 위축되면 페이스 조절 신호. 실력 문제 아님 — 순서 문제.
6. **답할 때 식만 쓰지 말고 숫자까지 낼 것.** 검산도 함께.

---

## 진행 현황 트래커

- [x] Ch 2 Introduction — mem.c, cpu.c, threads.c, io.c
- [x] 워밍업: reverse (파일 I/O, 연결 리스트, strdup 소유권, stat/inode, valgrind 클린)
- [x] 워밍업: unix-utilities (wcat, wgrep, wzip/wunzip 왕복 검증)
- [x] 워밍업: ssort (qsort + 함수 포인터, void** 캐스팅, realloc)
- [x] **Phase 1: CPU Virtualization (Ch 4~10)**
  - [x] Ch 4 Process / Ch 5 Process API / Ch 6 LDE(시스템콜 172ns vs 문맥교환 1.2μs 실측)
  - [x] Ch 7 Scheduling / Ch 8 MLFQ / Ch 9 Proportional Share
  - [x] Ch 10 Multiprocessor (work stealing이 손해인 반례 실측) — **Concurrency 후 재방문**
- [x] 프로젝트: **wish 셸** (fork/execv/wait, access로 path 탐색, dup2 리다이렉션, & 병렬)
- [x] **Phase 2: Memory Virtualization (Ch 13~22)** ✅
  - [x] Ch 13 Address Spaces (VSZ vs RSS로 lazy allocation 실측, pmap)
  - [x] Ch 14 Memory API (버그 5종·UB, brk/mmap, gdb/valgrind/ASan, shadow memory)
  - [x] Ch 15 Address Translation ("동적"=실행 시점, HW vs OS 책임, 신뢰 경계)
  - [x] Ch 16 Segmentation (감소 세그먼트 오프셋, 코드 공유, 외부 단편화)
  - [x] Ch 17 Free Space (BEST/WORST/FIRST 실측, **SIZESORT++FIRST=BEST 반례 직접 설계**)
  - [x] Ch 18 Paging (VPN/offset 분해, valid 비트, 선형 테이블 4MB)
  - [x] Ch 19 TLB (**measure.c로 절벽 실측: 1.4ns → 9.9ns, 2단계 TLB 확인**)
  - [x] Ch 20 Advanced Page Tables (멀티레벨 walk 손으로 완주, CR3 vs ASID 구분)
  - [x] Ch 21 Swapping 메커니즘 (present/dirty/reference, page fault, swap daemon, thrashing)
  - [x] Ch 22 Swapping 정책 (**세 워크로드 실험으로 "정책 순위는 워크로드가 결정" 확인**, Belady's anomaly)
- [ ] **Phase 3: Concurrency (Ch 26~34)** ← 다음
  - [ ] Ch 26 Intro / Ch 27 Thread API / Ch 28 Locks
  - [ ] Ch 29 Lock-based DS / Ch 30 CV / Ch 31 Semaphores / Ch 32 Bugs
  - [ ] Ch 10 재방문
- [ ] Phase 4: xv6 커널 분석
- [ ] Phase 5: Persistence (Ch 36~43)
