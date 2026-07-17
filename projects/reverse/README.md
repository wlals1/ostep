# reverse — OSTEP Initial Project 정리

파일(또는 표준입력)의 줄들을 **역순으로 출력**하는 간단한 UNIX 유틸리티.
C/UNIX 시스템 프로그래밍 워밍업용. 이 문서는 프로젝트 정리 + 만들면서 헷갈렸던 개념 복습 노트다.

---

## 1. 사용법 (3가지 모드)

| 실행 | 입력 | 출력 |
|------|------|------|
| `./reverse` | stdin | stdout |
| `./reverse input.txt` | input.txt | stdout |
| `./reverse input.txt output.txt` | input.txt | output.txt |

핵심 규칙: **`argc = 파일 인자 개수 + 1`** (argv[0]은 프로그램 이름이라 argc는 최소 1).

## 2. 요구사항 & 에러 처리

모든 에러는 `stderr`로 출력하고 `exit(1)`. 메시지는 **정확히** 일치해야 함(자동 테스트가 문자열 비교).

| 상황 | 메시지 |
|------|--------|
| 인자 너무 많음 (argc > 3) | `usage: reverse <input> <output>` |
| 파일 못 엶 | `error: cannot open file 'FILENAME'` |
| 입력=출력 같은 파일 | `Input and output file must differ` |
| malloc/strdup 실패 | `malloc failed` |

제약: 줄 길이 무제한(→ `getline`), 파일 크기 무제한(→ 동적 자료구조).

## 3. 핵심 설계 결정

- **왜 읽으면서 바로 출력 못 하나?** 역순이라, 첫 출력 줄(=입력 마지막 줄)을 찍으려면 파일을 **끝까지 다 읽어** 저장해둬야 함.
- **왜 고정 크기 배열은 안 되나?** 줄 개수·길이가 무제한이라 용량 보장이 안 됨 → 동적 메모리.
- **연결 리스트 + head에 prepend** → 읽는 순서대로 앞에 꽂으면 리스트가 자동으로 역순 → head부터 출력하면 끝. (역순을 공짜로 얻음)
- **`getline`** → 줄 길이 무제한을 자동 처리(버퍼 부족하면 스스로 realloc).
- **`strdup`으로 복사** → getline 버퍼는 재사용되므로 그대로 저장하면 안 됨(아래 개념 정리 참고).
- **`stat` + inode 비교** → 같은 파일 판별(이름이 달라도 잡음).

## 4. 빌드 · 테스트 · 메모리 검증

```bash
gcc -o reverse reverse.c -Wall
./reverse text.txt                 # 파일 → 화면(역순)
cat text.txt | ./reverse           # stdin → 화면
./reverse text.txt out.txt         # 파일 → 파일
./reverse ./text.txt text.txt      # 같은 파일 에러(이름 달라도 inode로 잡힘)
./reverse a b c d ; echo $?        # usage 에러, 종료코드 1
valgrind ./reverse text.txt        # "no leaks" + "0 errors" 확인
```

목표: valgrind에서 **alloc 수 = free 수, 0 errors**.

---

## 5. 대화하며 물어본 것 / 헷갈린 것 정리

### getline
- 시그니처: `ssize_t getline(char **lineptr, size_t *n, FILE *stream)`
- **반환값은 `char*`가 아니라 숫자(`ssize_t`)** — 읽은 바이트 수, EOF면 **-1**. 이게 루프 종료 조건.
- 인자에 **주소**를 넘김: `getline(&line, &len, fp)`. getline이 내 `line`·`len` 변수를 직접 채워 돌려주기 때문(→ 포인터로 값 돌려받기).
- **크기를 미리 알 필요 없음.** `char *line = NULL; size_t len = 0;`로 시작하면 getline이 알아서 malloc/realloc하고 크기를 `*n`에 갱신.
- **커서 자동 이동**: 호출할 때마다 다음 줄로 전진. 수동으로 옮길 필요 없음.
- **버퍼 재사용**: 매 호출이 같은 버퍼를 덮어씀 → 노드에 그 포인터를 그대로 저장하면 안 됨(strdup 필요).

### size_t / ssize_t / __ssize_t
- `size_t` = 크기·개수용 **unsigned** 정수 (sizeof, malloc 인자, 인덱스). 음수 불가.
- `ssize_t` = **signed** 크기. 크기 또는 **-1(에러)**를 반환하는 함수용(getline, read, write).
- `__ssize_t` = glibc **내부 이름**. `__` 접두사는 "구현 예약"이라 직접 쓰면 안 됨 → **`ssize_t`** 사용.

### 포인터로 값 돌려받기 (`&` 넘기기)
- 함수가 내 변수를 **직접 바꿔서 돌려줘야** 할 때, 값이 아니라 **주소**를 넘김.
- 예: `getline(&line, &len, fp)`, `stat(path, &statbuf)`. 전부 같은 패턴.

