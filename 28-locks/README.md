# OSTEP Ch 28 — Locks 정리

Ch26에서 남긴 **부트스트랩 문제**의 답: 하드웨어 원자 명령으로 락을 만든다.
핵심: **원자성이 필요한 건 "읽고 쓰는 그 순간"뿐이고, 그 위에 소프트웨어가 나머지를 쌓는다.**

---

## 출발점 — 깨진 락

```c
void lock(mutex *m) {
    while (m->flag == 1) ;   // ① 확인
    m->flag = 1;             // ② 설정
}
```
①과 ② 사이의 틈에 두 스레드가 모두 통과 가능. **`m->flag`도 결국 공유 변수.**

> **락을 만들려면 이미 원자성이 필요하다** = 부트스트랩 문제.
> Phase 2의 그 원리 재등장: **메커니즘은 자기 자신에 의존할 수 없다.**
> (page walk가 전부 물리 주소였듯, 락 구현은 락 없이 동작해야 함.)

---

## 락의 평가 기준 3가지

| 기준 | 뜻 |
|--|--|
| **상호 배제 (correctness)** | 한 번에 하나만 임계 영역에 |
| **공정성 (fairness)** | 기다리는 스레드가 언젠가 반드시 들어가나 (굶주림 없나) |
| **성능 (performance)** | CPU 낭비, 경합 시 오버헤드 |

"동작한다"만으로는 부족 — 뒤의 둘이 실제 설계를 가른다.

---

## 시도 1: 인터럽트 끄기

```c
void lock()   { DisableInterrupts(); }
void unlock() { EnableInterrupts(); }
```
문맥 교환을 원천 차단 → 임계 영역이 안 쪼개짐. **그런데 4가지 문제:**

1. **특권 명령** — 커널 모드에서만. 사용자에게 허용하면 인터럽트 끄고 **CPU 영구 독점** 가능 → Ch6의 "OS가 CPU를 되찾는 법"이 무너짐. (사용자를 신뢰할 수 없다는 원칙)
2. **멀티코어에서 무용** — 내 코어만 조용해질 뿐 다른 코어의 스레드는 그대로 실행. ★ 결정타
3. **오래 끄면 시스템 마비** — 디스크 완료·네트워크·타이머 인터럽트가 유실/지연
4. 인터럽트 마스킹 자체가 느린 명령

→ **커널 내부의 아주 짧은 구간에서만 사용.** 사용자 락으로는 불가.

---

## 하드웨어 원자 명령

### TestAndSet (x86: `xchg`)

```c
int TestAndSet(int *ptr, int new) {
    int old = *ptr;    // 읽고
    *ptr = new;        // 쓰고
    return old;        // 옛 값 반환
}   // ★ 이 셋이 하드웨어에서 원자적으로
```

**핵심: "확인하면서 동시에 차지한다".** `TestAndSet(&flag, 1)`은 **항상 1을 쓰고**, "내가 쓰기 직전엔 뭐였는지"를 알려줌.
- 옛 값 **0** → 락이 비어 있었고 **내가 차지했다** ✅
- 옛 값 **1** → 이미 남이 갖고 있다 → 대기

```
flag = 0
A: TestAndSet(&flag,1) → old=0 읽고 flag=1 쓰고 0 반환 → A 획득
B: TestAndSet(&flag,1) → old=1 읽고 flag=1 쓰고 1 반환 → 대기
```
**0을 받는 스레드는 유일함이 보장돼** — 읽기와 쓰기 사이에 틈이 없으니까.
("남이 덮어썼는지 어떻게 아느냐"는 물을 필요가 없음. 원자성이 그걸 보장.)

### 스핀락

```c
void lock(lock_t *l) {
    while (TestAndSet(&l->flag, 1) == 1) ;   // 1을 받으면 재시도
}
void unlock(lock_t *l) { l->flag = 0; }
```
- **`unlock`은 원자 명령 불필요** — 락 소유자는 나뿐이라 경쟁이 없음. 단순 쓰기로 충분.
- 실패해도 계속 1을 쓰는데, 이미 1이라 무해.

**평가:** 상호 배제 ✅ / 공정성 ❌ / 성능 ❌

### Compare-And-Swap (CAS) — 가장 강력

```c
int CompareAndSwap(int *ptr, int expected, int new) {
    int actual = *ptr;
    if (actual == expected) *ptr = new;   // 예상대로일 때만 쓴다
    return actual;
}
```
락 만들기는 TestAndSet과 동일: `while (CAS(&l->flag, 0, 1) == 1) ;`

