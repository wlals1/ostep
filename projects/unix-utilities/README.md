# Unix Utilities — OSTEP Initial Project 정리

UNIX 명령어의 간단 버전 4개를 직접 구현하는 워밍업 프로젝트.
각각 단일 `.c`, **`-Wall -Werror`**로 컴파일. 이 문서는 프로젝트 정리 + 만들면서 배운 개념 복습 노트다.

| 프로그램 | 원본 | 핵심 스킬 |
|----------|------|-----------|
| **wcat** | cat | 여러 파일 순차 I/O |
| **wgrep** | grep | 문자열 검색(strstr), 함수 분리, stdin 폴백 |
| **wzip** | 압축 | 바이너리 출력(fwrite), RLE 알고리즘 |
| **wunzip** | 해제 | 바이너리 입력(fread), 왕복 검증 |

---

## wcat

파일 내용을 그대로 출력. reverse와 달리 **순서 그대로**라 저장 불필요 → 읽는 즉시 출력.

```bash
gcc -o wcat wcat.c -Wall -Werror
./wcat file1 file2 ...     # 각 파일 순서대로 출력
```

- argv[1]부터 전부 파일 → `for (i=1; i<argc; i++)` 순회
- **파일 0개(argc==1)면 루프가 안 돌아 자연스럽게 exit 0** (별도 처리 불필요)
- fopen 실패 → 정확히 `wcat: cannot open file` (⚠️ 파일 이름 없음), exit 1
- 파일마다 `fopen` → 읽기 → **`fclose`** (안 닫으면 파일 디스크립터 누수)

## wgrep

검색어가 든 줄만 출력.

```bash
./wgrep searchterm file1 file2 ...   # 파일들에서 검색
./wgrep searchterm                   # 파일 없으면 stdin에서 검색
```

- **인자 구조가 wcat과 다름**: `argv[1]` = 검색어, `argv[2]`부터 = 파일들
- `argc==1` → `wgrep: searchterm [file ...]`, exit 1
- `argc==2` → 파일 없음 → **stdin**에서 읽기 (stdin도 그냥 `FILE *`)
- `argc>=3` → 파일들 순회
- 매칭: `strstr(line, term) != NULL` (부분 문자열, **대소문자 구분**)
- **공통 로직을 함수로 분리**: `search(FILE *fp, char *term)` 하나로 stdin·파일 모두 처리 (중복 제거)

## wzip

Run-Length Encoding(RLE) 압축. 같은 문자 n개 연속 → `(n, 문자)`.

```bash
./wzip file.txt > file.z     # 출력은 stdout, 보통 리다이렉트
```

**바이너리 포맷:** 각 run = **5바이트** = `[4바이트 정수 n (바이너리)] + [문자 1개]`.
- `10a` = 문자 "10"이 아니라 **4바이트 정수 10** + 'a'. `fwrite`로 씀 (`printf("%d")` 쓰면 ASCII로 나가서 틀림).

**RLE 알고리즘 + 세 가지 경계:**
```
prev, count, started(플래그)  ← 파일 루프 밖 선언 (경계 3)
for 각 파일:
    while (cur = fgetc(fp)) != EOF:
        if !started: prev=cur; count=1; started=1     # 경계 2: 첫 문자
        elif cur==prev: count++
        else: emit(count,prev); prev=cur; count=1     # 문자 바뀜 → 출력
if started: emit(count, prev)   # 경계 1: 마지막 run flush
```
- **경계 1 (마지막 run)**: 출력은 "문자 바뀔 때"만 → 마지막 run은 EOF 후 따로 flush. 안 하면 마지막 그룹 누락.
- **경계 2 (첫 문자)**: 비교할 prev가 아직 없음 → `started` 플래그로 "첫 문자면 초기화" 분기.
- **경계 3 (파일 경계)**: 여러 파일 = 하나의 스트림. `...aaa`+`aa...` → `a` 5개 한 run. → **prev/count/started를 파일 루프 밖에** 두고, 파일 사이에서 리셋 금지.

- 파일 0개 → `wzip: file1 [file2 ...]`, exit 1

## wunzip

