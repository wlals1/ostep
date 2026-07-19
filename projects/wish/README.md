# processes-shell (wish) — 유닉스 셸 구현 정리

Ch5(Process API)에서 배운 **fork/exec/wait + 리다이렉션 + 파이프/dup2**를 조립해 실제 동작하는 셸을 구현.
배운 개념을 실제로 구현하는 프로젝트. `wish.c` 참고.

---

## 구현한 5대 기능

1. **기본 실행**: 명령 파싱 → `fork` + `execv` + `wait`.
2. **path 탐색**: `execvp`의 PATH 검색을 직접 구현 (`access`로 실행 파일 찾기).
3. **빌트인(bulit-in)**: `exit`, `cd`, `path` (셸이 직접 처리, fork 안 함).
4. **리다이렉션 `>`**: stdout+stderr를 파일로 (`dup2` 2번).
5. **병렬 `&`**: 여러 명령 동시 fork 후 몰아서 wait.

## 구조 핵심

### 모드 (argc로)
- `argc==1`: 인터랙티브(프롬프트 `wish> `, stdin).
- `argc==2`: 배치(프롬프트 없음, 파일에서 읽기).
- `argc>=3` 또는 배치파일 못 엶: `exit(1)`. (셸 종료는 이 두 경우 + 정상 `exit`뿐)

### 파싱 (층 구분 중요)
- **층1**: `argc`/`argv` = "wish를 어떻게 시작했나". 고정.
- **층2**: `getline`으로 읽은 한 줄 = 명령. 이걸 파싱(`&`→`>`→공백).
- 파싱 순서: **`&`로 병렬 분리 → 각 명령을 `>`로 리다이렉션 분리 → 공백으로 토큰화.**
- **`strsep` 주의**: 원본 포인터를 전진시킴 → `rest = line`으로 작업해 `line` 보존(다음 getline 위해). 토큰들은 line 버퍼를 가리켜 → 다음 getline 전에 써야 함(바로 execv하니 OK).

### 빌트인 (왜 셸이 직접?)
- 셸 **자신의 상태**를 바꾸는 명령이라 fork한 자식이 하면 안 됨(프로세스 격리):
  - `cd` → `chdir`(셸의 작업 디렉토리). 자식이 바꿔봤자 부모에 안 보임.
  - `exit` → 셸 종료. `path` → 셸 내부 검색 경로.
- `run_builtin(args, count)`: 빌트인이면 **처리하고 1 반환**(인자 에러여도 1 — "빌트인은 맞음"), 아니면 0.
- `main`: `if (run_builtin(...)==0) 외부실행`.
- **인자 개수 검사**: exit(인자 없음), cd(정확히 1개=count 2), path(0개 이상 덮어쓰기).
- **path는 strdup**: args는 line 버퍼 가리켜 무효화됨 → 여러 줄 유지하려면 복사.

### path 탐색 (execvp 직접 구현)
```c
for (path[k] in path):
    snprintf(fullpath, "%s/%s", path[k], cmd)   // "/bin" + "/" + "ls"
    if (access(fullpath, X_OK) == 0): found; break   // 실행 가능한 첫 걸 사용
execv(found, myargs)   // execvp 아님 — 전체 경로 직접 찾았으니 execv
```
- `access(경로, X_OK)`: 실행 가능한 파일 존재 확인(0=가능).
- path 비면(`path` 단독) 외부 프로그램 못 찾음.

### 리다이렉션 `>` (자식에서, execv 전)
```c
int fd = open(outfile, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);
dup2(fd, STDOUT_FILENO);   // stdout → 파일
dup2(fd, STDERR_FILENO);   // stderr → 같은 파일 (twist!)
close(fd);                  // 원본 fd 잉여 → 닫음 (fd 1,2가 파일 유지)
```
- **open(fd 반환) vs fopen(FILE* 반환)**: dup2가 fd로 동작하니 open.
- open은 fd 3에 열림 → dup2로 fd 1·2가 그 파일 가리키게.
- **open 1번 + dup2 2번**: stdout·stderr가 **같은 open file description 공유**(offset 공유) → 안 겹침. (open 2번이면 offset 따로라 덮어씀 — Ch5 개념.)
- `close(fd)` OK 이유: fd는 통로일 뿐, 파일은 참조(fd 1,2) 있으면 유지. 잉여 통로만 정리.
- 에러: `>` 여러 개, 파일 0개/2개+ → write_error.

### 병렬 `&` (핵심: fork 루프 안, wait 루프 밖)
```c
rest = line;
while ((cmd = strsep(&rest, "&")) != NULL) {
    int pid = exec_command(cmd);   // fork까지, wait 안 함
    if (pid > 0) child_count++;
}
for (i in child_count) wait(NULL);  // ★ 한 getline 후 모든 fork 이후 마지막에 모두 wait으로 자식 거둬들이기
```
- **fork하고 바로 wait하면 순차 실행(병렬 아님).** 다 fork한 뒤 wait 몰아야 동시 실행.
- `exec_command`는 자식 pid 반환(빌트인/에러/빈 명령이면 -1).

---

## 에러 처리 규칙

- 유일한 메시지: `"An error has occurred\n"` → `write(STDERR_FILENO, ...)`.
- **대부분 에러 = 출력 후 계속**(셸 안 죽음). 명령 하나 잘못돼도 다음 프롬프트로.
- **셸 종료(exit(1))는 딱 2경우**: 시작 인자 3개+, 배치파일 못 엶. (정상 `exit`은 exit(0).)
- 프로그램 자체 에러(잘못된 옵션 등)는 그 프로그램이 처리 — 셸 몫 아님.

---

## 헷갈린것

- **strsep**: 토큰 하나씩 반환(배열 아님), 구분자를 `\0`으로 → 토큰 자동 널종료. `&포인터`(전진), 빈 토큰(연속 공백) 걸러야.
- **`char *myargs[64]`** = char* 64개 배열(포인터들). 각 칸엔 line 버퍼 안 토큰의 **주소**. 배열 위치(스택) ≠ 토큰 위치(힙 line버퍼).
- **연산자 우선순위**: `(token = strsep(...)) != NULL` 괄호 (=보다 !=가 우선).
- **탭 완성/히스토리 없음**: bash는 readline 라이브러리로 제공. wish는 getline(날것) → 스펙 밖, 정상.
- **자잘한 미완성**(스펙 통과엔 무방): `> file`(명령 없음) 처리, 메모리 완전 free 등.

---

## 다음
- 로드맵상 **Phase 2 Memory Virtualization (Ch 13~)** — 2장 CR3/페이지테이블 정식 회수.
- 나중 xv6 랩(scheduling-xv6-lottery)에서 스케줄러 구현.
- 전체 로드맵: [ROADMAP.md](../../ROADMAP.md)