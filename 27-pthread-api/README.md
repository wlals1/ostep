# OSTEP Ch 27 — Interlude: Thread API 정리

pthread API 사용법 + 흔한 함정. **API는 읽어서가 아니라 써서 익힌다.**
실전에 쓰는 건 열 개 남짓 — 나머지는 `man`으로 찾아 쓰면 됨.

---

## pthread = POSIX thread

`p`는 **POSIX**. 1990년대에 OS마다 스레드 API가 제각각(솔라리스/Win32/DCE)이라 이식이 안 돼서 POSIX.1c(1995)로 통일.
같은 규칙: `sem_*`(세마포어, Ch31), `shm_*`(공유 메모리).
윈도우는 POSIX를 안 따라 `CreateThread` 등 별도 API → 크로스 플랫폼은 C11 `<threads.h>`나 C++ `std::thread`.

> 책의 `Pthread_create`(대문자 P)는 표준이 아니라 **저자의 래퍼** — 반환값을 assert로 검사해줌. "시스템 함수 반환값 확인" 습관을 강제하는 장치.

---

## pthread_create

```c
int pthread_create(pthread_t *thread,
                   const pthread_attr_t *attr,
                   void *(*start_routine)(void *),
                   void *arg);
```

### 함수 포인터 읽는 법 ★ (헷갈렸던 것)

**안에서 밖으로, 이름 옆에 붙은 것부터.**
```c
void *(*start_routine)(void *)
```
1. `start_routine` — 이름
2. `(*start_routine)` — **포인터**다
3. `(*start_routine)(void *)` — **`void*`를 받는 함수**를 가리킨다
4. `void *(*...)` — 그 함수는 **`void*`를 반환**한다

| 부분 | 정체 |
|--|--|
| 맨 앞 `void *` | **반환 타입** |
| `start_routine` | **인자 이름** (`int a`의 `a`처럼) |
| 뒤 `(void *)` | 함수가 받는 **인자 타입** |

**괄호가 필수인 이유:**
```c
int *fp(char);    // 함수. int*를 반환
int (*fp)(char);  // 포인터. int를 반환하는 함수를 가리킴  ← 전혀 다름
```
복잡하면 **typedef로 치워버림**(실무 관례):
```c
typedef void *(*thread_func_t)(void *);
```

### 왜 `void*`인가

**라이브러리를 만드는 쪽은 사용자가 뭘 넘길지 모름.** int? 구조체? 파일 핸들?
→ `void*` = "아무 타입의 포인터든 담는 자리". **타입 정보를 지워서 범용성을 얻음.**
`qsort`가 `void*`를 받는 것과 정확히 같은 이유(ssort에서 한 것).
**대가:** 타입 안전성 포기. 컴파일러가 검사 못 함 → 런타임에 터짐. (C++ 템플릿·Rust 제네릭이 이걸 타입 안전하게 푼 것.)

### 인자 전달 세 방법

**A. 값을 void*로 캐스팅** (역참조 안 함)
```c
pthread_create(&p, NULL, worker, (void *)(long)n);
// worker
long count = (long)arg;        // * 없음!
```
**B. 주소 전달** (역참조 함)
```c
long n = ...;
pthread_create(&p, NULL, worker, &n);
// worker
long count = *(long *)arg;
```
**C. 전역 변수** — 스레드 만들기 전에 세팅하고 이후 아무도 안 고치면 **읽기 전용 공유**라 안전. 제일 간단.

→ **인자 메커니즘이 존재하는 이유는 스레드마다 다른 값을 줘야 할 때**(ID, 담당 구간, 결과 저장 위치). 여러 값이면 **구조체 포인터**.

### ★ 캐스팅 `(void *)(long)n`이 2단계인 이유 (많이 헷갈림)

```
int   : 4바이트
long  : 8바이트
void* : 8바이트   ← long과 같은 크기
```
- `(void *)atoi(...)` → int(4B)를 8B 자리에 → **"cast to pointer from integer of different size" 경고**
- `(void *)(long)n` → ① int→long(폭 맞춤) ② long→void*(같은 크기, 경고 없음)

**캐스팅은 컴파일 타임 지시야.** "이 비트들을 이 타입으로 취급하라"는 말이고, 정수↔포인터는 **런타임에 아무 일도 안 함**(재해석만). 비트 패턴은 그대로.
(`int`→`double`은 표현 방식이 달라 진짜 변환 명령이 나감.)