**★ 왜 더 강력한가** (루프 횟수 차이가 아님):
```
TestAndSet : 무조건 정해진 값을 씀. 알려주는 건 "옛날 값"뿐
CAS        : 조건이 맞을 때만 씀 → "내가 읽은 이후 아무도 안 건드렸다"를 검증 가능
```

**lock-free 리스트 삽입:**
```c
void push(node_t *n) {
    node_t *old;
    do {
        old = head;
        n->next = old;
    } while (CompareAndSwap(&head, old, n) != old);
    //  "head가 아직 old면 n으로 바꿔라"
}
```
읽은 뒤 누가 head를 바꿨으면 CAS가 **실패하고 그 사실을 알려줌** → 재시도.
TestAndSet으로는 불가능(확인할 방법이 없음).

**lock-free 프로그래밍:** 락 없이 CAS 재시도만으로 자료구조를 안전하게. 데드락 없음, 경합 없음. 대신 재시도가 많으면 느리고 설계가 훨씬 어려움.
(x86 `cmpxchg`, ARM `LDXR`/`STXR` 쌍. C11 `atomic_compare_exchange_weak`)

### FetchAndAdd → 티켓락 (공정성)

```c
int FetchAndAdd(int *ptr) { int old = *ptr; *ptr = old+1; return old; }
```

```c
typedef struct { int ticket; int turn; } lock_t;   // ★ 변수 둘이 합쳐 락 하나

void lock(lock_t *l) {
    int myturn = FetchAndAdd(&l->ticket);   // 번호표 뽑기
    while (l->turn != myturn) ;             // 내 번호 호출될 때까지
}
void unlock(lock_t *l) { l->turn = l->turn + 1; }   // 다음 번호 호출
```
**은행 번호표.** 뽑은 순서대로 정확히 진입 → **굶주림이 원리적으로 불가능.**
- 번호표 뽑기도 read-modify-write라 **원자적이어야** 함(중복 방지)
- `unlock`은 소유자만 하니 원자성 불필요
- **여전히 스핀함** — 공정성만 해결

---

## 성능 문제 — 스핀을 어떻게 없애나

**단일 코어에서 락 소유자가 문맥 교환되면:**
```
기다리는 스레드가 타임 슬라이스 전체(1~4ms)를 while만 돌며 낭비
→ 기다리는 스레드 N개면 슬라이스 N-1개가 통째로 증발
```
**멀티코어에선 나음** — 소유자가 다른 코어에서 계속 도니 스핀 시간이 임계 영역 길이만큼.

```
임계 영역 짧음 + 멀티코어  → 스핀이 유리 (문맥 교환 1.2μs보다 잠깐 도는 게 쌈)
임계 영역 김 / 단일 코어   → 스핀은 재앙. 잠들어야(block)
```

### yield

```c
while (TestAndSet(&l->flag, 1) == 1) yield();   // Running → Ready 자진 전환
```
```
spin  : 슬라이스 끝까지 태우고 → 타이머 인터럽트 → 문맥 교환 (강제)
yield : 즉시 시스템콜 → 문맥 교환 (자발적)
```
**이득:** 남은 슬라이스를 안 버림. 소유자가 빨리 CPU를 받음.
**남는 문제:**
1. **문맥 교환 비용이 계속 나감** (시스템콜 172ns, 문맥 교환 1.2μs — Ch6 실측). 100개가 기다리면 소유자 한 번 돌 때까지 99번 교환
2. **공정성 여전히 없음** — 그냥 "다음 놈"이라 굶주림 가능
3. **근본 문제: 락이 안 풀렸는데도 계속 깨어나서 확인** → 의미 없는 왕복

### 큐 + park/unpark ★

```c
typedef struct { int flag; int guard; queue_t *q; } lock_t;

void lock(lock_t *m) {
    while (TestAndSet(&m->guard, 1) == 1) ;   // guard로 짧게 보호
    if (m->flag == 0) { m->flag = 1; m->guard = 0; }       // 획득
    else {
        queue_add(m->q, gettid());
        m->guard = 0;
        park();                                // ★ 나를 재워라 (Blocked)
    }
}
void unlock(lock_t *m) {
    while (TestAndSet(&m->guard, 1) == 1) ;
    if (queue_empty(m->q)) m->flag = 0;
    else unpark(queue_remove(m->q));           // ★ 큐의 다음 놈을 깨움
    m->guard = 0;
}
```
- 대기 스레드가 **Blocked 상태**로 완전히 잠듦 → CPU 0, 스케줄러가 쳐다도 안 봄
- 락이 풀리면 **큐에서 하나만** 깨움 → 불필요한 경쟁 없음
- 큐 순서 → **공정성** ✅

