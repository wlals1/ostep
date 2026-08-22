# xv6 ① — 부팅 · 물리 메모리 · 락 · walk

첫 세션. `entry.S → start.c → main.c → kalloc.c → spinlock.c → vm.c`

---

## ★ 이 세션의 다섯 축

```
① 부팅은 M mode에서 시작 → mret으로 S mode의 main()으로 (딱 한 번)
② 트랩을 S mode로 위임해야 커널이 받을 수 있음
③ 타이머 인터럽트를 켜야 OS가 CPU를 되찾을 수 있음 (Ch6)
④ 실행 흐름 N개면 스택 N개 (entry.S가 코어마다 스택 세팅)
⑤ 낮은 권한으로 가는 유일한 길은 xret — 상태를 위조해 재활용
```

---

## 환경

```
hart      = hardware thread = CPU 코어 (RISC-V 용어)
QEMU      = RISC-V 하드웨어를 소프트웨어로 흉내내는 에뮬레이터
Makefile  : CPUS := 3,  -smp 3 -m 128M -bios none
```

**QEMU를 쓰는 이유:** 커널이 죽어도 안전 / RISC-V 하드웨어가 없어도 됨 / **gdb로 커널을 한 줄씩 디버깅 가능** / 코어 수·메모리를 마음대로 바꿔 실험 가능.

**RISC-V인 이유:** x86은 하위 호환 유산(세그먼트 잔재 등)이 많아 지저분함. RISC-V는 깔끔해서 커널의 본질에 집중 가능.

---

## `entry.S` — 코어마다 스택

```asm
la sp, stack0
li a0, 1024*4
csrr a1, mhartid          # 내 hart 번호 (하드웨어가 코어마다 다른 값을 줌)
addi a1, a1, 1
mul a0, a0, a1
add sp, sp, a0            # sp = stack0 + 4096*(hartid+1)
call start
```

**RISC-V 레지스터:** `sp`(스택 포인터, x86의 rsp), `a0~a7`(인자·임시), `csrr`(제어·상태 레지스터 읽기)

**`(hartid+1)`인 이유:** 스택은 아래로 자라니까(Ch13) sp를 자기 영역의 **꼭대기**에 놓아야 함.
```
stack0        stack0+4096      stack0+8192
  ├──hart0의 스택──┼──hart1의 스택──┤
              ↑hart0의 sp      ↑hart1의 sp
```

### ★ 왜 C로 못 하나 (틀렸던 지점)

**절대주소/가상주소 문제가 아님.** → **C 함수 호출에는 스택이 이미 있어야 하기 때문.**
```
함수 호출 → 반환 주소를 스택에 push
지역 변수 → 스택에 할당
```
부팅 직후 `sp`는 쓰레기값. 이 상태로 C 함수를 부르면 아무 데나 써서 즉사.
**"스택을 세팅하는 코드"가 스택을 필요로 하면 안 되므로 어셈블리여야 함.**

> **부트스트랩 패턴 네 번째:**
> ```
> 락을 만들려면 원자성 필요      → 하드웨어 원시연산이 바닥
> page walk에 번역 필요          → 물리 주소가 바닥
> 미스 핸들러에 번역 필요        → wired 엔트리가 바닥
> C를 실행하려면 스택 필요       → 어셈블리가 바닥  ★
> ```
> **메커니즘은 자기 자신에 의존할 수 없다.**

`spin: j spin`은 도달 불가능한 안전장치 (`start()`는 반환하지 않음).

---

## RISC-V 특권 레벨

```
M mode (Machine)     최고. 펌웨어·부팅용
S mode (Supervisor)  ★ 커널이 사는 곳. 페이징(satp), 커널 트랩
U mode (User)        사용자 프로그램
```
x86의 ring 0/3과 비슷하되 한 층 더.

**커널이 S mode인 이유:**
1. **페이징이 S mode의 기능** — `satp`(x86의 CR3)와 페이지 테이블이 S mode용. M mode에 머물면 Phase 2를 못 씀
2. 최소 권한 원칙

---

## `start.c` — M mode 설정 후 S mode로

