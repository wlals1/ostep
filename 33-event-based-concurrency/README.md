# OSTEP Ch 33 — Event-based Concurrency 정리

**"스레드를 아예 안 쓰고 동시성을 만드는 법."** Phase 3의 마지막 장.

---

## 동기 — 스레드가 싫은 두 가지 이유

**① 버그를 다루기가 어렵다** — race, 데드락, 재현 불가 (Ch26~32 내내 본 것)

**② 스케줄러를 제어할 수 없다**
> "이 세상 모든 스케줄러를 미리 알고 대비해서 코드를 짤 수 없다."

스레드를 만들고 나면 언제 어떤 순서로 실행될지 개발자가 못 정함(Ch27의 72:28). **모든 인터리빙에서 옳은** 코드를 짜야 하는데, 그 인터리빙은 하드웨어·부하·OS 버전마다 다름.

---

## 발상의 전환 — 스레드 하나만 쓴다

```c
while (1) {
    events = getEvents();       // 무슨 일이 일어났나
    for (e in events)
        processEvent(e);        // 하나씩 처리
}
```

**핵심: 핸들러가 실행되는 동안 아무도 끼어들 수 없다.** 스레드가 하나니까.
→ **race 없음, 락 불필요. 원자성이 공짜.**

한 번에 하나씩 처리하지만 각 처리가 짧으면 겉보기엔 동시에 하는 것처럼 보임.

### 이벤트 확인 — select / poll / epoll

```c
int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout);
```
"이 fd들 중 **읽을 준비가 된 게 있나?**" — 없으면 대기, 생기면 알려줌.

```c
while (1) {
    select(...);                  // 어느 소켓에 데이터가 왔나
    for (준비된 fd마다) 처리;
}
```
(`select`는 fd 수에 비례해 느려짐 → 리눅스는 **`epoll`**, BSD는 `kqueue`를 씀. 요즘 서버는 epoll.)

---

## 함정 ① — 블로킹 호출 하나가 전체를 멈춘다

핸들러 안에서 `read()`로 디스크를 읽으면, 그 사이 **모든 클라이언트가 멈춤.**
스레드였다면 그 스레드만 자고 다른 스레드가 돌았을 텐데, 여기선 스레드가 하나뿐.

### 해법 A — 논블로킹 I/O + 이벤트 등록

```c
int fd = open("file", O_RDONLY | O_NONBLOCK);   // ★ 논블로킹
read(fd, buf, size);   // 데이터 없으면 즉시 -1 (EAGAIN) 반환. 기다리지 않음
```
```
① read 시도 → 아직 없음 → 즉시 반환
② 그 fd를 select 목록에 등록 → 루프로 복귀 → 다른 클라이언트 처리 ✅
③ 나중에 select가 "준비됨" 통보 → 이어서 처리
```
**"기다리는 대신 돌아왔다가 나중에 다시 온다."**

### 해법 B — 비동기 I/O (AIO)

디스크는 논블로킹이 잘 안 통함(파일 fd는 항상 "준비됨"으로 나옴) → 별도 API 필요.
```c
struct aiocb cb = { .aio_fildes = fd, .aio_buf = buf, ... };
aio_read(&cb);        // "이거 읽어놔" 하고 즉시 반환
...  다른 일  ...
aio_error(&cb);       // 끝났나 확인 (또는 시그널로 통보)
```
**"요청만 걸어두고 즉시 반환, 완료는 나중에 확인."**
리눅스 최신판은 **`io_uring`** — 훨씬 빠르고 요즘 고성능 서버가 사용.

---

## ★ 함정 ② — 스택이 없다 (manual stack management)

`read`가 즉시 반환하고 나중에 다시 온다면, **"내가 뭘 하던 중이었는지"를 어떻게 기억하나?**

**스레드는 스택이 기억해줬음:**
```c
void handle(int fd) {
    request = read_request(fd);        // I/O 1
    user    = db_lookup(request.user); // I/O 2
    data    = read_file(user.file);    // I/O 3
    send(fd, data);
}
```
위에서 아래로 읽히고, 중간 상태(`fd`, `request`, `user`)를 **스택이 알아서 들고 있음.**

**이벤트 방식은 핸들러가 반환해버리므로 스택이 사라짐:**
```c
typedef struct { int fd; request_t req; user_t user; } ctx_t;

void step1(int fd) {
    ctx_t *c = malloc(sizeof(ctx_t));   // 상태 상자를 직접 생성
    c->fd = fd;
    async_read_request(fd, c, step2);   // "끝나면 step2 불러줘"
}
void step2(ctx_t *c, request_t r) { c->req = r;  async_db_lookup(r.user, c, step3); }
void step3(ctx_t *c, user_t u)    { c->user = u; async_read_file(u.file, c, step4); }
void step4(ctx_t *c, data_t d)    { send(c->fd, d); free(c); }
```

**같은 일인데 함수 4개로 찢어짐.** 이걸 **stack ripping(스택 찢기)** 또는 manual stack management라고 함.

