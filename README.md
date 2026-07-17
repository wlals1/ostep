# OSTEP 실습 코드

[OSTEP (Operating Systems: Three Easy Pieces)](https://pages.cs.wisc.edu/~remzi/OSTEP/)를 공부하며 작성한
**실습 코드 · 워밍업 프로젝트 · 챕터 정리 노트** 모음.
개념을 코드로 확인하고, 최종적으로 xv6 커널 분석까지 가는 것이 목표.

> 학습 계획·커리큘럼은 [`OSTEP_xv6_로드맵.md`](./OSTEP_xv6_ROADMAP.md) 참고.
> 이 README는 **저장소를 어떻게 굴릴지(구조·규칙·진행)** 위주.

---

## 저장소 구조

```
mine/
├── 02-Introduction-To-Operating-System/   # Ch2 예제 (mem, cpu, threads, io)
├── 04-process/                            # Ch4 정리 (개념 챕터, 코드 없음)
├── 05-process-API/                        # Ch5 fork/exec/wait (p1~p4)
│   └── homework/                          #   Ch5 숙제 (q1~q8)
├── projects/                              # 워밍업 프로젝트 (ostep-projects)
│   ├── reverse/                           #   줄 역순 출력
│   ├── unix-utilities/                    #   wcat, wgrep, wzip, wunzip
│   └── sort/                              #   ssort (qsort)
├── OSTEP_xv6_로드맵.md                     # 학습 계획
└── README.md
```

- **챕터 폴더**: 책 예제 코드 + 그 챕터 정리 `README.md`.
- **projects/**: 각 프로젝트별 폴더 + 스펙/배운 것 정리 `README.md`.

---

## 유지하는 규칙 (repo hygiene)

- **컴파일**: `gcc -o <name> <name>.c -Wall -Werror` — 경고를 에러로 취급, 하나도 안 남기기.
- **메모리**: 동적 할당 쓰는 코드는 `valgrind`로 **누수·에러 0** 확인.
- **에러 처리**: 시스템콜 반환값 항상 체크(`fopen`/`malloc`/`stat`/`open`...). 스펙 있으면 **에러 문자열 정확히**(자동 테스트가 문자열 비교).
- **정리 노트**: 각 챕터/프로젝트 폴더에 `README.md`로 **배운 것·헷갈렸던 것**을 남긴다 (누적 복습용).
- **커밋**: 소스만 올린다. `.gitignore`는 화이트리스트 방식으로 `.c/.h/.md/Makefile`만 추적, 컴파일 바이너리·테스트 산출물 제외.
  ```gitignore
  *
  !*/
  !*.c
  !*.h
  !*.md
  !Makefile
  !.gitignore
  ```

---

## 학습 방식 (keep)

- **예측 → 검증**: 코드를 돌리기 전에 결과를 예측하고, 실행으로 확인 (특히 시뮬레이션 숙제의 `-c` 플래그).
- **깊이 우선**: 진도보다 멘탈 모델. 막히면 그 자리에서 복습, 다음으로 안 넘어감.
- **왜를 우선**: 암기보다 "왜 이렇게 되는가". 그럴듯하지만 틀린 답을 스스로 잡아낼 수준까지.
- 반복 실수 패턴은 로드맵에 **누적 기록**해 다음에 미리 점검.

---

## 진행 현황

- [x] Ch 2 Introduction — mem, cpu, threads, io
- [x] 워밍업 프로젝트 — reverse, unix-utilities, sort
- [x] Ch 4 The Abstraction: Process (+ process-run.py 숙제)
- [x] Ch 5 Process API — fork/exec/wait (+ 숙제 q1~q8)
- [ ] Ch 6 Limited Direct Execution ← 다음

---

## 빌드 / 실행 예

```bash
# 프로젝트 예: reverse
cd projects/reverse
gcc -o reverse reverse.c -Wall -Werror
./reverse input.txt
valgrind ./reverse input.txt        # 누수·에러 0 확인

# 챕터 예제 예: Ch5 fork
cd 05-process-API
gcc -o p1 p1.c -Wall && ./p1
```

---

## 참고 링크

- 교재: [OSTEP](https://pages.cs.wisc.edu/~remzi/OSTEP/) (무료 PDF)
- 숙제: [ostep-homework](https://github.com/remzi-arpacidusseau/ostep-homework)
- 프로젝트: [ostep-projects](https://github.com/remzi-arpacidusseau/ostep-projects)