**★ 주목: 두 층의 락**
```
guard : TestAndSet 스핀락. 큐 조작하는 아주 짧은 구간만
flag  : 실제 락. 길게 잡힐 수 있음
```
→ **스핀을 없앤 게 아니라 최소 구간으로 몰아넣은 것.**

### wakeup/waiting race (lost wakeup)

```c
    queue_add(m->q, gettid());
    m->guard = 0;
    // ★ 여기서 문맥 교환되면?
    park();
```
```
A: queue_add(A), guard=0
[문맥 교환 — A는 아직 park() 전]
B: unlock() → 큐가 안 비었으니 unpark(A)
   → 그런데 A가 아직 안 자고 있음! ★ 깨우기가 허공에 날아감
[A 복귀]
A: park() → 이제야 잠듦 → 깨워줄 사람이 없음 → 영원히 잠
```
**큐에서 사라지는 게 아니라 "깨우기 신호가 유실"되는 것.** `park()`는 이미 온 깨우기를 기억 못 함.

**해법: `setpark()`** — "나 곧 잘 거야"를 미리 선언
```c
    queue_add(...);
    setpark();        // ★ 잠들 의도 등록
    m->guard = 0;
    park();           // 사이에 unpark이 왔으면 즉시 반환(안 잠듦)
```

**핵심 패턴: "잠들 의도"를 미리 등록해 그 사이 온 신호를 놓치지 않게 한다.** 계속 재등장:
- **Ch30 조건 변수**: `pthread_cond_wait`이 뮤텍스를 **원자적으로 놓으면서 잠드는** 이유가 정확히 이것
- **futex(리눅스 실제 구현)**: `futex_wait(addr, expected)` = "이 주소가 아직 expected면 자라"를 원자적으로 → 이미 바뀌었으면 안 잠듦

### 실제 리눅스 — two-phase lock

**먼저 잠깐 스핀**(짧은 임계 영역이면 여기서 끝) → 안 되면 **futex로 잠듦.** 두 방식의 좋은 점만.

---

## 락은 코드가 아니라 데이터를 보호한다

**`mutex_t`는 코드도 데이터도 안 가리킴.** 그냥 메모리의 상태값(잠김/소유자/대기큐).
**"락 범위" = lock()과 unlock() 사이 = 개발자가 정하는 것.** 잘못 잡아도 컴파일러는 침묵.

**설계할 땐 데이터 중심으로:**
```c
pthread_mutex_t counter_lock;   // protects: counter, total
```
"이 락이 어느 데이터를 지키는가"를 먼저 정하고, **그 데이터를 건드리는 모든 코드**에서 그 락을 잡을 것.
```c
lock(&m); counter++; unlock(&m);   // 함수 A
counter++;                          // 함수 B — 락 없이 → 전체가 무너짐
```

**세분도(granularity):**
```
coarse-grained : 큰 락 하나로 전부 → 단순·안전, 병렬성 죽음 (main-deadlock-global.c의 g)
fine-grained   : 데이터마다 따로  → 병렬성 좋음, 복잡·데드락 위험↑
```
그 외: **임계 영역은 짧게**(락 잡고 I/O 금지), **락 잡은 채 다른 락 잡으면 순서 통일**.

**Rust의 `Mutex<T>`**는 데이터를 락 안에 넣어 "락 없이 접근"을 **타입 시스템상 불가능**하게 만듦. C는 개발자 규율에만 의존.

---

## 숙제 (x86.py) — `threads-locks`

Ch26과 같은 시뮬레이터. `xchg`, `fetchadd`가 추가됨.

### flag.s — 깨진 락

```asm
mov  flag, %ax      # 읽기
test $0, %ax        # 확인
jne  .acquire       # 판단
mov  $1, flag       # 설정
```
```bash
./x86.py -p flag.s -t 2 -i 2 -a bx=1 -M flag,count -c    # → count 1 (기대 2)
```

**깨지는 순간:**
```
T1: mov flag,%ax    → %ax = 0 (이때는 진짜 0)
T1: test $0,%ax     → 조건 코드 세팅
[문맥 교환 — %ax와 조건 코드가 T1의 TCB에 저장]
   T0가 flag를 1로 바꿈
[문맥 교환 — T1의 %ax=0, 조건 코드 그대로 복원]
T1: jne .acquire    → 복원된 낡은 조건 코드로 판단 → 통과 → 임계 영역 진입
```
**메모리는 1인데 T1은 자기 레지스터에 박제된 0을 보고 결정.**
Ch27 race.c에서 A가 낡은 카운터로 덮어쓴 것과 같은 구조 — **레지스터는 스레드마다 따로.**
그리고 틈이 **두 개**임(mov→test, test→jne→mov).

