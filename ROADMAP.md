# OSTEP → xv6 학습 로드맵

> 목표: 정답 암기가 아니라 **"왜"를 설명할 수 있는 멘탈 모델** 구축 → xv6 커널 코드 분석까지.
> 학습 방식: 각 챕터는 (1) 개념 직관 → (2) 책의 코드/homework 직접 실행 → (3) 변형 문제로 재현 확인.

---

## 진단 요약 (2026-07-16 작성, 2026-07-22 갱신)

- **현재 위치**: Phase 2 Memory Virtualization — Ch 13~17 완료, **Ch 18 Paging 진행 중**.
- **강점**: 논리적 추론력 중상급. 반례 생성·논리 반박 잘함. 알고리즘은 골드 수준.
  - 최근 성장: wish 셸을 처음부터 완성(개념→코드 간극 메움). VSZ/RSS 실측, SIZESORT+ 반례 설계 등 **스스로 실험을 설계해 검증**하는 습관이 자리잡는 중.
- **약점 / 주의**:
  1. C 시스템 프로그래밍 관용구 경험 부족 (헤더, 포인터, 캐스팅). → 코드 예제를 많이 돌려 몸으로 익히기.
  2. **앞선 챕터 내용을 미리 물으면 위축됨.** 순서 절대 건너뛰지 않기. 지금 챕터가 요구하는 것만.
- **반복 실수 패턴 (누적 복습 대상)**:
  - "같은 가상 주소 = 같은 물리 메모리"로 착각 → 페이징 챕터(18~20장)에서 확실히 매듭.
  - 타입을 "CPU가 읽는 라벨"로 오해 → 타입은 컴파일 타임 개념. C 계속 나올 때마다 재확인.
  - **visibility(가시성) vs atomicity(원자성) 혼동** → volatile이 race를 막는다고 착각함. 락(28장)에서 재점검.
  - **"고수준 한 줄 = 기계 명령 한 개"로 착각** (counter++는 사실 load-add-store 3개). 원자성/critical section 볼 때마다 상기.
  - 헤더 누락 에러 반복 (stddef.h, pthread.h, unistd.h, string.h, sys/stat.h). 컴파일 에러 = 선언/헤더, 링크 에러 = 라이브러리 구분 습관화.
  - **메모리 소유권**: `free`는 재귀 아님(구조체 내부 포인터 따로 free). alias된 포인터 이른 free → use-after-free, 안 하면 leak. reverse에서 체득 — 새 코드마다 "이 할당 누가 소유/해제?" 자문.
  - 시스템 함수 반환값 미확인 습관 (fopen/stat NULL·-1 체크 빠뜨림). "성공/실패 반환 → 항상 확인 후 사용" 반복 강조.
  - **"어디서 일어나는가"와 "왜 일어나는가"를 섞어 답하는 경향** (예: lazy allocation 질문에 "mmap 영역에 할당됨"으로 답). 질문이 메커니즘인지 이유인지 구분.
  - **가상 크기 ≠ 물리 소비** (malloc 성공 ≠ 물리 메모리 확보). Ch13 VSZ/RSS 실측으로 잡음 — 스와핑(21~22장)에서 재확인.
  - **감소(스택) 세그먼트 오프셋**: 하위 비트가 아니라 `주소공간크기 - VA`(끝에서의 거리). Ch16에서 헷갈렸던 것.
  - **우연의 일치를 증거로 착각** ⚠ 신규: 두 설정의 출력이 같다고 "같은 것"이라 결론내림(Ch17 SIZESORT+). → **차이가 드러나는 반례를 직접 설계해서 검증**하는 습관. 이게 이 공부의 핵심 근육.
  - **문제 조건(정렬·헤더) 빼먹고 계산**: `-a 8`에서 10바이트 요청=16 소비, `-H 8`에서 반환 주소=블록시작+8. 시뮬레이터 숙제마다 조건 먼저 체크.

---

## 전체 구조

OSTEP는 세 기둥(Three Easy Pieces)으로 되어 있다:

| 기둥 | 핵심 질문 | 챕터 |
|------|-----------|------|
| **Virtualization** | 하나의 CPU/메모리를 어떻게 여러 프로세스가 독점하는 것처럼 쓰게 하나 | 4~23 |
| **Concurrency** | 여러 실행 흐름이 공유 자원을 어떻게 안전하게 다루나 | 26~34 |
| **Persistence** | 전원이 꺼져도 데이터를 어떻게 안전하게 보존하나 | 36~43 |

**추천 순서**: Virtualization → Concurrency → (xv6로 커널 분석) → Persistence.
xv6를 Concurrency 직후에 넣는 이유: 그 시점이면 프로세스/주소공간/락 개념이 다 잡혀서 커널 코드가 "읽힌다". Persistence(파일시스템)는 xv6 파일시스템 분석과 묶어서 봐도 좋다.