**A방식 = 포인터 자리를 "8바이트 값 운반칸"으로 쓰는 것.** 그래서 worker에서 `*`를 붙이면 **없는 주소를 역참조 → segfault.**
엄밀히는 구현 정의지만 리눅스/맥/윈도우에서 관용적으로 씀. 정석은 B나 구조체.

### ★ 포인터 넘길 때의 수명 함정

```c
for (int i = 0; i < 4; i++)
    pthread_create(&t[i], NULL, worker, &i);   // ❌ 전형적 버그
```
모든 스레드가 **같은 `i`의 주소**를 받음 → 읽을 때쯤 `i`는 이미 4 → **전부 4를 봄.**

**reverse의 그 구조 그대로:**
```
getline 버퍼를 그대로 저장 → 모든 노드가 같은 버퍼 → 마지막 줄만 보임
&i를 그대로 넘김          → 모든 스레드가 같은 i  → 마지막 값만 보임
해결도 같음: 각자 자기 사본을 갖게 (strdup / 배열 칸 / 값 전달)
```

**규칙: 스레드에 포인터를 넘길 땐 그 메모리가 스레드보다 오래 살아야 한다.**
- main 스택 변수 → join까지 살아있으면 OK
- 힙(malloc) → 안전, 대신 누가 free할지 정할 것
- **루프 안 지역변수 → ❌**

### attr

스레드 속성 구조체. **스택 크기**(기본 8MB), detach 여부, 스케줄링 정책 등.
```c
pthread_attr_t attr;
pthread_attr_init(&attr);
pthread_attr_setstacksize(&attr, 1024*1024);
```
**NULL = "기본값 써라".** 흔한 경우를 쉽게 만드는 API 설계 패턴.

---

## pthread_join

```c
int pthread_join(pthread_t thread, void **value_ptr);
```

### 왜 `void **`인가

worker가 `void*`를 반환 → 그걸 받을 변수도 `void*` → **그 변수를 채워주려면 주소** → `void**`
```c
void *result;
pthread_join(t, &result);      // &result의 타입 = void**
```
**별 하나는 "반환값이 포인터라서", 다른 하나는 "그 변수를 채워주려고".** (`getline(&line,...)`에서 `char**`가 된 것과 동일)

### 왜 create는 `&p1`, join은 `p1`인가 ★

> **함수가 내 변수를 바꿔야 하면 주소, 읽기만 하면 값.**

```c
pthread_create(&p1, ...);   // 스레드 ID를 "써준다" → 출력 파라미터 → 주소
pthread_join(p1, NULL);     // 이미 있는 ID를 "알려준다" → 입력 → 값
```
같은 패턴: `getline(&line,...)`/`fclose(fp)`, `stat(path, &buf)`.
`pthread_join`은 한 함수 안에 입력(값)과 출력(주소)이 둘 다 있는 예.

### join이 하는 일

**"끝날 때까지 기다린다."** 순서를 정하는 게 아님.
```c
int main() {
    pthread_create(&t, NULL, worker, NULL);
    return 0;    // ❌ worker가 아직 돌아도 프로세스 종료 → worker 사망
}
```
- **main이 끝나면 프로세스 전체가 종료** → 일하던 스레드가 잘려 죽음
- **자원 회수**: 반환값과 TCB가 join될 때까지 남음 → 안 하면 누수(좀비 프로세스와 같은 구조). 필요 없으면 `pthread_detach()`

### pthread_t가 담는 것

**불투명한 핸들(opaque handle).** glibc에선 `unsigned long`이고, 값은 **유저 공간 스레드 디스크립터의 주소.**
```
struct pthread     (glibc, 유저 공간)  ← pthread_t가 가리킴. 스택 정보, TLS, join 상태
struct task_struct (커널)              ← 진짜 스케줄링 단위 (커널의 TCB). gettid()로 ID 확인
```
**직접 뜯거나 `==`로 비교 금지** → `pthread_equal(a, b)` 사용. (구현마다 타입이 다를 수 있음)

### pthread_mutex_t

락 상태를 담는 불투명 구조체 (잠김 여부, 소유자, 대기자 수, 종류).
```c
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;  // 정적. 전역/static용
pthread_mutex_init(&m, NULL);  ...  pthread_mutex_destroy(&m);  // 동적. 힙/구조체 안
```

