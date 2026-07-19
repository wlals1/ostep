# OSTEP Ch 6 숙제 — 측정 실습 (Measurement)

시스템콜과 문맥 교환의 **실제 비용을 나노초 단위로 측정.** 오늘 개념("트랩·context switch가 공짜가 아니다")을 숫자로 확인.
`syscall-cost.c`, `ctx-cost.c` 참고.

---

## 측정 결과 (내 환경, 8코어 WSL)

| 항목 | 1회 비용 |
|------|----------|
| **시스템콜** (0바이트 read) | **≈ 172 ns** |
| **문맥 교환** (파이프 핑퐁) | **≈ 1207 ns (1.2 μs)** |

→ **문맥 교환이 시스템콜의 약 7배.**

---

## 파트 1 — 시스템콜 비용

**핵심 아이디어: 한 번은 너무 빨라(timer 해상도 1μs 이하) 못 잰다 → 수백만 번 반복 후 총시간 / 횟수.**

- **시스템콜 선택 = 0바이트 `read`**:
  - `read(fd, buf, 0)` = 데이터는 0개 읽지만 **트랩은 발생** → 순수 "트랩 왕복 비용"만 측정.
  - `fork`(무거운 작업 섞임), `read(...,4096)`(데이터 복사 섞임), `getpid`(glibc 캐싱으로 트랩 안 할 수도) → 부적합.
- **fd는 유효해야**: `/dev/zero`나 STDIN. (닫힌/쓰기전용 fd는 에러 경로.)

```c
struct timeval start, end;
char buf[10];
gettimeofday(&start, NULL);
for (int i = 0; i < 10000000; i++)   // 천만 번
    read(fd, buf, 0);
gettimeofday(&end, NULL);
long usec = (end.tv_sec - start.tv_sec)*1000000L + (end.tv_usec - start.tv_usec);
double ns = (double)usec * 1000.0 / 10000000;   // μs→ns, 횟수로 나눔
```

**결과:** 천만 번에 1.7초 → **172 ns/회.**

**겪은 버그:**
- `gettimeofday`는 `struct timeval`(tv_sec+tv_usec)을 채움 — `int`로 받으면 스택 오염(UB).
- 구조체끼리 `-` 불가 → 초·마이크로초 각각 계산.

---

## 파트 2 — 문맥 교환 비용

**난관 2개:**
1. **8코어라 두 프로세스가 병렬 실행 → 문맥 교환 안 일어남.**
   → **`sched_setaffinity`로 같은 코어(CPU 0)에 묶어** 번갈아 돌게 강제.
2. **문맥 교환을 억지로 유발해야 함.**
   → **파이프 2개로 핑퐁**: 한쪽이 read에서 blocked → 상대로 전환(=문맥 교환).

**파이프 배선 (방향 주의!):**
- **pipe1 = 부모(A) → 자식(B)**: 부모 `write(p1[1])`, 자식 `read(p1[0])`
- **pipe2 = 자식(B) → 부모(A)**: 자식 `write(p2[1])`, 부모 `read(p2[0])`

```c
#define _GNU_SOURCE
#include <sched.h>
#define N 1000000
...
cpu_set_t set; CPU_ZERO(&set); CPU_SET(0, &set);
sched_setaffinity(0, sizeof(set), &set);   // fork 전 → 자식도 상속

char c = 'x';
int rc = fork();
if (rc == 0) {                    // 자식 B
    for (int i = 0; i < N; i++) {
        read(p1[0], &c, 1);        // p1에서 읽기 (blocked → A로 전환)
        write(p2[1], &c, 1);       // p2에 쓰기
    }
} else {                          // 부모 A
    gettimeofday(&start, NULL);
    for (int i = 0; i < N; i++) {
        write(p1[1], &c, 1);       // p1에 쓰기
        read(p2[0], &c, 1);        // p2에서 읽기 (blocked → B로 전환)
    }
    gettimeofday(&end, NULL);
    long usec = (end.tv_sec-start.tv_sec)*1000000L + (end.tv_usec-start.tv_usec);
    printf("per ctx switch: %f ns\n", (double)usec*1000.0 / (N*2));  // 왕복당 교환 2번 → ×2
}
```

**계산:** N번 왕복 = 문맥 교환 **2N번** → `1회 = 총시간 / (2N)`.

**결과:** 백만 왕복(2백만 교환)에 2.4초 → **1207 ns/회.**

**겪은 버그:**
- 부모가 `p2`에 쓰고 `p2`에서 읽음(자기 것 자기가 읽음) → 핑퐁 안 됨. **부모는 p1 쓰기 / p2 읽기.**
- 나누는 수를 `N`이 아닌 하드코딩(`10000000`)으로 → 10배 틀림. **`N*2`로.** (하드코딩의 위험.)
- `wrtie` 오타, `&rc`(pid)를 버퍼로 씀 → 별도 `char c`.

---

## 왜 문맥 교환이 시스템콜보다 7배 비싼가 (핵심 해석)

**시스템콜(172ns) = 트랩 왕복만:**
- 사용자→커널 모드 전환 + trapframe 저장/복원 + iret. **한 프로세스 안.** 주소 공간 안 바뀜.

**문맥 교환(1207ns) = 그 위에 더:**
- **swtch**: 커널 문맥(context) 저장/복원 (A→스케줄러→B)
- **CR3 교체(switchuvm) → TLB flush**: 주소 공간 전환. ← 시스템콜엔 없는 큰 비용. TLB 비워지면 새 프로세스가 한동안 TLB miss.
- **스케줄러 로직** (다음 프로세스 선택)
- **캐시 오염**: CPU 캐시가 A 데이터 → B 데이터로 교체됨.

→ 특히 **CR3 교체 + TLB flush**가 시스템콜과의 결정적 차이. (주의: "PCB를 통째로 교환"이 아님 — PCB는 프로세스 테이블에 그대로. swtch가 context를 교환하고 CR3를 갈아끼우는 것.)

---

## 배운 것 / 의미

- **트랩도 문맥 교환도 공짜가 아니다** — 실측 가능한 비용.
- 그래서 **시스템콜을 줄이려 버퍼링**(stdio가 write를 모아서 하는 이유), **문맥 교환을 줄이려 스케줄링 정책**을 고민.
- Ch 7 스케줄링에서 "얼마나 자주 전환할지" 트레이드오프가 나오는 이유 = 이 1.2μs가 자주 쌓이면 오버헤드.
- 검증 팁: `strace -c ./syscall-cost`로 실제 트랩 횟수 확인. 문맥교환 값이 시스템콜보다 작으면 배선/affinity 문제.

---

> 챕터 정리: [../README.md](../README.md) · 전체 로드맵: [ROADMAP.md](../../ROADMAP.md)