```c
x |= MSTATUS_MPP_S;   w_mstatus(x);   // ① "이전 특권 = S mode"로 기록
w_mepc((uint64)main);                  // ② "예외 발생 지점 = main"으로 기록
w_satp(0);                             //   페이징 끔 (main의 kvminithart가 켬)
w_medeleg(0xffff); w_mideleg(0xffff);  // ③ 트랩을 S mode로 위임
w_sie(... SEIE | STIE);                //   S mode 인터럽트 허용
w_pmpaddr0(...); w_pmpcfg0(0xf);       //   S mode가 물리 메모리 전체 접근 가능
timerinit();                           // ④ 첫 타이머 인터럽트 예약
w_tp(r_mhartid());                     // ⑤ hart 번호를 tp 레지스터에 저장
asm volatile("mret");                  // ⑥ S mode의 main()으로
```

### ★ `mret` 트릭 (틀렸던 지점)

**`mret`은 트랩을 "발생시키는" 게 아니라 "트랩에서 복귀하는" 명령.**
```
트랩  : 낮은 권한 → 높은 권한 (올라감)
mret  : 높은 권한 → 낮은 권한 (내려감)  ★
```

**핵심: 권한을 낮추는 방법이 이것뿐.**
```c
main();   // ❌ 그냥 부르면 M mode에서 실행됨. 모드가 안 바뀜
```
Ch15의 그 원칙 — **모드 비트를 직접 못 바꾸고, 정해진 경로로만 전환.**
RISC-V에선 그 경로가 **트랩(올라감)과 `xret`(내려감)** 둘뿐.

**그래서 상태를 위조:** "S mode의 `main()`에서 트랩이 왔던 것처럼" 만들고 복귀 명령을 실행.
**한 번도 가본 적 없는 곳으로 "돌아가는" 것.**

> 같은 트릭이 계속 나옴 — `userinit()`이 첫 사용자 프로세스를 만들 때 가짜 trapframe을 만들고 `sret`으로 U mode로 "복귀". 스케줄러가 프로세스를 처음 실행할 때도 마찬가지.
> **"정상 흐름의 복귀 경로를, 상태를 위조해서 시작 경로로 재활용한다."**

### 위임(delegation)이란

**"일이 들어온 뒤 넘기는" 게 아니라 "애초에 나한테 오지 않게 배선을 바꾸는" 것.**

각 특권 레벨이 자기 트랩 핸들러를 가짐 (`mtvec` / `stvec`).
```
기본값     : 모든 트랩 → M mode의 mtvec
위임 후    : 시스템콜 → S mode의 stvec로 직행. M mode를 아예 안 거침 ✅
```

**왜 기본값이 M mode인가:** 실제 시스템엔 커널 아래 **펌웨어 층(OpenSBI 등)**이 있고, 일부 트랩을 가로채야 할 수 있음. xv6는 `-bios none`으로 펌웨어를 안 쓰고, 자기가 최소 M mode 코드를 두고 전부 위임한 뒤 **다시는 M mode로 안 돌아감.**

**목적은 성능:** 위임 없이 매번 M mode를 거치면 모드 전환이 두 배. 시스템콜은 초당 수만 번.
(Ch15의 *"매 명령마다 OS가 개입하면 수천 배 느려지니 하드웨어에 박아둔다"*와 같은 논리)
**권한 관계는 유지** — 위임 설정은 M mode만 가능(특권 명령).

### `timerinit()` — Ch6의 그것

```c
w_stimecmp(r_time() + 1000000);   // 첫 타이머 인터럽트 예약
```
**"OS가 사용자에게 CPU를 넘긴 뒤 어떻게 되찾나"의 답.**
```c
while (1) ;   // 시스템콜도 안 하는 프로그램이 CPU를 잡으면?
```
타이머 인터럽트가 강제로 커널로 끌어옴 → 스케줄러 실행 가능.
**부팅 때 첫 알람을 맞춰두고, 이후 핸들러가 매번 다음 알람을 다시 맞춤.**
**사용자 프로세스를 만들기 전에 "되찾을 수단"부터 마련하는 것.**

### `w_tp(id)`