**대기 방식 3종:**
```c
pthread_mutex_lock(&m);            // 얻을 때까지 대기(블록)
pthread_mutex_trylock(&m);         // 잠겨 있으면 즉시 EBUSY 반환
pthread_mutex_timedlock(&m, &ts);  // 지정 시간까지만
```
`trylock`은 데드락 회피에 쓰임 — "못 잡으면 갖고 있던 걸 놓고 재시도".

---

## ★ 락은 강제가 아니라 규약 (중요한 오해 교정)

> "락을 잡으면 그 변수에 다른 스레드가 접근 못 한다" → **틀림.**

```c
// A
pthread_mutex_lock(&m); counter++; pthread_mutex_unlock(&m);
// B — 락을 안 잡고
counter++;        // ✅ 아무 제지 없이 실행됨. race 발생
```
**하드웨어도 OS도 "이 mutex가 counter를 보호한다"를 모름.** mutex는 그냥 메모리의 flag이고, `lock()`을 호출한 스레드끼리만 서로 양보하는 것.

- **그 데이터를 건드리는 모든 코드**가 같은 락을 잡기로 **약속**해야 성립
- 한 군데만 빼먹어도 무의미. 컴파일러가 검사 못 함 → **개발자 책임**
- 관례: `pthread_mutex_t counter_lock;  // protects: counter, total` 처럼 주석으로 명시
- Rust의 `Mutex<T>`는 **데이터를 락 안에 넣어** 타입으로 강제 — C에선 불가능한 보장

---

## 실습 — race.c

```c
int counter = 0;
void *worker(void *arg) {
    long count = (long)arg;
    for (long i = 0; i < count; i++) counter += 1;
    return NULL;
}
// main: 스레드 2개, 각각 count번. join 후 expected/actual 출력
```
```bash
gcc -o race race.c -Wall -Werror -pthread -g
```
**`-pthread` 필수** — 없으면 `undefined reference` **링크** 에러(컴파일 에러 아님).
**`-g`도 붙일 것** — 없으면 helgrind/gdb가 줄 번호를 못 알려줌.

### 실측 ①: 멀티코어

```
./race 10           → 20 / 20          정확
./race 10000000     → 20000000 / 10715784   절반 손실
./race 1000000 ×5   → 1014563, 1099245, 1211424, 1034759, 1000000
```
- **`1000000`이 나온 실행 = 한 스레드 분량이 통째로 증발.**
- **왜 이렇게 심한가:** x86.py(Ch26)는 단일 CPU + 인터럽트를 흉내냈지만, **실제는 두 코어가 진짜 동시에** load-add-store를 함 → 매 순간 서로 덮어씀.
- **Ch10 연결 — 캐시 라인 핑퐁:** counter가 한 캐시 라인에 있어 두 코어가 계속 무효화. 그래서 **느리기까지** 함(단일 스레드보다 느릴 수도).
- `./race 10`이 맞는 이유: 너무 빨리 끝나 **겹칠 틈이 없음.**

### 실측 ②: 단일 코어 (`taskset -c 0`)

```
taskset -c 0 ./race 1000000 ×5   → 전부 2000000  ✅ 완벽
taskset -c 0 ./race 200000000    → 231084688, 245416312, 246139072  ❌ 40% 손실
```

**★ 여기서 내 첫 설명이 반증됨.** "단일 코어는 문맥 교환당 최대 1 손실"이라 했는데 틀렸음.

**실제 손실 메커니즘:**
```
counter = 100
A: load → reg_A = 100
[인터럽트]
B: 자기 슬라이스 내내 100만 번 → counter = 1,100,000
[인터럽트, reg_A=100 복원]
A: add → 101, store → counter = 101
   ★ B의 100만 개가 통째로 증발. counter가 과거로 되돌아감
```
- **A가 못 저장한 자기 몫은 항상 1**(매 반복 store하니까)
- **A의 낡은 값이 덮어쓴 남의 몫 = B가 그 슬라이스에 한 전부**
- **"낡은 값 하나가 그 사이의 모든 진전을 되돌린다"** — lost update의 진짜 무서움 (DB에서 같은 이름으로 심각하게 다뤄짐)

