# xv6 학습 로드맵 (Phase 4)

> 목표: **"OSTEP에서 배운 개념 X가 실제 커널의 어느 코드로 구현됐나"**를 매핑하며 읽기.
> 커널을 처음부터 만드는 게 아니라, **이미 아는 개념의 실물을 확인**하는 것.

---

## ★ 학습 규칙 (1일차에 얻은 교훈)

1. **한 세션에 파일 하나, 많아야 함수 세 개.** xv6는 급할 게 없다.
2. **한 번에 개념 하나만.** 여러 개를 동시에 던지면 무너진다 (ROADMAP 규칙 7).
3. **모든 줄을 이해하려 하지 말 것.** CSR 조작 같은 하드웨어 세부는 "무슨 일을 하려는 코드인지"만 읽고 넘어간다.
4. **각 세션 끝에 "설명할 수 있다/들어봤다/모르겠다"로 자가 점검.** 모르겠다가 나오면 진도 대신 복습.
5. **OSTEP 노트를 옆에 두고 대조.** 새로 배우는 게 아니라 회수하는 것.

### 읽기 3단계
```
1차: "이 파일이 무슨 일을 하나" — 함수 이름과 주석만
2차: "내가 아는 개념이 어디 있나" — Ch6 트랩, Ch17 free list, Ch28 락
3차: 특정 기능을 파고들 때 그 부분만 정밀하게
```

---

## 환경

```bash
cd ~/programming/ostep/xv6-riscv
make qemu           # 실행.  종료: Ctrl-A 떼고 x
make CPUS=1 qemu    # 단일 코어로 (실험용)
make qemu-gdb       # 디버깅 모드 (다른 터미널에서 gdb-multiarch 붙이기)
```

```
~/programming/ostep/
├── xv6-riscv/      ← MIT 소스. 추적 안 함
└── mine/xv6/       ← 내 노트. 랩 하면 고친 파일만 복사해 추적
```

---

## 단계별 계획

### ✅ 1단계 — 부팅 (완료)

| 파일 | 내용 | 대조 |
|--|--|--|
| `entry.S` | 코어마다 스택 세팅 → `start()` | Ch26 "실행 흐름 N개면 스택 N개" |
| `start.c` | M mode 설정 → `mret`으로 S mode의 `main()`으로 | Ch6 트랩, Ch15 특권 |
| `main.c` | hart 0이 전역 초기화, 나머지는 코어별 초기화 | Ch32 순서 위반, Ch10 per-CPU |
| `kernel.ld` | 0x80000000에 배치, `end` 심볼 생성 | — |

**노트:** `xv6-01-부팅.md`

### ✅ 2단계 — 물리 메모리 + 락 (일부 완료)

| 파일 | 내용 | 대조 | 상태 |
|--|--|--|--|
| `kalloc.c` | 4KB 페이지 단위 free list | **malloc lab**, Ch17, Ch18 | ✅ |
| `spinlock.c` | TestAndSet 스핀락 + `push_off` + 배리어 | Ch28 | ✅ |
| `sleeplock.c` | 잠들 수 있는 락 (내부에 spinlock) | Ch28 park/unpark | ⬜ |

**남은 것:** `sleeplock.c` (짧음)

### 🔶 3단계 — 가상 메모리 (진행 중)

| 함수/파일 | 내용 | 대조 | 상태 |
|--|--|--|--|
| `vm.c: walk()` | 3단계 페이지 테이블 순회 | **Ch20 멀티레벨 walk** | ✅ |
| `vm.c: mappages()` | va~va+size를 pa에 매핑 | Ch18 | ✅ |
| `memlayout.h` | 물리 메모리 배치도 | Ch13 | 🔶 부분 |
| `vm.c: kvmmake()` | 커널 페이지 테이블 구축 | Ch16 보호 비트, Ch13 배치 | ⬜ |
| `vm.c: uvmalloc/uvmfree` | 사용자 주소 공간 늘리고 줄이기 | Ch13, Ch14 `sbrk` | ⬜ |
| `vm.c: uvmcopy()` | fork 시 주소 공간 복사 | Ch5 fork | ⬜ |
| `vm.c: copyin/copyout` | 커널↔사용자 데이터 이동 | **Ch15 신뢰 경계** ★ | ⬜ |

