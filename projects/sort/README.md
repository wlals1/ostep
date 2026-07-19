# ssort — OSTEP Initial Project 정리

파일(또는 표준입력)의 줄들을 **사전순으로 정렬해 출력**하는 간단한 sort. `qsort` 워밍업용.

## 사용법

| 실행 | 입력 | 출력 |
|------|------|------|
| `./ssort` | stdin | stdout (정렬) |
| `./ssort f1 [f2 ...]` | 파일들 (이어붙여서) | stdout (정렬) |

에러: 파일 못 열면 `ssort: cannot open file`, 할당 실패면 `malloc failed` — 모두 stderr + exit 1.

## 핵심 설계

- **동적 배열 + 배가(doubling)**: `char **lines`를 `realloc`으로 16 → 32 → ... 두 배씩 성장. `realloc` 실패 대비해 임시 포인터(`tmp`)로 받고 검사 후 대입.
- **`getline` + `strdup`**: getline 버퍼는 재사용되므로 줄마다 복사해 소유 (reverse에서 배운 aliasing 패턴 그대로).
- **`qsort` + 비교 함수(함수 포인터)**:
  ```c
  int compare(const void *a, const void *b) {
      char *sa = *(char **)a;   // 원소가 char*라 인자는 char** → 역참조
      char *sb = *(char **)b;
      return strcmp(sa, sb);
  }
  qsort(lines, count, sizeof(char *), compare);
  ```
  - qsort는 **원소의 주소**(`void*`)를 비교 함수에 넘김 → 배열 원소가 `char*`면 인자의 실체는 `char**`. 캐스팅 후 한 번 역참조해야 문자열이 나옴 (단골 함정).
- **해제**: 출력하며 `free(lines[i])` → 마지막에 `free(lines)`. 문자열과 포인터 배열은 별도 할당이라 각각 free.

## 빌드 · 테스트

```bash
gcc -o ssort ssort.c -Wall -Werror
printf "banana\napple\ncherry\n" | ./ssort     # apple banana cherry
./ssort input.txt
diff <(./ssort input.txt) <(sort input.txt)    # 시스템 sort와 대조
valgrind ./ssort input.txt                     # 누수·에러 0 확인
```

## 잡았던 버그 (use-after-free)

- 원래는 파일 루프 **안**에서 `free(line)`을 호출하면서 `line = NULL; len = 0;` 리셋을 안 함 → 파일 2개 이상이면 다음 파일의 `getline`이 해제된 버퍼를 재사용 (use-after-free). 파일 1개·stdin에선 증상이 없어 숨어 있었음.
- 수정: getline 버퍼는 파일이 바뀌어도 재사용해도 되므로 `free(line)`을 루프 밖으로 빼 **마지막에 한 번만** 호출. (free 후 포인터를 계속 쓸 거라면 반드시 NULL/0 리셋 — "이 할당 누가 소유/해제?" 규칙의 연장)
- 검증: 파일 3개 입력을 시스템 `sort`와 diff 대조 + valgrind 누수·에러 0.

---

> 전체 로드맵: [ROADMAP.md](../../ROADMAP.md)