**숫자 검증:**
```
루프 1회 ≈ 5 사이클 ≈ 1.7ns   (같은 주소 의존이라 파이프라인이 못 겹침)
타임 슬라이스 ≈ 1~4ms         → 비율 약 60만 : 1
2억 × 1.7ns ≈ 0.34초/스레드, 총 0.7초 ÷ 1~4ms ≈ 문맥 교환 200~700회
취약 구간(load~store)에 걸릴 확률 ~1/3 → 나쁜 교환 70~230회
× 슬라이스당 100~200만 손실 = 1억~4억
실측 손실 1.69억 ✅ 일치
```

**정정된 결론:**
| | 손실 |
|--|--|
| 멀티 코어 | 매 반복 충돌 → **반복 횟수에 비례**(~50%) |
| 단일 코어 | 나쁜 교환마다 **상대 슬라이스 분량** → 교환 횟수 × 슬라이스 크기 |

→ **단일 코어가 안전한 게 아니라 작업이 짧아 안 드러났을 뿐.**
**실무 교훈: 100만은 완벽 통과, 2억은 40% 손실. 테스트 규모를 키우기 전엔 안 보이는 버그.** 단일 코어 환경에서만 테스트하면 영원히 못 잡음.

### 실측 ③: 스레드 실행 순서는 보장 안 됨

`t0.c`(A/B 출력) 100회:
```
72 AB / 28 BA
```
- **join은 실행 순서를 정하지 않음.** `create` 순간 둘 다 실행 가능 상태 → 누가 먼저 CPU를 잡는지는 스케줄러 마음.
- join은 "**main이 먼저 나가지 못하게 붙잡는 것**"일 뿐.
- 순서를 강제하려면 `create(p1) → join(p1) → create(p2)` — **하지만 동시성이 0이 됨.**
  → 동시에 돌면서 특정 지점만 동기화하려면 **조건 변수(Ch30) / 세마포어(Ch31)** 필요.
- **만약 99:1이었다면** 100번 테스트해도 다 AB → "순서가 보장된다"고 믿게 됨. race가 발견 안 되는 이유.

---

## 숙제 — helgrind (`threads-api`)

**valgrind는 도구 모음**, `--tool`로 선택:
```bash
valgrind --tool=memcheck ./p    # 메모리 (기본값, Ch14)
valgrind --tool=helgrind ./p    # 스레드 race/데드락
valgrind --tool=drd ./p         # 스레드 (다른 알고리즘)
valgrind --tool=cachegrind ./p  # 캐시 프로파일
```

**helgrind가 잡는 것:** data race / 락 순서 위반(데드락 가능성) / 잘못된 pthread API 사용.

### 어떻게 잡나 — happens-before

**"운 좋게 충돌해야" 잡는 게 아님.** 실행 중 "누가 어느 주소를 언제 접근했나 + 사이에 동기화가 있었나"를 추적해서, **동기화 없이 같은 메모리를 건드렸으면 실제 충돌이 없어도** race로 판정.
→ **Ch26의 "테스트로는 못 잡는다"에 대한 답.** 단, 그 코드 경로가 실행은 돼야 함.
**대가: 10~100배 느림** (ASan과 같은 트레이드오프 — 개발/CI용).

### race.c에 적용

```
expected: 20000
actual  : 20000        ← 이번엔 결과가 맞았는데도
ERROR SUMMARY: 2 errors from 2 contexts
```
**결과가 정확해도 경고를 냄** ← 핵심.

```
Possible data race during READ  ... conflicts with a previous WRITE   (read-write)
Possible data race during WRITE ... conflicts with a previous WRITE   (write-write)
Locks held: none                                    ← 판정 근거
Address 0x10c02c is 0 bytes inside data symbol "counter"   ← 변수 이름까지
```
- `counter++`가 load와 store를 다 하니 두 종류가 다 잡힘
- **`Locks held: none`이 결정적 증거.** mutex를 걸면 `Locks held: 0x...`로 바뀌고 경고 사라짐

**errors 개수 = 충돌 횟수가 아니라 "서로 다른 접근 지점 쌍(context)"의 수.**
같은 지점에서 만 번 충돌해도 context는 1개 → `./race 10000`이든 `./race 1억`이든 **똑같이 2개.**
**도구의 목적은 "고칠 코드 위치"지 피해 규모 집계가 아님.** 규모는 expected vs actual로 직접 측정.
(memcheck이 "400 bytes lost in 1 blocks"로 블록 단위 보고한 것과 같은 성격.)