---

## Phase 0 — Introduction ✅

- **Ch 2 Introduction**: 가상화의 세 가지 환상 맛보기. `cpu.c`, `mem.c`, `threads.c`, `io.c` 예제.
- ✅ `mem.c` — 프로세스별 사적 주소 공간, 가상 vs 물리, ASLR.
- ✅ `cpu.c` — 프로세스 여러 개가 하나의 CPU를 나눠 쓰는 착시 (time-sharing, 타이머 인터럽트).
- ✅ `threads.c` — 공유 변수 경쟁 상태 미리보기 (Concurrency 예고편).
- ✅ `io.c` — 영속성, 시스템콜로 파일 쓰기.
- **남겨둔 질문(지금 답 안 해도 됨)**: 가상→물리 번역에 왜 하드웨어(MMU)가 필수인가 → Ch 15에서 답함 ✅.

## Phase 1 — CPU Virtualization (Ch 4~10) ✅

여기서 2장에서 못 답한 "context switch가 정확히 어떻게 일어나나"가 풀린다.

- **Ch 4 The Abstraction: Process** — 프로세스란 무엇인가, 상태(Running/Ready/Blocked).
- **Ch 5 Process API** — `fork()`, `exec()`, `wait()`. ⭐ 손으로 코드 많이 짜볼 것.
- **Ch 6 Limited Direct Execution** — user/kernel mode, trap, system call, timer interrupt. ⭐ **2장에서 어렵던 "OS가 어떻게 CPU를 뺏나"의 정답.**
- **Ch 7 Scheduling: Intro** — FIFO, SJF, STCF, 응답시간 vs 반환시간.
- **Ch 8 MLFQ** — 실전 스케줄러의 원형.
- **Ch 9 Lottery/Stride** — 비례 배분 스케줄링.
- **Ch 10 Multiprocessor Scheduling** — SQMS vs MQMS, 캐시 친화성, work stealing. **Concurrency 끝나고 한 번 더 볼 것** (락·캐시 일관성 개념 잡힌 뒤).
- **체크포인트 ✅**: "타이머 인터럽트 → 커널 진입 → 스케줄러 → context switch" 흐름 설명 가능.

## Phase 2 — Memory Virtualization (Ch 13~23) ← 진행 중

⭐ **2장에서 널 무너뜨린 CR3 / 페이지 테이블 / 프로세스별 매핑 질문이 전부 여기서 정식으로 답해진다.**

- ✅ **Ch 13 Address Spaces** — 주소 공간 추상화, 세 목표(투명성/효율/보호), lazy allocation 실측(VSZ vs RSS).
- ✅ **Ch 14 Memory API** — malloc/free, 메모리 버그 5종과 UB, brk/mmap, gdb/valgrind/ASan.
- ✅ **Ch 15 Address Translation** — base/bounds, 하드웨어 vs OS 책임 분담, "동적"의 의미.
- ✅ **Ch 16 Segmentation** — 세그먼트별 base/bounds, 감소 세그먼트 오프셋, 공유·보호 비트, 외부 단편화.
- ✅ **Ch 17 Free Space Management** — 헤더·임베디드 free list, splitting/coalescing, BEST/WORST/FIRST, 탐색 vs 병합 트레이드오프.
- **Ch 18 Paging: Intro** — 페이지, 프레임, 페이지 테이블. ⭐ ← **지금 여기**
- **Ch 19 TLB** — 번역 캐싱, 성능. (MMU 성능 이야기 완결)
- **Ch 20 Advanced Page Tables** — multi-level page table. ⭐ CR3가 가리키는 그 구조.
- **Ch 21~22 Swapping** — 메커니즘 + 정책(LRU 등).
- **체크포인트**: "두 프로세스가 같은 가상 주소를 찍는데 값이 안 섞이는 이유"를 페이지 테이블 + CR3 교체로 완전히 설명하면 통과. (2장의 미해결 질문 회수)

## Phase 3 — Concurrency (Ch 26~34)

- **Ch 26 Concurrency: Intro** — 스레드, 경쟁 상태, 원자성.
- **Ch 27 Thread API** — `pthread` 생성/조인.
- **Ch 28 Locks** — spinlock, 하드웨어 원자 명령(test-and-set, CAS).
- **Ch 29 Lock-based Data Structures**.
- **Ch 30 Condition Variables**.
- **Ch 31 Semaphores** — ⭐ 생산자-소비자, 리더-라이터. 면접 단골.
- **Ch 32 Concurrency Bugs** — atomicity/order violation, **deadlock 4조건**.
- 끝나면 **Ch 10 재방문** (멀티프로세서 스케줄링을 락·캐시 일관성 관점에서).
- **체크포인트**: 데드락 4조건을 대고, 주어진 코드에서 경쟁 상태를 짚어낼 수 있으면 통과.