wzip의 역. 압축 파일 읽어 원본 복원(stdout).

```bash
./wunzip file.z              # 4바이트 정수 + 문자 반복 읽어 복원
```
```
for 각 파일:
    while fread(&count, sizeof(int), 1, fp) == 1:   # 0이면 EOF
        fread(&c, sizeof(char), 1, fp)
        c를 count번 출력
```
- 파일 0개 → `wunzip: file1 [file2 ...]`, exit 1
- 바이너리 파일이라 `"rb"`가 정석 (리눅스에선 `"r"`과 동일)

---

## 이번에 배운/헷갈린 개념 정리

### strstr — 부분 문자열 검색
- `char *strstr(const char *haystack, const char *needle)` — 찾으면 위치 포인터, 없으면 **NULL**. `<string.h>`.
- 부분 일치(substring). `"barfood"`도 `"foo"` 검색에 걸림. 대소문자 구분.

### 함수로 공통 로직 분리
- `search(FILE *fp, char *term)`처럼 **FILE\* 를 받는 함수**로 빼면, stdin이든 파일이든 같은 코드로 처리. `main` 위에 정의.

### fgetc는 `int`를 반환한다 (중요 함정)
- `fgetc`는 문자(0~255) **또는 EOF(-1)**를 반환 → 반드시 **`int`로 받기**.
- `char`로 받으면: 바이트 `0xFF`(255)가 signed char에서 **-1로 truncate** → 진짜 데이터를 EOF로 오인해 파일이 잘림. 비이식적.
- 규칙: `int c; while ((c = fgetc(fp)) != EOF)`.

### 바이너리 I/O — fwrite / fread
- `fwrite(&count, sizeof(int), 1, stdout)` = 정수의 raw 4바이트를 그대로 씀 (ASCII 변환 X).
- `fread(&count, sizeof(int), 1, fp)` = 4바이트를 정수로 읽음. **반환값** = 읽은 항목 수 → 0이면 EOF(루프 종료 조건).
- 텍스트 출력(`printf("%d")`)과 바이너리 출력(`fwrite`)은 완전히 다름. 포맷이 바이너리면 반드시 fwrite/fread.

### 엔디언(endianness)
- 정수 5를 4바이트로 쓰면 x86(리틀엔디언)에선 `05 00 00 00` (낮은 바이트 먼저). `xxd`로 확인.
- wzip이 쓴 순서로 wunzip이 읽으면 같은 기계에선 문제없음.

### `started` 플래그의 두 역할
- (1) "첫 문자인가?" 구분 → 비교 대신 초기화. (2) "입력이 비었나?" → 최종 flush를 `if(started)`로 가드.

### fopen/fclose 짝 (파일 디스크립터 누수)
- 파일 열면 반드시 닫기. 안 닫으면 **fd 누수**(메모리 누수와 별개). malloc/free 짝과 같은 습관.

### "r" vs "rb"
- 바이너리 파일은 `"rb"`가 정석. **리눅스/유닉스에선 동일**, 윈도우에선 `\r\n` 변환 때문에 달라짐.

### 정확한 에러 메시지 & 종료 코드
- 자동 테스트는 **문자열을 정확히** 비교 + 종료 코드 확인. 오타(`opne`, 여분 공백) 하나로 실패. `-Werror`라 컴파일 경고도 에러.

---

## 빌드 · 테스트 요약

```bash
for u in wcat wgrep wzip wunzip; do gcc -o $u $u.c -Wall -Werror; done

# wcat
./wcat f1 f2 ; ./wcat 없는파일 ; echo $?   # 순차 출력 / cannot open, 1
# wgrep
./wgrep foo file.txt ; echo "x foo y" | ./wgrep foo   # 매칭 / stdin
# wzip (바이너리 → xxd로 확인)
printf "aaaaabbbc" > t.txt ; ./wzip t.txt | xxd       # 05..61 03..62 01..63
# 왕복 (핵심 검증)
./wzip t.txt > t.z ; diff <(./wunzip t.z) t.txt && echo "왕복 OK"
```

---

> 전체 로드맵: [ROADMAP.md](../../ROADMAP.md)