### main-deadlock.c

```c
pthread_mutex_t m1, m2;
worker(arg): arg==0 ? (lock m1; lock m2) : (lock m2; lock m1)
```
**보호할 데이터가 없음 — 데드락만 순수하게 보여주려는 예제.** 데드락은 데이터와 무관하게 **락 자체(자원)만으로** 발생.

```
p1: lock(m1) 성공          → m1 보유
p2: lock(m2) 성공          → m2 보유
p2: lock(m1) 시도 → 블록    (p1이 보유)
p1: lock(m2) 시도 → 블록    (p2가 보유)
→ 서로가 서로를 기다림. 영원히 안 풀림
```
**`pthread_mutex_lock`은 에러가 아니라 블록** — 스레드가 **Blocked 상태**(Ch4)로 들어가 CPU를 못 받음.

**순서 자체가 문제가 아니라 "서로 다른 순서"가 문제.** 둘 다 `m1→m2`였다면:
```
p1: lock(m1) 성공 / p2: lock(m1) 시도 → 대기
p1: lock(m2) 성공 (p2는 m1에서 막혀 m2를 쥘 수 없음) → 진행 → unlock
```
**첫 락에서 막히니 "두 번째 락을 쥔 채 기다리는" 상황이 안 생김.**

**데드락 4조건 (Ch32에서 정식):**
1. 상호 배제 2. **점유 대기(hold and wait)** 3. 비선점 4. **순환 대기(circular wait)** ← 이 예제
**하나만 깨면 데드락 없음.** 락 순서 통일 = 4번을 깸.
실무 규칙: 락을 여러 개 잡으면 **전역적으로 정해진 순서**(예: 락의 주소 순).

### 데드락 관찰 방법 (출력이 없는 프로그램)

```bash
time ./main-deadlock; echo "exit: $?"

for i in $(seq 20); do
  timeout 1 ./main-deadlock
  [ $? -eq 124 ] && echo "run $i: DEADLOCK" || echo "run $i: ok"
done          # timeout이 죽이면 종료 코드 124

./main-deadlock & sleep 1
ps -o pid,stat,wchan,cmd -p $!     # WCHAN이 futex_wait면 락 대기 중
gdb -p $! -batch -ex "thread apply all bt"   # ★ 실무 표준: 매달린 프로세스에 gdb 붙여
                                             #   각 스레드가 어디서 멈췄나 확인
```

### helgrind의 데드락 탐지 — 락 순서 그래프

**이번 실행은 데드락이 안 걸렸는데도 잡아냄.**
```
Required order was established by: m1 → m2      ← 먼저 본 순서를 규칙으로 등록
Observed (incorrect) order is:     m2 → m1      ← 위반
Address 0x10c040 is ... "m1" / 0x10c080 ... "m2"  ← 변수명까지
```
데드락은 타이밍이 맞아야만 걸리는데(20번 돌려도 몇 번), helgrind는 **한 번만 돌려도 순서 어긋남을 확정.**
**데드락을 기다리지 않고 그 가능성을 잡아내는 것** — 도구의 가치.

### ★ main-deadlock-global.c — 거짓 양성

```c
pthread_mutex_t g;    // 전역 락 추가
worker: lock(g); ...기존 m1/m2 로직 그대로... unlock(g);
```
- **점유 대기를 깬 것** — 락 순서는 여전히 반대지만, `g`가 **두 스레드의 동시 진입 자체를 막아** "하나 쥐고 기다리는" 상황이 안 생김.
- **문제: 병렬성이 죽음.** m1·m2가 무관한 자원이어도 완전 직렬화.
  (A/B 순서를 join으로 해결했을 때와 같은 함정 — 정확성을 얻고 동시성을 잃음.)
- **더 나은 해법: 락 순서 통일** — 병렬성을 안 죽이고 순환 대기만 깸.

**helgrind는 여전히 1 error를 보고 = 거짓 양성(false positive).**
락 순서 그래프에 `g→m1→m2`와 `g→m2→m1`이 남아 m1↔m2 사이클이 보이는데, **`g`가 상호 배제시켜 애초에 동시 실행이 불가능하다는 전역적 추론은 못 함.**