**무엇이 고통인가:**
- **흐름이 안 보임** — `step2` 다음이 뭔지 알려면 코드를 뒤져야 함
- **상태를 손으로 관리** — 만들고, 채우고, 넘기고, `free`까지 → **누수·이중 해제 위험**
- **에러 처리가 지옥** — 스레드면 조기 `return` 한 번인데, 여기선 **네 군데 전부**에 에러 경로
- **디버깅 어려움** — 스택 트레이스에 `step3`만 보이고 "왜 여기 왔는지"가 안 남음
- (상태 저장/확인 오버헤드도 추가)

이게 **콜백 지옥(callback hell)**. 자바스크립트 초창기 코드가 딱 이랬음.

### 해결 — async/await

```javascript
async function handle(fd) {
    const request = await readRequest(fd);
    const user    = await dbLookup(request.user);
    const data    = await readFile(user.file);
    send(fd, data);
}
```
**스레드 버전처럼 위에서 아래로 읽히는데 실제 동작은 이벤트 기반.**
컴파일러가 이 함수를 **자동으로 상태 기계(state machine)로 변환** — 위의 `step1~4`와 `ctx_t`를 기계가 생성.

> **"스택 찢기를 컴파일러에게 시킨다."**

Node.js, Python `asyncio`, Rust `async`, C# `async/await` — 전부 이 문제를 푼 것.
(Go의 고루틴은 방식이 다름 — 경량 스레드 + 런타임 스케줄러로 스택을 유지하면서 같은 효과)

---

## 한계

**① 멀티코어를 못 쓴다** ★ 결정적
스레드 하나 = 코어 하나. 8코어에서 1/8만 사용. 문맥 교환을 아껴도 코어 7개를 노는 손해가 훨씬 큼.

**② CPU 바운드 핸들러가 전체를 막는다**
이벤트 루프는 **협력적(cooperative)** 스케줄링 — 핸들러가 자진 반환할 때까지 아무것도 못 함.
스레드였다면 스케줄러가 강제로 뺏어갔을(선점) 텐데.

**③ 암묵적 블로킹**
`read`를 논블로킹으로 바꿔도 **페이지 폴트**(Ch21)가 나면 그 스레드가 잠듦 → 전체 정지.
개발자가 통제할 수 없는 블로킹이 남아 있음.

**④ API 변화에 취약** — 라이브러리가 어느 날 블로킹 호출을 추가하면 전체가 느려짐. 모든 호출이 논블로킹인지 계속 감시해야 함.

---

## 언제 이기나 — 워크로드가 답을 정한다

```
I/O 대기가 지배적  → 이벤트 기반 승
CPU 계산이 지배적  → 스레드 승 (멀티코어를 써야 함)
```

**C10K 문제** — "동시 접속 1만 명을 어떻게 처리하나":
```
스레드 방식: 클라이언트 1만 = 스레드 1만 개
             → 스택 1만 개(8MB씩이면 80GB), 문맥 교환 폭발
이벤트 방식: 스레드 1개 + fd 1만 개
             → 메모리 적고 문맥 교환 없음
```
답이 이벤트 기반이었고 — **nginx, Node.js, Redis**가 이 구조.
(아파치의 옛 방식은 요청당 프로세스/스레드였고 그래서 nginx에 밀림)

### 실무는 하이브리드

**코어 수만큼 프로세스/스레드를 띄우고 각각이 이벤트 루프를 돌림.**
nginx의 worker process, Node.js의 cluster 모듈. **멀티코어도 쓰고 연결도 많이 받음.**

---

## 실전 포인트

- **이벤트 기반의 동기 두 가지** — 버그 난이도 + 스케줄러 제어 불가.
- **왜 락이 필요 없나** — 스레드가 하나라 핸들러 실행 중 끼어들 수 없음. 원자성이 공짜.
- **블로킹 호출 하나가 전체를 멈춘다** → 논블로킹 I/O / AIO / io_uring.
- **stack ripping과 콜백 지옥**, 그리고 **async/await이 그걸 컴파일러로 해결**한다는 연결. 면접에서 async 물어보면 이 배경을 말하면 강함.
- **멀티코어를 못 쓴다**는 결정적 한계와 **하이브리드(nginx worker)** 해법.
- **워크로드가 선택을 정한다** — I/O 바운드 vs CPU 바운드. (Ch22 "정책 순위는 워크로드가 결정"과 같은 사고)

---

## 다음
- **Phase 3 개념 완료** (Ch26~33).
- **Ch 10 재방문** — 멀티프로세서 스케줄링을 락·캐시 일관성 관점에서.
- **Phase 3 프로젝트** — concurrency-pzip(멀티스레드 압축), mapreduce, webserver.
  ※ webserver 프로젝트가 이 장의 스레드 방식 vs 이벤트 방식을 직접 비교하는 것.
- 이후 **Phase 4 xv6** — `spinlock.c`에서 Ch28의 락 구현을 실제 코드로.
- 전체 로드맵: [ROADMAP.md](../ROADMAP.md)