`mhartid`는 **M mode에서만 읽을 수 있는 CSR.** S mode로 내려가면 못 읽음.
→ **아직 읽을 수 있을 때 읽어서 범용 레지스터 `tp`에 저장.** `main.c`의 `cpuid()`가 이걸 읽음.
**Ch10 per-CPU 데이터의 가장 원초적인 형태.**

---

## `main.c` — hart 0만 전역 초기화

```c
volatile static int started = 0;

if (cpuid() == 0) {
    kinit();       // 물리 페이지 할당기      ← Ch17, kalloc.c
    kvminit();     // 커널 페이지 테이블 생성   ← Ch18~20
    kvminithart(); // 페이징 켜기 (satp)       ← Ch18
    procinit();    // 프로세스 테이블          ← Ch4
    trapinit();    // 트랩 벡터               ← Ch6
    ... binit, iinit, fileinit, virtio_disk_init ...
    userinit();    // 첫 사용자 프로세스
    __atomic_store_n(&started, 1, __ATOMIC_RELEASE);
} else {
    while (__atomic_load_n(&started, __ATOMIC_ACQUIRE) == 0) ;
    kvminithart();   // 페이징 켜기      ← 코어마다
    trapinithart();  // 트랩 벡터        ← 코어마다
    plicinithart();  // 인터럽트 설정     ← 코어마다
}
scheduler();
```

**함수 이름 끝의 `hart`가 구분선:**
```
kvminit      한 번만 (페이지 테이블은 하나)
kvminithart  코어마다 (그걸 가리키는 satp 레지스터는 코어마다 따로)
```
**Ch10 재방문의 "캐시·TLB는 코어 전용"과 같은 이유.**

**부팅 메시지 `hart 2 starting`이 `hart 1`보다 먼저 나오는 이유:** 순서를 강제하지 않았고 그럴 이유도 없음. 실행마다 바뀔 수 있음. (Ch27의 A/B 72:28과 같은 얘기)

### ★ `started` — 락이 아니라 순서 강제

**Ch32의 "순서 위반(order violation)"을 막는 코드.** hart 1이 페이지 테이블도 없이 `kvminithart()`를 부르면 죽음.
Ch27 `main-signal.c`의 스핀 대기와 같은 구조. **부팅 때 딱 한 번, 아주 짧게라 스핀이 정당**하고, 조건 변수를 쓰려면 그것부터 초기화돼야 하는 부트스트랩 문제도 있음.

### ★ volatile만으로는 왜 부족한가 (중요)

**`volatile` = "최적화하지 말고 매번 실제 메모리를 읽고 써라".**
없으면:
```c
while (started == 0) ;
// 컴파일러: "루프 안에서 started를 바꾸는 코드가 없네"
// → 레지스터에 한 번만 읽고 무한 루프로 최적화 → 영원히 못 빠져나옴
```

**그런데 volatile은 메모리 재정렬을 못 막음.**
```
hart 0이 쓴 순서: ① 페이지 테이블 만들기  ② started = 1
CPU/캐시가 이 두 쓰기를 다른 코어에게 다른 순서로 보여줄 수 있음
→ hart 1이 started=1만 먼저 보고 반쯤 만들어진 페이지 테이블로 페이징을 켬 → 죽음
```
**단일 코어에선 안 보임** — 하드웨어가 "내 관점에선 순서대로"라는 환상을 유지. **다른 코어는 그 환상 밖.**

**RELEASE/ACQUIRE가 울타리를 만듦:**
```
hart 0: [페이지 테이블 쓰기들] ─┐
                                │ RELEASE (이 아래로 못 넘어감)
        started = 1 ────────────┘
hart 1: started == 1 확인 ─┐
                           │ ACQUIRE (이 위로 못 넘어감)
        [페이지 테이블 읽기] ┘  ← 반드시 완성된 것을 봄 ✅
```

| | 막는 것 | 못 막는 것 |
|--|--|--|
| **`volatile`** | 컴파일러의 생략·캐싱 | CPU 재정렬, 원자성 |
| **RELEASE/ACQUIRE** | 재정렬 (컴파일러+CPU) | — |