| | 뜻 | 결과 |
|--|--|--|
| **거짓 양성** | 안전한데 경고 | 시간 낭비, 심하면 **경고를 무시하는 습관** |
| **거짓 음성** | 위험한데 침묵 | 버그 놓침 |

helgrind는 거짓 양성 쪽으로 치우친 설계(놓치는 것보다 낫다). 확실한 거짓 양성은 **suppression 파일**로 등록(`--suppressions=`). 출력의 `suppressed: 8 from 8`이 그 메커니즘(시스템 라이브러리용).

> **도구는 보조다. "helgrind 통과 = 안전"도 "경고 = 버그"도 둘 다 성급.**
> Ch26 결론의 완성: **테스트로는 못 잡고(거짓 음성), 도구도 완벽하지 않다(거짓 양성). 결국 설계로 보장해야 한다.**

### main-signal.c vs main-signal-cv.c — spin vs block (Ch30 예고)

```c
// main-signal.c
while (done == 0) ;                       // 스핀 대기(busy-wait). Running 유지
// main-signal-cv.c
pthread_cond_wait(&s->cond, &s->lock);    // 블록. Blocked 상태로 잠듦
```

| | spin | cond_wait |
|--|--|--|
| 스레드 상태 | **Running** | **Blocked** (Ch4) |
| CPU 소비 | **100% 낭비** | 0 |
| 반응 속도 | 즉각(유일한 장점) | 깨우는 비용 약간 |

**★ 단일 코어에서 spin은 재앙:** 기다리는 스레드가 CPU를 안 놓아서 **`done`을 세팅해줄 스레드가 실행조차 못 함** → 영원히 대기.
```bash
taskset -c 0 timeout 3 ./main-signal      # 매달릴 수 있음
taskset -c 0 ./main-signal-cv             # 정상
```
**Ch28에서 스핀락 vs 뮤텍스로 재등장.** 짧게 기다리면 spin이 유리할 수도(잠들었다 깨는 비용 > 잠깐 도는 비용), **길게 기다리면 무조건 block.**

(cv 버전이 mutex·cond·flag를 **구조체로 묶은 것**은 "이 락이 이것들을 함께 지킨다"를 코드로 드러낸 것 — 위의 "락은 규약" 관례의 실물.)

---

## 실전에 쓰는 API는 열 개 남짓

```c
pthread_create / pthread_join                    // 스레드
pthread_mutex_lock / unlock / init               // 락
pthread_cond_wait / signal / broadcast           // 조건 변수 (Ch30)
sem_init / sem_wait / sem_post                   // 세마포어 (Ch31)
```
나머지(attr 계열, rwlock, spinlock, barrier, trylock...)는 필요할 때 `man`으로.
**API 목록을 붙들고 외우지 말고, 코드 예제에 집중해서 써보며 익힐 것.** wish 셸에서 `strsep`/`access`/`dup2`를 한 번 써보고 손에 붙은 것처럼.

---

## 실전 포인트

- **함수 포인터 읽기** (`void *(*f)(void *)`) — 안에서 밖으로. 면접·코드 리딩에서 자주.
- **create는 주소, join은 값** — 출력 파라미터 vs 입력 파라미터로 설명.
- **`(void*)(long)` 2단계 캐스팅 이유** — 크기 맞춤. 그리고 A방식에선 역참조 금지.
- **루프 변수 주소를 스레드에 넘기는 버그** — 전형적. reverse의 strdup 문제와 같은 구조.
- **락은 규약이지 강제가 아님** — 모든 접근 지점이 같은 락을 잡아야.
- **helgrind는 결과가 맞아도 경고** (happens-before) / **거짓 양성도 있음**.
- **단일 코어 테스트로는 race를 못 잡는다** — 실측 데이터로 답하면 강력.

---

## 다음
- **Ch 28 Locks** — `xchg`/test-and-set/CAS로 **락을 직접 만든다.**
  ※ Ch26의 부트스트랩 문제("락을 만들려면 이미 원자성이 필요")의 답.
  ※ Phase 2의 "메커니즘은 자기 자신에 의존할 수 없다"가 재등장.
  ※ spin vs block이 스핀락 vs 뮤텍스로.
- 전체 로드맵: [ROADMAP.md](../ROADMAP.md)