### aliasing & strdup (화이트보드 비유)
- getline 버퍼 = 화이트보드 1개. 매 줄 읽을 때마다 지우고 새로 씀.
- 노드에 그 버퍼 주소를 그대로 저장하면 → 모든 노드가 **같은 화이트보드**를 가리킴 → 다 읽고 나면 전부 **마지막 줄**만 보임(+ realloc되면 옛 주소는 dangling).
- 해결: 줄마다 **자기만의 종이에 복사**(`strdup`) → 각 노드가 독립된 메모리 소유.

### 메모리 소유권 & free
- **`free`는 재귀적이지 않다.** `free(node)`는 노드 블록만 해제, 안의 `node->line`(별도 할당)은 **자동 해제 안 됨**.
- 노드와 문자열은 **서로 다른 두 할당**(calloc vs strdup). 각각 free 필요.
- **해제 순서**: `free(s->line)` 먼저 → `free(s)` 나중. (노드 먼저 free하면 안의 `line` 주소를 못 읽음)
- **use-after-free**: 노드가 소유한 문자열을 미리 free하면, 노드의 포인터가 해제된 메모리를 가리킴(dangling) → 나중에 읽으면 UB.
- 규칙: `malloc/calloc/strdup` 한 번 = `free` 한 번. "이 할당은 누가 소유하고 언제 해제?"를 항상 자문.

### 헤더 누락 / implicit declaration / 컴파일 vs 링크 에러
- "unknown type" / "implicit declaration of function" = 컴파일러가 그 이름을 모름 = **헤더 누락**.
  - `NULL`→`<stddef.h>`, `pthread_t`→`<pthread.h>`, `write`/`close`→`<unistd.h>`, `strdup`→`<string.h>`, `stat`→`<sys/stat.h>`
  - `"did you mean fwrite?"`는 추천이 아니라 컴파일러의 비슷한-이름 추측일 뿐. 함정.
- **컴파일 에러** = 선언/헤더 문제. **링크 에러**(`undefined reference`) = 라이브러리 연결 문제(`-pthread`, `-lm` 등).

### assert는 에러 처리용이 아니다
- `assert`는 **프로그래머 버그(불변식)** 검출용. 실패 시 고유 형식 출력 + `abort()`(종료코드 134) + `-DNDEBUG`면 무력화.
- 사용자/환경 탓 에러(인자 틀림, 파일 없음)는 **정상 처리** 대상 → `fprintf(stderr, ...)` + `exit(1)`.

### `=` vs `==`
- `=`는 대입, `==`는 비교. `assert(rc = 13)`은 항상 참(대입 결과 13). `-Wall`이 경고.

### 연산자 우선순위 (`=` vs `!=`)
- `while (x = getline(...) != -1)`는 `x = (getline(...) != -1)`로 묶임(`!=`가 우선). x엔 0/1만 들어감.
- 의도대로: `while ((x = getline(...)) != -1)` — 대입을 괄호로 명시.

### argc / argv 매핑
- `argv[0]` = 프로그램 이름. `argc = 파일 인자 수 + 1`.
- argc==1(파일 0개, stdin), ==2(파일 1개), ==3(파일 2개), >3(에러).

### fopen 모드 & truncation
- `"r"` 읽기, `"w"` 쓰기(**파일을 즉시 비움/생성**).
- 그래서 "같은 파일" 체크는 **출력을 "w"로 열기 전에** 해야 함(안 그러면 입력이 날아감).

### FILE* (스트림) vs inode (디스크의 파일)
- `fopen`은 매번 **새 스트림(새 FILE 객체, 다른 포인터)** 반환 → 같은 파일 두 번 열어도 포인터 다름. 포인터 비교로는 같은 파일 판별 불가.
- 디스크 위 파일의 정체성 = **inode(`st_ino`) + device(`st_dev`)**. 이름이 달라도 같은 파일이면 같음.

### 시스템 함수 반환값 확인 습관
- `fopen` 실패 → `NULL`. `stat` 실패 → `-1`(그때 struct는 안 채워짐 → 쓰레기값, 읽으면 UB).
- **성공/실패를 반환하는 함수는 항상 확인 후 사용.** C 시스템 프로그래밍의 기본 습관.

---

## 6. 반복 실수 체크리스트 (다음 프로젝트에서 스스로 점검)

- [ ] 쓰는 함수마다 필요한 **헤더** 다 include했나? (`man`의 SYNOPSIS 확인)
- [ ] `fopen`/`stat`/`malloc` 등 **반환값 확인**했나?
- [ ] 포인터를 여러 곳에 저장할 때 **aliasing**(같은 메모리 공유) 문제 없나? 복사 필요 없나?
- [ ] 모든 할당에 **짝이 되는 free**가 있나? 순서는? (내부 포인터 먼저)
- [ ] **use-after-free / 이중 free** 없나?
- [ ] `=` vs `==`, 연산자 **우선순위** 괄호 확인했나?
- [ ] `-Wall` 경고 0개인가? `valgrind` 누수·에러 0인가?

---

## 7. 다음 단계

- (선택) 워밍업 프로젝트 더: `wcat`, `wgrep`
- 본 커리큘럼: OSTEP **Ch 4 The Abstraction: Process** (CPU 가상화 시작)
- 전체 로드맵은 `OSTEP_xv6_로드맵.md` 참고.
