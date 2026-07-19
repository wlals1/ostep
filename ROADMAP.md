# OSTEP → xv6 학습 로드맵

> 목표: 정답 암기가 아니라 **"왜"를 설명할 수 있는 멘탈 모델** 구축 → xv6 커널 코드 분석까지.
> 학습 방식: 각 챕터는 (1) 개념 직관 → (2) 책의 코드/homework 직접 실행 → (3) 변형 문제로 재현 확인.

---

## 진단 요약 (2026-07-16 작성, 2026-07-19 갱신)

- **현재 위치**: Phase 1 CPU Virtualization 완료 (Ch 4~9). 다음은 Ch 10 Multiprocessor Scheduling(선택) 또는 Phase 2 Memory Virtualization.
- **강점**: 논리적 추론력 중상급. 반례 생성·논리 반박 잘함. 알고리즘은 골드 수준.
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
- **남겨둔 질문(지금 답 안 해도 됨)**: 가상→물리 번역에 왜 하드웨어(MMU)가 필수인가 → Ch 15에서 정식으로.

## Phase 1 — CPU Virtualization (Ch 4~10)

여기서 2장에서 못 답한 "context switch가 정확히 어떻게 일어나나"가 풀린다.

- **Ch 4 The Abstraction: Process** — 프로세스란 무엇인가, 상태(Running/Ready/Blocked).
- **Ch 5 Process API** — `fork()`, `exec()`, `wait()`. ⭐ 손으로 코드 많이 짜볼 것. 네가 좋아할 챕터.
- **Ch 6 Limited Direct Execution** — user/kernel mode, trap, system call, timer interrupt. ⭐ **2장에서 어렵던 "OS가 어떻게 CPU를 뺏나"의 정답.**
- **Ch 7 Scheduling: Intro** — FIFO, SJF, STCF, 응답시간 vs 반환시간.
- **Ch 8 MLFQ** — 실전 스케줄러의 원형.
- **Ch 9 Lottery/Stride** — 비례 배분 스케줄링.
- **Ch 10 Multiprocessor Scheduling** (선택, 나중에 돌아와도 됨).
- **체크포인트**: "타이머 인터럽트 → 커널 진입 → 스케줄러 → context switch" 흐름을 그림 없이 말로 설명할 수 있으면 통과.

## Phase 2 — Memory Virtualization (Ch 13~23)

⭐ **2장에서 널 무너뜨린 CR3 / 페이지 테이블 / 프로세스별 매핑 질문이 전부 여기서 정식으로 답해진다.** 그때 어려웠던 건 실력이 아니라 이 챕터를 안 배웠기 때문.

- **Ch 13 Address Spaces** — 주소 공간 추상화.
- **Ch 14 Memory API** — `malloc`/`free`, 흔한 메모리 버그(누수, dangling, double free).
- **Ch 15 Address Translation** — base/bound, 하드웨어 지원의 시작. (MMU가 왜 필요한지 여기서 정답)
- **Ch 16 Segmentation**.
- **Ch 17 Free Space Management** — free list, 할당자 내부.
- **Ch 18 Paging: Intro** — 페이지, 프레임, 페이지 테이블. ⭐
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

## 6주 스케줄 (하루 4시간 / 40일, 2026-07-16 시작)

> 총 예산 ~160h. 진도보다 **깊이 우선** — 체크포인트에서 막히면 그 자리에서 복습, 진도 미루기.
> 하루 4h 중 3h 학습(읽기+코드) + 1h는 복습/변형 문제/막힌 곳 정리 권장.

### Week 1 — 기초 + CPU 가상화 (Ch 2 마무리 ~ Ch 9)
- Ch 2: cpu.c, threads.c 실습 마무리
- Ch 4 Process → Ch 5 Process API(fork/exec/wait, 코드 많이) → Ch 6 Limited Direct Execution ⭐
- Ch 7 Scheduling → Ch 8 MLFQ → Ch 9 Lottery
- **주말 체크포인트**: "타이머 인터럽트 → context switch" 말로 설명

### Week 2 — 메모리 가상화 ① (Ch 13~18)
- Ch 13 Address Spaces → Ch 14 Memory API(malloc 버그) → Ch 15 Address Translation ⭐(MMU 정답)
- Ch 16 Segmentation → Ch 17 Free Space → Ch 18 Paging Intro ⭐

### Week 3 — 메모리 가상화 ② (Ch 19~22 + 종합)
- Ch 19 TLB → Ch 20 Multi-level Page Table ⭐(CR3가 가리키는 구조)
- Ch 21~22 Swapping
- **체크포인트**: 2장의 CR3/페이지테이블 질문 완전 회수 — 같은 가상주소가 안 섞이는 이유 설명

### Week 4 — 동시성 (Ch 26~32)
- Ch 26 Intro → Ch 27 Thread API → Ch 28 Locks(CAS/test-and-set)
- Ch 29 Lock-based DS → Ch 30 Condition Vars → Ch 31 Semaphores ⭐ → Ch 32 Bugs/Deadlock
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

- [x] Ch 2 Introduction — mem.c, cpu.c, threads.c, io.c (2장 완료)
- [x] 워밍업 프로젝트: reverse (파일 I/O, 연결 리스트, strdup 소유권, stat/inode, valgrind 클린)
- [x] 워밍업 프로젝트: unix-utilities (wcat, wgrep=strstr, wzip=fwrite/RLE, wunzip=fread, 왕복 검증)
- [x] 워밍업 프로젝트: ssort (qsort + 비교 함수/함수 포인터, void** 캐스팅, realloc 동적 배열)
- [x] Phase 1: CPU Virtualization (Ch 4~9, Ch 10은 선택으로 남김)
  - [x] Ch 4 Process (프로그램 vs 프로세스, machine state, 상태 Running/Ready/Blocked, PCB, 생성 5단계)
  - [x] Ch 5 Process API (fork/exec/wait, fd 리다이렉션, 셸=프로세스/프로세스 트리, 숙제 q1~q8)
  - [x] Ch 6 Limited Direct Execution (트랩·트랩 테이블, 타이머 인터럽트, trapframe vs context, xv6 swtch 매핑, 숙제: 시스템콜 172ns vs 문맥교환 1.2μs 실측)
  - [x] Ch 7 Scheduling: Introduction (FIFO/SJF/STCF/RR, 반환 ↔ 응답 트레이드오프)
  - [x] Ch 8 MLFQ (5규칙, starvation/gaming/부스트, 정책 vs 메커니즘)
  - [x] Ch 9 Proportional Share (Lottery/Stride, CFS는 stride의 실전판)
  - [ ] Ch 10 Multiprocessor Scheduling (선택) ← 다음 또는 Phase 2로
- [ ] Phase 2: Memory Virtualization (Ch 13~23)
- [ ] Phase 3: Concurrency (Ch 26~34)
- [ ] Phase 4: xv6 커널 분석
- [ ] Phase 5: Persistence (Ch 36~43)