**`copyin`/`copyout`이 특히 중요** — Ch15에서 *"커널은 사용자가 넘긴 포인터를 절대 신뢰하지 않는다"*고 한 그 검증이 여기 있음.

### ⬜ 4단계 — 트랩과 시스템콜

| 파일 | 내용 | 대조 |
|--|--|--|
| `trap.c` | 트랩 진입점, 타이머·장치 인터럽트 처리 | **Ch6 LDE** ★ |
| `trampoline.S` | 사용자↔커널 전환 어셈블리 | Ch6 trapframe |
| `syscall.c` | 시스템콜 번호 → 핸들러 디스패치 | Ch5, Ch15 |
| `sysproc.c` | fork/exit/wait/sbrk 등 구현 | Ch5, Ch14 |

**Ch6에서 배운 "타이머 인터럽트 → 커널 진입 → 스케줄러 → 문맥 교환"의 앞부분.**

### ⬜ 5단계 — 프로세스와 스케줄링

| 파일 | 내용 | 대조 |
|--|--|--|
| `proc.c: scheduler()` | 각 CPU의 스케줄러 루프 | **Ch7~10** ★ |
| `swtch.S` | 문맥 교환 어셈블리 | **Ch6 context** ★ |
| `proc.c: fork/exit/wait` | 프로세스 생성·종료 | Ch4, Ch5 |
| `proc.c: sleep/wakeup` | 커널의 조건 변수 | **Ch30** ★ |

**`sleep`/`wakeup`이 Ch30의 조건 변수 그 자체** — lost wakeup을 어떻게 막는지 확인.

### ⬜ 6단계 — 파일 시스템 (Phase 5와 병행)

| 파일 | 내용 | 대조 |
|--|--|--|
| `bio.c` | 버퍼 캐시 | Ch21 "메모리는 디스크의 캐시" |
| `fs.c` | inode, 디렉터리, 블록 할당 | Ch39~40 |
| `log.c` | 저널링 | Ch42 |
| `file.c` | 파일 디스크립터 | Ch5, Ch39 |

**Ch36~42를 읽으면서 대조하는 게 효율적.** 먼저 개념, 그다음 코드.

### ⬜ 7단계 — 직접 고치기 (랩)

```
ostep-projects: scheduling-xv6-lottery  (Ch9 복권 스케줄러 구현)
                vm-xv6-intro            (널 포인터 역참조 감지)
MIT 6.1810 labs: util, syscall, pgtbl, traps, cow, thread, ...
```

**읽기만으로는 안 남아.** 최소 하나는 직접 고쳐볼 것.

---

## 체크포인트

**중간 (5단계 후):** 다음을 코드 없이 말로 설명
```
사용자 프로그램이 read()를 호출하면 무슨 일이 일어나나?
  → 시스템콜 명령 → 트랩 → trampoline → trap.c → syscall.c → sys_read
  → 디스크 대기 → sleep() → 스케줄러가 다른 프로세스로 → ... → wakeup
  → 원래 프로세스 재개 → sret으로 사용자 복귀
```

**최종 (7단계 후):** 랩 하나를 완성하고, 고친 부분이 왜 그렇게 되어야 하는지 설명

---

## 페이스 제안

```
세션당 1~2시간, 파일 하나
1단계 ✅  2단계 (남은 sleeplock)  →  3단계 (4~5세션)  →  4단계 (3~4세션)
5단계 (4~5세션)  →  6단계는 Phase 5와 함께  →  7단계 랩
```

**진도보다 "설명할 수 있는가"가 기준.** 막히면 그 자리에서 복습.

---

## 참고

- xv6 book (riscv): https://pdos.csail.mit.edu/6.828/2024/xv6/book-riscv-rev4.pdf
- MIT 6.1810: https://pdos.csail.mit.edu/6.828/2024/schedule.html
- 소스: `~/programming/ostep/xv6-riscv/kernel/`

전체 로드맵: [ROADMAP.md](../ROADMAP.md)
