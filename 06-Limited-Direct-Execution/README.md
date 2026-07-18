# OSTEP Ch 6 — Limited Direct Execution 정리

프로그램을 **빠르게(직접 실행)** 돌리면서 OS가 **통제권**을 유지하는 법.
"direct execution"(그냥 CPU에서 실행) + "limited"(제한을 걸어 OS가 통제).

---

## 핵심: 두 문제 + 두 해법

| 문제 | 해법 |
|------|------|
| ① 프로그램이 위험한 짓(I/O·다른 메모리·하드웨어) 하면? | **사용자/커널 모드 + 트랩(시스템콜) + 트랩 테이블** |
| ② 프로그램이 CPU 안 놓으면(Spin)? | **타이머 인터럽트(강제 선점) + context switch** |

- cpu.c의 "OS가 어떻게 CPU를 되찾나" + Ch5의 "시스템콜이 어떻게 커널에 들어가나" = **둘 다 여기서 풀림.**

---

## 문제 1 — 제한된 연산 (사용자/커널 모드 + 트랩)

- CPU에 **최소 두 모드**: **사용자 모드(제한)** / **커널 모드(특권)**.
- 사용자 프로그램은 사용자 모드로 제한 실행 → 위험한 짓 못 함.
- 근데 정당한 특권 연산(파일 읽기 등)은? → **트랩(시스템콜)**으로 커널에 "부탁".

**트랩 메커니즘:**
1. 사용자 코드에 **이미 박혀 있는** 트랩 명령(`int`/`syscall`/`ecall`) 실행 (하드웨어가 만드는 게 아님).
2. 하드웨어: 특권 상향 + 커널 스택 전환 + 일부 상태 저장 + **트랩 테이블** 보고 정해진 핸들러로 점프.
3. 커널 핸들러: **시스템콜 번호**(레지스터) 읽어 **검증** 후 디스패치.
4. **return-from-trap**(`iret`/`sret`) → 특권 하향 + 사용자 재개.

**트랩 테이블:**
- **부팅 시(커널 모드, 특권)** OS가 세팅 — "각 트랩 번호 → 핸들러 주소" 매핑. 사용자는 못 건드림.
- 구조 = **직접 인덱스 배열**(해시 아님). `trapno`로 인덱싱.
- **보안**: 사용자는 목적지를 못 고름. 트랩 "발생"만 시키고, **어디로 갈지는 하드웨어+테이블이 결정.** 사용자는 시스템콜 **번호**만 넘김.

**유효성 검사 분담:**
- **하드웨어**: 특권 위반·잘못된 메모리 접근 감지, 정해진 입구로만 진입.
- **커널(소프트웨어)**: 시스템콜 번호·인자 유효성. (엉뚱한 번호 → -1 에러, 점프 안 함.)

**모드 vs 상태 (혼동 주의):**
- **모드(user/kernel)** = 특권 레벨. 시스템콜 = **모드 전환**(커널 모드로), 프로세스는 여전히 Running.
- **상태(Running/Ready/Blocked)** = CPU에 있나/기다리나. 시스템콜이 **느린 I/O로 대기해야 할 때만** Blocked.
- 즉 **"시스템콜 = 커널 모드 진입"이지 "시스템콜 = Blocked"가 아님.**

---

## 문제 2 — 통제권 (타이머 인터럽트 + 선점)

- **협조적**: 프로세스 자발적 양보에 의존 → 무한 루프면 OS가 영영 못 되찾음 → **시스템 멈춤.** (옛 Mac OS, Win 3.x)
- **비협조적(현대)**: OS가 부팅 때 **타이머**를 세팅 → 몇 ms마다 **인터럽트** → 프로그램이 뭘 하든 **강제로 커널로** → 선점.

**선점(preemption)하는 이유:**
1. **통제권 확보**: 폭주 프로세스로부터 CPU 되찾기.
2. **공정한 시분할**: CPU를 여러 프로세스에 번갈아 나눠줌 (cpu.c의 "동시 실행 환상").