> **Ch26의 결론이 정밀해짐:** 가시성(volatile) / 원자성(락) 에 **순서(배리어)**가 추가.
> **멀티코어에서는 순서가 원자성만큼 중요하다.**

---

## `kalloc.c` — 물리 페이지 할당기 (malloc lab과 대조)

```c
struct run { struct run *next; };
struct { struct spinlock lock; struct run *freelist; } kmem;
```

| | malloc lab | kalloc |
|--|--|--|
| 단위 | 임의 크기 | **4096 고정** |
| 헤더/푸터 | 필요 | **불필요** (크기가 늘 같음) |
| splitting/coalescing | 필요 | **불필요** |
| find_fit, 배치 정책 | 필요 | **불필요** (아무거나 하나) |
| 링크 | prev+next (이중) | **next만** (단일) |
| 이유 | 병합 때문에 임의 위치 제거 | 항상 head에서만 넣고 뺌 → O(1) |
| 락 | 없음 (단일 스레드) | **spinlock** (코어 3개가 동시 호출) |

**Ch18의 결론이 코드로:** *"균일 크기로 자르면 외부 단편화가 원천 소멸."*
크기가 다 같으니 **"맞는 걸 찾는다"는 문제 자체가 사라짐.** 대가는 내부 단편화.

### `memset`으로 뭉개는 이유 — fail fast

```c
kalloc: memset(r, 5, PGSIZE);     // 할당 시 0x05로 → 초기화 안 하고 읽으면 티 남
kfree:  memset(pa, 1, PGSIZE);    // 해제 시 0x01로 → 해제 후 읽으면 티 남
```
**헤더 비트를 세팅하는 게 아니라 내용물을 뭉개는 것.** (kalloc엔 헤더가 없음)

**Ch14의 use-after-free / uninitialized read를 드러내는 장치.**
남은 데이터가 그럴듯하면 조용히 동작하다 한참 뒤 터짐 → **명백히 이상한 값으로 즉시 드러냄.**
`0x01`을 고른 이유: 포인터로 쓰면 즉시 page fault, 정수로 쓰면 눈에 띄는 값.
(glibc 디버그는 0x5a, MSVC는 0xDD/0xCD를 씀)

**커널은 valgrind를 못 쓰니**(그건 커널 위에서 도는 도구) **수동 방어가 필요.**

### `kinit` / `freerange`

```c
void kinit() { initlock(&kmem.lock, "kmem"); freerange(end, (void*)PHYSTOP); }
```
**초기화 방식이 영리함 — 처음부터 리스트를 만드는 게 아니라 전부 `kfree`해서 채움.**

```
end      링커가 계산해준 "커널 코드·데이터가 끝나는 주소"
         kernel.ld의 PROVIDE(end = .). C에선 extern char end[]로 받음
         (extern이라 컴파일은 통과, 링크 시 실제 주소가 채워짐 — Ch14의 컴파일 vs 링크)
PHYSTOP  KERNBASE(0x80000000) + 128MB.  Makefile의 -m 128M
```

```
0x00000000 ┌──────────┐
           │ 장치들    │  UART, PLIC, virtio disk
0x80000000 ├──────────┤  ← KERNBASE. 여기부터 RAM
           │ 커널 코드 │
end ──────→│ 자유 메모리│  ← kalloc이 관리
0x88000000 └──────────┘  ← PHYSTOP
```

**`PHYSTOP`은 물리 주소.** 부팅 직후엔 페이징이 꺼져 있고, 페이징을 켠 뒤에도 **커널 영역은 항등 매핑(가상=물리)**이라 헷갈릴 여지가 있음.

> **Ch20에서 얘기한 문제의 xv6식 해법:** 리눅스는 `__va()`/`__pa()`로 상수 오프셋 변환, **xv6는 오프셋을 0으로 만들어 변환 자체를 없앰.**
> 그래서 `kalloc()`이 반환하는 주소는 **물리 주소이면서 동시에 커널이 쓸 수 있는 가상 주소.**

---

## `spinlock.c` — Ch28의 실물

