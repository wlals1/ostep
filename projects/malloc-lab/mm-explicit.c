/*
 * mm-implicit.c — Implicit free list allocator with boundary tags
 *
 *   블록 구조:  [헤더 4B][payload][푸터 4B]
 *               헤더/푸터 = size | alloc  (8정렬이라 하위 3비트가 남아 alloc
 * 비트로 겸용) 힙 구조:    [패딩][프롤로그 8/1][ ... 블록들 ... ][에필로그 0/1]
 *               프롤로그·에필로그는 sentinel — 경계 특수처리를 제거
 *
 *   배치 정책:  first fit
 *   병합:       즉시 병합 (coalesce), 4가지 경우
 *   분할:       남는 공간이 최소 블록(16B) 이상이면 split
 *   realloc:    ① 이미 충분 → 제자리
 *               ② 다음이 free이고 합치면 충분 → 흡수 (복사 없음)
 *               ③ 다음이 에필로그 → extend_heap 후 흡수 (복사 없음)
 *               ④ 그 외 → malloc + memcpy + free
 *
 *   점수: 75/100  (util 51 + thru 24, 5회 실행 중앙값)
 *
 *   트레이스별 결과:
 *     amptjp 99% / cccp 99% / cp-decl 99% / expr 100%   ← 실제 프로그램은 거의
 * 완벽 coalescing 66% (thru는 최고) random 92% / random2 92% binary
 * 55%/184Kops,  binary2 51%/54~114Kops        ← ★ 병목 realloc 100%,  realloc2
 * 87%                        ← 제자리 확장으로 27%/34%에서 개선
 *
 *   병목 분석:
 *     binary 패턴 = [작은][큰][작은][큰]... 을 할당하고 작은 것만 free
 *       → util: 작은 free 블록이 큰 할당 블록에 가로막혀 병합 불가 (외부
 * 단편화) → thru: find_fit이 매번 힙 전체를 순회. 할당된 블록까지 전부 훑음.
 * O(n²) → 다음 단계: explicit free list (free 블록만 연결) → 둘 다 개선 기대
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"
#include "mm.h"

team_t team = {
    /* Team name */
    "jimin",
    /* First member's full name */
    "jimin",
    /* First member's email address */
    "duawlals0211@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* ---------- 상수 ---------- */
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define WSIZE 4             /* 워드 = 헤더/푸터 크기 */
#define DSIZE 8             /* 더블워드 = 정렬 단위 = 헤더+푸터 */
#define CHUNKSIZE (1 << 12) /* 힙 확장 기본 단위 (4KB) */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* ---------- 헤더/푸터 조작 ---------- */
#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* ---------- 블록 포인터(bp) 기준 주소 계산 ---------- */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
#define GET_PREV_FREE(bp) (*(void **)(bp))
#define GET_NEXT_FREE(bp) (*(void **)((char *)(bp) + WSIZE))

#ifdef DEBUG
#define CHECK() assert(mm_check())
#else
#define CHECK()
#endif

/* ---------- 전역/선언 ---------- */
static char *heap_listp; /* 프롤로그 블록의 bp */
static char *free_listp = NULL;

static int mm_check(void);
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

static void insert_free(void *bp) {
  GET_PREV_FREE(bp) = NULL;
  GET_NEXT_FREE(bp) = free_listp;
  if (free_listp != NULL)
    GET_PREV_FREE(free_listp) = bp;
  free_listp = bp;
}

static void remove_free(void *bp) {
  if (GET_PREV_FREE(bp) == NULL) {
    free_listp = GET_NEXT_FREE(bp);
  } else {
    GET_NEXT_FREE(GET_PREV_FREE(bp)) = GET_NEXT_FREE(bp);
  }
  if (GET_NEXT_FREE(bp)) {
    GET_PREV_FREE(GET_NEXT_FREE(bp)) = GET_PREV_FREE(bp);
  }
}