**트랩 vs 인터럽트 (진입 경로는 공유, 트리거만 다름):**
- **트랩/예외** = 동기적(현재 명령이 유발: 시스템콜, page fault).
- **인터럽트** = 비동기적(외부 하드웨어: 타이머, 디스크).
- 하드웨어 처리 절차는 거의 같고, `trapno`로 종류 구분해 다른 핸들러로.

---

## Context Switch — 두 층의 저장

프로세스를 멈췄다 재개하려면 상태 저장. **두 종류의 저장이 있음:**

| | trapframe | context |
|--|-----------|---------|
| 용도 | 트랩 (사용자 ↔ 커널) | swtch (커널 ↔ 커널) |
| 저장 | **레지스터 전체** | **callee-saved만** (5개) |
| 이유 | 경계 넘기(비동기, 전체 보존 필요) | 정식 C 함수 호출(규약 적용) |
| 위치 | **커널 스택** | struct context (커널 스택 top) |

**왜 다른가 (핵심):**
- **트랩** = 임의 지점에서 강제 중단 → calling convention이 안 통함(함수 호출 아님, 경계 넘음, context switch 가능) → **전체 저장**.
- **swtch** = 커널 코드 안의 정식 함수 호출 → caller-saved는 컴파일러가 이미 저장 → swtch는 **callee-saved만**.
- (Ch4에서 "callee-saved만 저장하면 안 되나" 물었던 게 정확히 이 두 층.)

**하드웨어 vs 소프트웨어 분담:**
- 진입: 하드웨어가 일부(eip/cs/eflags/esp 등) 자동 저장 + 커널 스텁이 `pushal`로 나머지.
- 복귀: 커널 스텁이 `popal`로 복원 + **`iret`**(전용 명령)이 특권 하향+재개를 **원자적으로**.
- 특권 전환은 오직 트랩(올림)/iret(내림)으로만 — 사용자가 일반 명령으로 특권 못 올림(보안).

---

## A → B 전환 전체 흐름 (A ↔ 스케줄러 ↔ B)

**핵심: 프로세스끼리 직접 안 감. 항상 스케줄러 경유. swtch 두 번.**

```
① A 사용자 실행 중 (OS 안 돎)
② 타이머 인터럽트 (A 협조 없이 강제)
③ 하드웨어+커널스텁: A 레지스터 전체 → trapframe (커널 스택)   [저장1: 전체]
④ trap(): tf->trapno = 타이머 → yield()
⑤ yield(): A state RUNNING→RUNNABLE, sched()
⑥ sched(): swtch(&A->context, scheduler)  → A→스케줄러          [저장2: callee 5개]
⑦ scheduler(): RUNNABLE한 B 선택 → switchuvm(B): CR3 ← B->pgdir
⑧ scheduler(): swtch(&scheduler, B->context) → 스케줄러→B       [저장2]
⑨ B가 자기 옛 sched()의 swtch 다음 줄에서 깨어남 (커널 모드)
⑩ B: sched→yield→trap 리턴 → trapret → iret → B 사용자 재개    [복원1]
```

- **저장1 (trapframe, 전체)**: ③(진입)/⑩(복귀) — 사용자↔커널
- **저장2 (context, 5개)**: ⑥⑧ — 커널스레드↔커널스레드 (swtch)
- **CR3 교체**: ⑦ switchuvm — B의 주소 공간으로 (안 하면 B가 A 메모리를 봄 → 격리 붕괴)

---

## xv6 코드 매핑 (x86, xv6-public)

### 구조체 (proc.h)
- **`struct proc` = PCB**: `pgdir`(페이지테이블), `kstack`(커널스택 **포인터**), `state`, `tf`(trapframe **포인터**), `context`(**포인터**), `ofile[]`(fd 테이블). → **프로세스 테이블(ptable.proc[])에 상주.**
- **`struct trapframe`**: 레지스터 전체 스냅샷. **커널 스택에** 저장. `tf`가 가리킴.
- **`struct context`**: `edi/esi/ebx/ebp/eip` = **callee-saved 5개만.** (주석: "caller가 eax/ecx/edx 이미 저장").