```c
void acquire(struct spinlock *lk) {
  push_off();                                          // ①
  if(holding(lk)) panic("acquire");                    // ②
  while(__sync_lock_test_and_set(&lk->locked, 1) != 0) // ③ Ch28의 TestAndSet
    ;
  __sync_synchronize();                                // ④
  lk->cpu = mycpu();
}
```

**Ch28에서 만든 건 ③뿐. 나머지는 실전에서 필요한 방어.**

### ① `push_off()` — 인터럽트 끄기

**목적: 인터럽트 핸들러가 같은 락을 재진입하는 걸 막기.**
```
CPU 0: acquire(&kmem.lock) 성공 → 작업 중
       ↓ 인터럽트 발생 (타이머든 디스크든)
       핸들러가 kalloc() 호출 → acquire(&kmem.lock) → 스핀
       ↓ 놓아줄 코드는 핸들러 아래 멈춰 있음 → 데드락
```
**Ch32 4조건 중 순환 대기, 사이클 길이 1.** (main-deadlock은 2, 철학자는 5)

**멀티코어에선 무해:** 다른 CPU는 인터럽트가 켜진 채 멀쩡히 돎. **"자기 CPU의 인터럽트만" 끄는 게 핵심.**
> Ch28에서 *"인터럽트 끄기는 멀티코어에서 무용지물"*이라 한 건 **락의 대체재로는** 그렇다는 뜻.
> 여기선 **락과 함께, 다른 목적(재진입 방지)**으로 씀. **같은 도구를 다른 문제에.**

**`push`/`pop`인 이유 — 중첩:**
```c
acquire(&A);   // 깊이 1
  acquire(&B); // 깊이 2
  release(&B); // 깊이 1 — 아직 켜면 안 됨!
release(&A);   // 깊이 0 — 이제 켬 ✅
```
**"원래 켜져 있었는지"도 기억** — 꺼진 채로 들어왔으면 나갈 때도 꺼진 채로.

### 부작용 — 스핀락을 쥔 채 잠들면 안 되는 이유

```
push_off()가 스핀 루프 "앞"에 있음
→ 스핀 중인 CPU도 선점당하지 않음
→ 단일 코어에서 락 소유자로 돌아갈 방법이 없음 → 데드락
```

> **커널 스핀락의 철칙: 스핀락을 쥔 채 절대 양보(yield)하거나 잠들지 말 것.**
> 그래서 오래 기다려야 하는 경우(디스크 I/O)를 위한 **`sleeplock`**이 따로 있음.
> **Ch28의 "짧으면 스핀, 길면 블록"이 두 자료구조로 나뉜 것.**
> `sleeplock` 내부에 `spinlock`이 들어있음 = Ch28의 "guard로 짧은 구간만 스핀" 구조.

**사용자 공간 스레드였다면** 선점이 되니 "기다리면 언젠가 풀림"이 성립. **`push_off` 때문에 그 가정이 깨지는 것.**

### ② `holding()` → panic

```c
static int holding(struct spinlock *lk) { return (lk->locked && lk->cpu == mycpu()); }
```
**같은 CPU가 이미 쥔 락을 또 잡으려 하면 즉시 죽임.** 기다리게 두면 **자기 자신을 기다려** 영원히 못 빠져나옴.

**어차피 멈추는데, 조용히 멈추느냐 시끄럽게 죽느냐의 차이.**
```
스핀 방치 : 커널이 그냥 멈춤. 로그도 없음 → 원인 파악 불가
panic     : 메시지 + 어느 락 + 백트레이스 → 즉시 드러남 ✅
```
**`release`에도 같은 검사** — 남의 락을 놓으려는 것도 잡음.
Ch28의 *"락은 규약이라 컴파일러가 못 잡는다"* → **런타임 검사로 최소한 방어.**

**재귀 락을 일부러 안 쓰는 이유:** 허용하면 "이 함수가 락을 잡고 있나?"를 신경 안 쓰게 되고, 임계 영역이 길어지고 불명확해짐. **"재귀 락이 필요하면 설계가 잘못된 것"**이 커널 쪽 통념.

### ④ `__sync_synchronize()` — 메모리 배리어