## Phase 4 — xv6 커널 분석 (프로젝트 최종 목표)

전제: Phase 1~3 완료 (프로세스, 주소공간, 트랩, 락 개념 필요).

- **xv6 book (riscv 버전)** 과 소스를 병행. MIT 6.1810/6.S081 자료 활용.
- 추천 분석 순서:
  1. 부팅 → `main()` → 첫 프로세스
  2. 프로세스/스케줄러 (`proc.c`, `swtch.S`) — Phase 1과 대조
  3. 시스템 콜/트랩 (`trap.c`, `syscall.c`) — Ch 6과 대조
  4. 가상 메모리 (`vm.c`, page table) — Phase 2와 대조
  5. 락 (`spinlock.c`, `sleeplock.c`) — Phase 3과 대조
  6. 파일 시스템 (`fs.c`, `log.c`) — Phase 5와 함께
- **방식**: "OSTEP에서 배운 개념 X가 실제 커널에서 어느 코드로 구현됐나"를 매핑하며 읽기. 개념 없이 코드부터 읽으면 길을 잃음.

## Phase 5 — Persistence (Ch 36~43, xv6 FS와 병행)

- **Ch 36 I/O Devices**, **Ch 37 Hard Disk Drives**.
- **Ch 39 Files and Directories** — 파일 디스크립터, inode, `open/read/write`.
- **Ch 40 File System Implementation** — ⭐ VSFS. xv6 `fs.c`와 직접 대조.
- **Ch 41 FFS** — 지역성 최적화.
- **Ch 42 Crash Consistency & Journaling** — ⭐ xv6 `log.c`와 대조.
- (선택) Ch 43 LFS, 이후 RAID/분산 등.

---

## 프로젝트 배치 (ostep-projects)

| 프로젝트 | 시점 | 상태 |
|----------|------|------|
| reverse, unix-utilities, sort | 워밍업 | ✅ |
| **processes-shell (wish)** | Phase 1 끝 | ✅ fork/exec/wait + path 탐색 + `>` 리다이렉션 + `&` 병렬 |
| memory allocator 계열 | Phase 2 중반~끝 | 예정 (CSAPP malloc lab도 후보) |
| concurrency 계열 (pzip 등) | Phase 3 | 예정 |
| xv6 랩 (lottery 스케줄러 등) | Phase 4 | 예정 |
| fs checker | Phase 5 | 예정 |

---

## 6주 스케줄 (하루 4시간 / 40일, 2026-07-16 시작)

> 총 예산 ~160h. 진도보다 **깊이 우선** — 체크포인트에서 막히면 그 자리에서 복습, 진도 미루기.
> 하루 4h 중 3h 학습(읽기+코드) + 1h는 복습/변형 문제/막힌 곳 정리 권장.

### Week 1 — 기초 + CPU 가상화 (Ch 2 마무리 ~ Ch 9) ✅
- Ch 2 실습 → Ch 4~6 (LDE ⭐) → Ch 7~9 스케줄링
- **체크포인트 ✅**: "타이머 인터럽트 → context switch" 말로 설명

### Week 2 — Ch 10 + wish 셸 + 메모리 가상화 ① (Ch 13~17) ✅
- Ch 10 멀티프로세서 → **wish 셸 프로젝트** (계획엔 없었지만 개념→코드 간극 메우기로 추가, 잘한 선택)
- Ch 13 Address Spaces → Ch 14 Memory API → Ch 15 Address Translation ⭐
- Ch 16 Segmentation → Ch 17 Free Space

### Week 3 — 메모리 가상화 ② (Ch 18~22 + 종합) ← 지금
- **Ch 18 Paging Intro ⭐** → Ch 19 TLB → Ch 20 Multi-level Page Table ⭐(CR3가 가리키는 구조)
- Ch 21~22 Swapping
- **체크포인트**: 2장의 CR3/페이지테이블 질문 완전 회수 — 같은 가상주소가 안 섞이는 이유 설명

### Week 4 — 동시성 (Ch 26~32)
- Ch 26 Intro → Ch 27 Thread API → Ch 28 Locks(CAS/test-and-set)
- Ch 29 Lock-based DS → Ch 30 Condition Vars → Ch 31 Semaphores ⭐ → Ch 32 Bugs/Deadlock
- 끝나면 Ch 10 재방문
- **체크포인트**: 데드락 4조건 + 주어진 코드에서 경쟁 상태 짚기