### 진입/복귀 (trapasm.S)
- **`alltraps`**: `pushl ds/es/fs/gs` + `pushal`(전체 저장) → trapframe 완성 → `call trap` (esp=tf 인자).
- **`trapret`**: `popal` + `popl` 세그먼트 → `addl $0x8`(trapno/err 건너뛰기) → **`iret`**(return-from-trap).

### 분기 (trap.c)
- `trap(tf)`: `tf->trapno`로 분기. `T_SYSCALL` → `syscall()`, 타이머 → `yield()`, 그 외 → 예외 처리.

### swtch (swtch.S) — 저장2의 실체
```asm
swtch:
  movl 4(%esp), %eax    # eax = old (32비트라 인자가 스택으로! 4(%esp))
  movl 8(%esp), %edx    # edx = new
  pushl %ebp; %ebx; %esi; %edi   # callee-saved 4개 push (+call이 넣은 eip = 5개)
  movl %esp, (%eax)     # ★ 현재 esp를 *old에 저장 ("A 문맥 여기 있음")
  movl %edx, %esp       # ★ esp를 new로 교체 ("스택 전환 = 프로세스 전환")
  popl %edi; %esi; %ebx; %ebp    # new의 callee-saved 복원
  ret                   # ★ new의 eip로 점프 (옛 멈춤 지점에서 깨어남)
```
- **핵심**: swtch = "**esp를 old에 저장하고 new로 갈아끼우는 것**". 스택 바꾸면 문맥 통째로 바뀜.
- **eip는 명시적 push 안 함** → `call swtch`가 리턴주소(eip)를 스택에 넣어둠, `ret`이 꺼내 점프. 그래서 "예전 swtch 다음 줄"에서 깨어남.
- (32비트 x86: 인자 스택 전달 → `4(%esp)`. 64비트면 rdi/rsi. RISC-V면 a0/a1 + s0~s11.)

### 스케줄러 (proc.c)
- **`yield()`**: state=RUNNABLE → `sched()`.
- **`sched()`**: `swtch(&p->context, cpu->scheduler)` → **프로세스→스케줄러** (swtch #1).
- **`scheduler()`**: 무한 루프. RUNNABLE한 p 찾기 → `switchuvm(p)`(CR3 교체) → state=RUNNING → `swtch(&cpu->scheduler, p->context)` → **스케줄러→프로세스** (swtch #2) → 돌아오면 `switchkvm()`.
- **대칭**: 각 swtch는 짝을 이뤄 "예전에 나갔던 지점"에서 만남.

---

## 오늘 역질문으로 푼 것 (헷갈렸던 것)

- **PCB vs trapframe 위치**: PCB = **프로세스 테이블**에 상주(커널 스택 아님). trapframe = **커널 스택**에 저장. PCB의 `tf`가 trapframe을 가리킴. (내가 반대로 알았다가 잡음.)
- **swtch는 caller가 아니라 callee-saved 저장**: 정식 함수라 caller-saved는 컴파일러가 이미 처리.
- **CR3는 커널 스택 pop이 아니라 PCB의 pgdir에서**: 페이지테이블 주소는 프로세스 고정 속성이라 PCB 필드.
- **B는 사용자 아니라 커널(sched 직후)에서 깨어남**: 예전에 swtch로 멈춘 그 지점.
- **iret은 명령어이자 하드웨어 특권 동작**: pop(소프트웨어)으로 레지스터 복원 후, iret이 특권 하향+재개를 원자적으로.
- **트랩 명령은 하드웨어가 만드는 게 아니라 코드에 이미 있고 CPU가 실행**. 번호 레지스터는 커널이 읽는 요청서, 트랩 명령이 트리거.
- **하드웨어는 시스템콜 번호 검증 안 함**: 항상 같은 입구로 보내고, 번호 유효성은 커널이 검사.

---

## 다음
- Ch 6 숙제 (시스템콜·문맥교환 비용 측정) → Ch 7 Scheduling
- xv6 남은 것: `scheduler()`/`sched()`는 봤고, 나중에 Phase 4에서 RISC-V로 랩.
- 전체 로드맵: `OSTEP_xv6_로드맵.md`