**"값이 보이게"가 아니라 "순서가 지켜지게".**
```c
acquire:  test_and_set(...)      // 락 획득
          __sync_synchronize();  // ─── 울타리 ───
          [임계 영역]             // 위로 못 올라감 ✅
release:  [임계 영역]             // 아래로 못 내려감 ✅
          __sync_synchronize();  // ─── 울타리 ───
          lk->locked = 0
```
**임계 영역이 락 밖으로 새는 걸 막음.** 새면 다른 코어가 "락도 안 잡았는데 임계 영역 실행"으로 보게 됨.

**가시성은 캐시 일관성(Ch10)이 이미 보장.** 문제는 **순서.**
```
가시성  : 언젠가 보인다        → 캐시 일관성이 보장
순서    : 어떤 순서로 보인다    → 배리어가 필요 ★
```
`start.c`의 RELEASE/ACQUIRE는 특정 변수용, `__sync_synchronize()`는 전체 울타리(더 강하고 비쌈).

---

## 락 목록 = 커널의 공유 자원 지도

```bash
grep -rn "initlock" kernel/*.c
```
```
kmem.lock        물리 페이지 free list       ← Ch17
pid_lock         PID 카운터
wait_lock        부모-자식 wait 관계
p->lock          ★ 프로세스마다 하나         ← Ch29 세분화
tickslock        틱 카운터                   ← Ch6
bcache.lock      디스크 버퍼 캐시            ← Ch21 "메모리=디스크의 캐시"
log.lock         파일시스템 저널             ← Ch42
itable/ftable    inode / 파일 디스크립터
pi->lock         ★ 파이프마다 하나           ← Ch33 생산자-소비자
cons/pr.lock     콘솔·커널 출력
```

**"CPU 혼자 쓰는 락"은 개념적으로 없음** — 혼자 쓰면 락이 필요 없음.
```
독립적으로 나눌 수 있는 데이터 → 락 불필요 (struct cpu, mycpu())
공유해야 하는 데이터          → 락 필요
```
**`p->lock`, `pi->lock`이 객체마다 있는 이유:** 서로 다른 프로세스/파이프를 다루는 코어끼리는 경쟁할 이유가 없음. 전역 락 하나였다면 **Ch10의 SQMS 문제**가 그대로.
→ **Ch29의 "독립적으로 나눌 수 있는 축을 찾아라"**가 적용된 것.

---

## `vm.c: walk()` — Ch20의 멀티레벨 walk 실물

```c
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc) {
  for (int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if (*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);     // 하위 테이블로 내려감
    } else {
      if (!alloc || (pagetable = (pde_t*)kalloc()) == 0) return 0;
      memset(pagetable, 0, PGSIZE);              // 새 테이블은 전부 invalid
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];                  // 최종 PTE
}
```

### 3단계 = Sv39

```
[ L2 9비트 ][ L1 9비트 ][ L0 9비트 ][ offset 12비트 ]     총 39비트
└──────────── VPN 27비트 ─────────────┘
```
```c
#define PX(level, va)  ((((uint64)(va)) >> PXSHIFT(level)) & 0x1FF)
```
**Ch20에서 손으로 한 `PDI = VPN/1024, PTI = VPN%1024`와 같은 계산.** 시프트+마스크가 더 빠를 뿐.

**왜 9비트:** 페이지 4096B ÷ PTE 8B = **512 = 2⁹**. Ch20의 *"조각 크기 = 페이지 크기가 인덱스 비트 수를 정한다."*
**왜 3단계:** 27 = 9×3. 39비트면 512GB 주소 공간.

### `alloc` 인자

```c
walk(pt, va, 0)   // 조회만. 없으면 0
walk(pt, va, 1)   // 없으면 중간 테이블을 새로 만들어라
```
**Ch20의 *"안 쓰는 구간은 테이블 자체를 안 만든다"*** → 처음 매핑할 땐 만들어야 함.
**`kalloc()`이 여기 쓰임** — 페이지 테이블 한 장이 정확히 4096바이트. Ch20의 *"조각을 페이지 크기로 맞추면 프레임 하나에 딱"*이 코드로.

**0을 반환하는 경우는 "미할당"이 아니라 `kalloc()` 실패**(물리 메모리 부족).

### PTE의 비트 겸용