### Week 5 — xv6 착수 (환경 + 프로세스/트랩)
- 환경 세팅(QEMU, riscv toolchain) + xv6 book Ch 1
- boot → main() → 첫 프로세스
- proc.c / swtch.S (Phase 1 대조)
- trap.c / syscall.c (Ch 6 대조)

### Week 6 — xv6 심화 + Persistence 핵심 + 버퍼
- vm.c 페이지 테이블 (Phase 2 대조) → spinlock.c (Phase 3 대조)
- Ch 36 I/O, Ch 37 Disks → Ch 39 Files → Ch 40 FS Impl + fs.c 대조
- Ch 42 Journaling + log.c 대조
- 남는 시간 = 밀린 챕터 회수 / xv6 랩 / 종합 복습

> **버퍼 규칙**: 매주 하루는 예비일. 밀리면 여기서 흡수. 다 따라가면 xv6 랩(6.1810) 추가.

---

## 진행 규칙 (코치와의 약속)

1. **한 번에 한 챕터**, 순서대로. 앞 챕터 궁금해도 참고 지금 것에 집중.
2. 각 챕터: 개념 → 책 코드/homework 실행 → 내가 내는 변형 문제로 마무리.
3. 막히면 정답 대신 단계별 힌트. "직답"이라고 말하면 그때만 바로 답.
4. 챕터 끝날 때마다 이 로드맵의 체크박스를 갱신.
5. 위축되면 페이스 조절 신호. 실력 문제 아님 — 순서 문제.

---

## 진행 현황 트래커

- [x] Ch 2 Introduction — mem.c, cpu.c, threads.c, io.c
- [x] 워밍업 프로젝트: reverse (파일 I/O, 연결 리스트, strdup 소유권, stat/inode, valgrind 클린)
- [x] 워밍업 프로젝트: unix-utilities (wcat, wgrep=strstr, wzip=fwrite/RLE, wunzip=fread, 왕복 검증)
- [x] 워밍업 프로젝트: ssort (qsort + 비교 함수/함수 포인터, void** 캐스팅, realloc 동적 배열)
- [x] Phase 1: CPU Virtualization (Ch 4~10)
  - [x] Ch 4 Process (프로그램 vs 프로세스, machine state, 상태 Running/Ready/Blocked, PCB, 생성 5단계)
  - [x] Ch 5 Process API (fork/exec/wait, fd 리다이렉션, 셸=프로세스/프로세스 트리, 숙제 q1~q8)
  - [x] Ch 6 Limited Direct Execution (트랩·트랩 테이블, 타이머 인터럽트, trapframe vs context, xv6 swtch 매핑, 숙제: 시스템콜 172ns vs 문맥교환 1.2μs 실측)
  - [x] Ch 7 Scheduling: Introduction (FIFO/SJF/STCF/RR, 반환 ↔ 응답 트레이드오프)
  - [x] Ch 8 MLFQ (5규칙, starvation/gaming/부스트, 정책 vs 메커니즘)
  - [x] Ch 9 Proportional Share (Lottery/Stride, CFS는 stride의 실전판)
  - [x] Ch 10 Multiprocessor Scheduling (SQMS/MQMS, 캐시 친화성, work stealing이 손해인 반례 실측) — **Concurrency 후 재방문 예정**
- [x] 프로젝트: processes-shell **wish** (fork/execv/wait, access로 path 탐색, dup2 리다이렉션, & 병렬 — "개념→코드" 간극 해소)
- [ ] Phase 2: Memory Virtualization (Ch 13~23) ← 진행 중
  - [x] Ch 13 Address Spaces (세 목표, 코드/힙/스택 배치, VSZ vs RSS로 lazy allocation 실측, pmap)
  - [x] Ch 14 Memory API (버그 5종과 UB, brk/mmap, gdb/valgrind/ASan 실습, shadow memory)
  - [x] Ch 15 Address Translation (base/bounds, "동적"=실행 시점 번역, HW vs OS 책임, 신뢰 경계)
  - [x] Ch 16 Segmentation (명시적/암묵적 판별, 감소 세그먼트 오프셋, 코드 공유, 외부 단편화)
  - [x] Ch 17 Free Space Management (헤더, 임베디드 free list, BEST/WORST/FIRST 실측, SIZESORT++FIRST=BEST 반례 검증, 탐색 vs 병합 트레이드오프)
  - [ ] Ch 18 Paging: Intro ← **지금**
  - [ ] Ch 19 TLB
  - [ ] Ch 20 Advanced Page Tables
  - [ ] Ch 21~22 Swapping
- [ ] Phase 3: Concurrency (Ch 26~34) → 끝나고 Ch 10 재방문
- [ ] Phase 4: xv6 커널 분석
- [ ] Phase 5: Persistence (Ch 36~43)