static int mm_check(void) {
  char *bp;
  int heap_free_count = 0;
  int list_count = 0;

  /* 프롤로그 */
  if (GET_SIZE(HDRP(heap_listp)) != DSIZE || !GET_ALLOC(HDRP(heap_listp))) {
    printf("Error: bad prologue at %p\n", heap_listp);
    return 0;
  }

  for (bp = NEXT_BLKP(heap_listp); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    /* 범위 */
    if (bp < (char *)mem_heap_lo() || bp > (char *)mem_heap_hi()) {
      printf("Error: [heap walk] bp %p out of range\n", bp);
      return 0;
    }
    /* 정렬 */
    if ((size_t)bp % ALIGNMENT) {
      printf("Error: bp %p not aligned\n", bp);
      return 0;
    }
    /* 헤더 == 푸터 */
    if (GET(HDRP(bp)) != GET(FTRP(bp))) {
      printf("Error: header/footer mismatch at %p (hdr=%u ftr=%u)\n", bp,
             GET(HDRP(bp)), GET(FTRP(bp)));
      return 0;
    }
    /* 크기 */
    if (GET_SIZE(HDRP(bp)) < 2 * DSIZE || GET_SIZE(HDRP(bp)) % DSIZE) {
      printf("Error: bad size %u at %p\n", GET_SIZE(HDRP(bp)), bp);
      return 0;
    }
    /* 인접 free */
    if (!GET_ALLOC(HDRP(bp)) && !GET_ALLOC(HDRP(NEXT_BLKP(bp)))) {
      printf("Error: uncoalesced free blocks at %p and %p\n", bp,
             NEXT_BLKP(bp));
      return 0;
    }

    if (!GET_ALLOC(HDRP(bp)))
      heap_free_count++;
  }

  /* 에필로그 */
  if (GET_SIZE(HDRP(bp)) != 0 || !GET_ALLOC(HDRP(bp))) {
    printf("Error: bad epilogue at %p\n", bp);
    return 0;
  }

  for (bp = free_listp; bp != NULL; bp = GET_NEXT_FREE(bp)) {
    if (bp < (char *)mem_heap_lo() || bp > (char *)mem_heap_hi()) {
      printf("Error: [list walk] bp %p out of range\n", bp);
      return 0;
    }
    if (GET_ALLOC(bp) != 0) {
      printf("Error: free list node but allocbit is not 0\n");
      return 0;
    }
    if (GET_NEXT_FREE(bp) && GET_PREV_FREE(GET_NEXT_FREE(bp)) != bp) {
      printf("Error: next pointer's prev is not me\n");
      return 0;
    }
    list_count++;
    if (list_count > heap_free_count) {
      printf("Error: list count is more than heap free count\n");
      return 0;
    }
  }

  if (list_count != heap_free_count) {
    printf("Error: %d free blocks in heap but %d in list\n", heap_free_count,
           list_count);
    return 0;
  }

  return 1;
}

/*
 * coalesce - 인접한 free 블록과 병합. 합쳐진 블록의 bp를 반환.
 *            프롤로그/에필로그가 alloc=1이라 경계 검사가 필요 없다.
 */
static void *coalesce(void *bp) {
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

  if (prev_alloc && next_alloc) { /* ① */
    /* 뺄 것 없음 */
  } else if (prev_alloc && !next_alloc) { /* ② 뒤만 free */
    void *next = NEXT_BLKP(bp);
    remove_free(next);
    size += GET_SIZE(HDRP(next));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
  } else if (!prev_alloc && next_alloc) { /* ③ 앞만 free */
    void *prev = PREV_BLKP(bp);
    remove_free(prev);
    size += GET_SIZE(HDRP(prev));
    PUT(HDRP(prev), PACK(size, 0));
    PUT(FTRP(prev), PACK(size, 0));
    bp = prev;
  } else { /* ④ 양쪽 free */
    void *prev = PREV_BLKP(bp);
    void *next = NEXT_BLKP(bp);
    remove_free(prev);
    remove_free(next);
    size += GET_SIZE(HDRP(prev)) + GET_SIZE(HDRP(next));
    PUT(HDRP(prev), PACK(size, 0));
    PUT(FTRP(prev), PACK(size, 0));
    bp = prev;
  }

  insert_free(bp); /* ★ 모든 경우 공통. 마지막에 한 번 */
  return bp;
}

/*
 * extend_heap - 힙을 words 워드만큼(짝수로 올림) 늘려 하나의 free 블록으로
 * 만든다. 옛 에필로그 자리가 그대로 새 블록의 헤더가 되는 게 핵심.
 */
static void *extend_heap(size_t words) {
  char *bp;
  size_t size;

  size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
  if (size < 2 * DSIZE)
    size = 2 * DSIZE;
  if ((long)(bp = mem_sbrk(size)) == -1)
    return NULL;

  PUT(HDRP(bp), PACK(size, 0)); /* 옛 에필로그를 새 블록 헤더로 덮어씀 */
  PUT(FTRP(bp), PACK(size, 0));         /* 푸터 */
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* 맨 끝에 새 에필로그 */

  return coalesce(bp);
}

/*
 * mm_init - 초기 힙 골격을 세운다.
 *           [패딩 0][프롤로그 헤더 8/1][프롤로그 푸터 8/1][에필로그 0/1]
 */
int mm_init(void) {
  free_listp = NULL;
  if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
    return -1;
  PUT(heap_listp + (0 * WSIZE), 0);              /* 패딩 (정렬용) */
  PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* 프롤로그 헤더 */
  PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* 프롤로그 푸터 */
  PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* 에필로그 헤더 */
  heap_listp += (2 * WSIZE); /* 프롤로그 bp를 가리키게 */

  if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    return -1;
  CHECK();
  return 0;
}