```c
#define PA2PTE(pa)  ((((uint64)pa) >> 12) << 10)
#define PTE2PA(pte) (((pte) >> 10) << 12)
```
**하위 10비트가 플래그(V, R, W, X, U, A, D...)**라 주소를 10비트 밀어 넣음.
**Ch18의 "안 쓰는 비트에 플래그 겸용" = malloc lab의 `size | alloc`과 같은 발상.**

### 왜 물리 주소를 그대로 역참조할 수 있나

**커널의 항등 매핑 때문.** 가상 = 물리라 번역 결과가 같음.
**Ch20의 "walk의 모든 포인터가 물리 주소라 재귀가 없다"**가 여기서 성립.

---

## `vm.c: mappages()`

```c
int mappages(pagetable_t pt, uint64 va, uint64 size, uint64 pa, int perm) {
  if ((va % PGSIZE) != 0)   panic("mappages: va not aligned");
  if ((size % PGSIZE) != 0) panic("mappages: size not aligned");
  if (size == 0)            panic("mappages: size");

  a = va;  last = va + size - PGSIZE;
  for (;;) {
    if ((pte = walk(pt, a, 1)) == 0) return -1;
    if (*pte & PTE_V) panic("mappages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if (a == last) break;
    a += PGSIZE;  pa += PGSIZE;
  }
  return 0;
}
```

**va부터 size바이트를 pa부터의 물리 메모리에 매핑.**

### `last`가 `va + size - PGSIZE`인 이유

**`last` = 마지막 페이지의 시작 주소.**
```
size = 4096: last = va          → 첫 바퀴에 a == last → 한 장만 매핑
size = 8192: last = va + 4096   → 두 바퀴
```
**`a < va + size`로 안 쓰는 이유: 주소 공간 맨 끝을 매핑할 때 `va + size`가 오버플로**해서 0이 되면 루프가 안 돎.
→ **경계값을 다룰 땐 "끝 다음"이 아니라 "마지막"을 기준으로.**

### `panic("remap")` — 왜 덮어쓰면 안 되나

```
va 0x1000 → 물리 A (이미 매핑, 누가 쓰는 중)
        ↓ 덮어씀
va 0x1000 → 물리 B
① A를 가리키던 유일한 참조가 사라짐 → 영원히 회수 불가 (누수)
② A를 쓰던 코드가 갑자기 B의 내용을 봄 → 데이터 손상
```
**둘 다 조용히 일어나 한참 뒤 터짐** → fail fast로 즉시 죽임.

### ★ `panic` vs `return -1` 구분

```c
if ((va % PGSIZE) != 0) panic(...);            // 코드가 잘못됨. 일어나면 안 되는 일
if ((pte = walk(...)) == 0) return -1;         // 메모리 부족. 있을 수 있는 정상 상황
if (*pte & PTE_V) panic("remap");              // 불변식 위반
```
**Ch14의 *"assert는 에러 처리용이 아니다"*가 그대로.** (reverse 프로젝트에서 정리한 것)

---

## 자가 점검 (다음에 볼 때)

각각 "설명할 수 있다 / 들어봤다 / 모르겠다"로:
1. hart가 뭐고 hart 0만 다른 일을 하는 이유
2. `entry.S`가 스택을 세팅하는 이유 — 왜 C로 못 하나
3. M mode → S mode로 내려가는 방법과 `mret`이 트랩과 어떻게 다른가
4. `push_off()` — 락을 쥘 때 인터럽트를 끄는 이유, 그 부작용
5. `volatile`만으로 부족한 이유 (재정렬)
6. `walk()`가 va를 세 조각으로 쪼개는 이유
7. `panic`과 `return -1`을 나누는 기준

---

## 다음

- `sleeplock.c` (짧음)
- `vm.c`의 `kvmmake` — 커널 페이지 테이블을 실제로 만드는 곳. Ch16 보호 비트
- `vm.c`의 `copyin`/`copyout` — **Ch15 신뢰 경계**의 실물 ★

xv6 계획: [xv6-학습-로드맵.md](./README.md)
전체 로드맵: [ROADMAP.md](../ROADMAP.md)