### test-and-set.s — 고쳐진 락

```asm
mov  $1, %ax
xchg %ax, mutex     # 원자적 교환
test $0, %ax
jne  .acquire
```

**★ 틈이 어디로 갔나:**
```
flag.s:          mov ─틈─ test ─틈─ jne ─틈─ mov $1
                 └──────── 전체가 위험 ────────┘
test-and-set.s:  xchg(원자적) ─틈─ test ─틈─ jne
                 └─ 안전 ─┘      └─ 이 틈은 무해 ─┘
```
**`xchg` 뒤에도 틈은 있지만 무해** — 이미 승부가 났으니까. 0을 받았으면 나는 이미 주인, 1이면 이미 실패. 시간이 흘러도 결과가 안 바뀜.
> **원자성이 필요한 건 "읽고 쓰는 그 순간"뿐, 그 뒤 판단은 느긋해도 된다.**

### 실측 — 인터럽트 간격을 바꿔가며 (bx=3, 기대값 6)

```
test-and-set:  i=1~7 전부 6      ✅ 간격과 무관하게 항상 정확
flag:          3, 3, 4, 5, 6, 5, 4  ❌ 제각각
```
- **flag의 i=5가 6이 나온 건 우연** — 통과했다고 안전한 게 아님 (Ch26의 그 교훈)
- **모든 간격에서 정확한 게 올바름의 증거** — race는 특정 타이밍에서만 터지므로

### ticket.s — 공정성

```asm
mov $1, %ax
fetchadd %ax, ticket   # ticket += 1, 옛 값이 내 번호
.tryagain
mov turn, %cx
test %cx, %ax
jne .tryagain
... 임계 영역 ...
mov $1, %ax
fetchadd %ax, turn     # 다음 번호 호출
```
**`ticket`과 `turn` 두 변수가 합쳐서 락 하나** (별도 mutex 변수 없음).

실측(스레드 3개): ticket이 1,2,3으로 발급 → **T0(0번) → T1(1번) → T2(2번) 순서로 진입.**
**번호를 뽑는 순간 순서가 확정** — 그 뒤 스케줄링과 무관.
반면 스핀락은 락이 풀린 뒤 **먼저 xchg를 때린 놈이 이김** → 순서 무보장.

### ★ 스핀 중에 뭘 하는지가 다르다 (성능)

```asm
; 스핀락 대기 루프
xchg %ax, mutex     ← 실패해도 매번 mutex에 "쓴다"
; 티켓락 대기 루프
mov turn, %cx       ← "읽기만" 한다
```
**Ch10 캐시 일관성 연결:**
```
쓰기 → 다른 코어의 캐시 라인 무효화 → 캐시 라인 핑퐁
읽기 → 여러 코어가 공유 상태로 캐시 → 조용함
```
대기 코어가 많을수록 **스핀락은 버스를 마비시킴.**
→ 실제 스핀락은 **test-and-test-and-set**: 먼저 읽어보고(일반 load) 자유로울 때만 `xchg`를 때림.

---

## 실전 포인트

- **부트스트랩 문제**: 락을 만들려면 원자성이 필요 → 하드웨어가 바닥을 깖. (Phase 2의 "메커니즘은 자기 자신에 의존 못 함"과 동일 구조)
- **평가 3기준**(상호 배제/공정성/성능)으로 각 방식을 비교할 수 있어야.
- **TestAndSet vs CAS 차이** — 후자는 "안 바뀌었는지 검증"이 가능 → lock-free. 면접에서 CAS 물어보면 이거.
- **원자성 범위는 최소로** — xchg 뒤의 틈이 왜 무해한지 설명.
- **티켓락이 공정한 이유**, 그리고 **대기 중 읽기만 해서 캐시에도 유리**.
- **park/unpark의 lost wakeup**과 setpark/futex — Ch30 조건 변수의 복선.
- **스핀 vs 블록 선택 기준**: 임계 영역 길이 + 코어 수.
- **락은 규약** — 데이터 중심 설계, 세분도 트레이드오프.

---

## 다음
- **Ch 29 Lock-based Data Structures** — 실제 자료구조에 락 걸기. 세분도와 확장성.
- **Ch 30 Condition Variables** — "조건이 만족될 때까지 대기". 이 장의 lost wakeup 문제가 정식으로.
- 전체 로드맵: [ROADMAP.md](../ROADMAP.md)