/*
 * find_fit - first fit. 프롤로그부터 순회하며 조건에 맞는 첫 free 블록을 반환.
 *            에필로그(크기 0)에서 종료.
 */
static void *find_fit(size_t asize) {
  char *bp;
  for (bp = free_listp; bp != NULL; bp = GET_NEXT_FREE(bp)) {
    if (GET_SIZE(HDRP(bp)) >= asize)
      return bp;
  }
  return NULL;
}

/*
 * place - bp에 asize만큼 배치. 남는 게 최소 블록 크기 이상이면 split.
 */
static void place(void *bp, size_t asize) {
  size_t csize = GET_SIZE(HDRP(bp));

  remove_free(bp);

  if ((csize - asize) >= (2 * DSIZE)) { /* 쪼갤 수 있음 */
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    bp = NEXT_BLKP(bp);
    PUT(HDRP(bp), PACK(csize - asize, 0));
    PUT(FTRP(bp), PACK(csize - asize, 0));
    insert_free(bp);
  } else { /* 통째로 할당 (남는 건 내부 단편화로 감수) */
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
  }
}

/*
 * mm_malloc - 요청 크기를 블록 크기로 조정하고, 맞는 free 블록을 찾아 배치.
 *             못 찾으면 힙을 확장한다.
 */
void *mm_malloc(size_t size) {
  size_t asize;
  size_t extendsize;
  char *bp;

  if (size == 0)
    return NULL;

  asize = ALIGN(size + DSIZE); /* 헤더+푸터를 붙이고 8 올림 */
  if (asize < 2 * DSIZE)
    asize = 2 * DSIZE; /* 최소 블록 크기 16 보장 */

  if ((bp = find_fit(asize)) != NULL) {
    place(bp, asize);
    CHECK();
    return bp;
  }

  /* 못 찾으면 확장. asize가 CHUNKSIZE보다 크면 그만큼 늘려야 함 */
  extendsize = MAX(asize, CHUNKSIZE);
  if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
    return NULL;
  place(bp, asize);
  CHECK();
  return bp;
}

/*
 * mm_free - alloc 비트를 0으로 하고 인접 free 블록과 병합.
 */
void mm_free(void *bp) {
  size_t size = GET_SIZE(HDRP(bp));

  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  coalesce(bp);
  CHECK();
}

/*
 * mm_realloc - 복사를 최대한 피한다.
 *   ① 현재 블록이 이미 충분 (축소 포함) → 그대로
 *   ② 다음이 free이고 합치면 충분        → 흡수 (복사 없음)
 *   ③ 다음이 에필로그(= 마지막 블록)     → 힙 확장 후 흡수 (복사 없음)
 *   ④ 그 외                              → malloc + memcpy + free
 *
 *   각 단계는 성공하면 return, 실패하면 아래로 흐른다 (else를 쓰지 않는 이유).
 */
void *mm_realloc(void *ptr, size_t size) {
  if (ptr == NULL) {
    CHECK();
    return mm_malloc(size);
  }
  if (size == 0) {
    mm_free(ptr);
    CHECK();
    return NULL;
  }

  size_t oldsize = GET_SIZE(HDRP(ptr));
  size_t asize = ALIGN(size + DSIZE);
  if (asize < 2 * DSIZE)
    asize = 2 * DSIZE;

  if (oldsize >= asize)
    return ptr;

  void *next = NEXT_BLKP(ptr);

  if (!GET_ALLOC(HDRP(next))) {
    size_t newsize = oldsize + GET_SIZE(HDRP(next));
    if (newsize >= asize) {
      remove_free(next);
      PUT(HDRP(ptr), PACK(newsize, 1));
      PUT(FTRP(ptr), PACK(newsize, 1));
      CHECK();
      return ptr;
    }
  }

  if (GET_SIZE(HDRP(next)) == 0) {
    size_t extendsize = asize - oldsize;
    if (extend_heap(extendsize / WSIZE) == NULL)
      return NULL;
    void *newblk = NEXT_BLKP(ptr);
    remove_free(newblk);
    size_t newsize = oldsize + GET_SIZE(HDRP(NEXT_BLKP(ptr)));
    PUT(HDRP(ptr), PACK(newsize, 1));
    PUT(FTRP(ptr), PACK(newsize, 1));
    CHECK();
    return ptr;
  }

  void *bp = mm_malloc(size);
  if (bp == NULL)
    return NULL;
  size_t copysize = oldsize - DSIZE;
  if (size < copysize)
    copysize = size;
  memcpy(bp, ptr, copysize);
  mm_free(ptr);
  CHECK();
  return bp;
}