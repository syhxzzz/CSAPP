#include "cachelab.h"
#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
typedef struct _ReadLine {
  u_int8_t type;
  u_int64_t address;
  u_int32_t size;
} ReadLine;

typedef struct _Line {
  BOOL flag;
  int blockStart;
  long tag;
  struct _Line *nextLine;
} Line;

typedef struct _Set {
  Line *linePointer;
  struct _Set *Next;
} Set;
int numSetBit, numLineBit, numBlockBit;
int numSet, numLine, numBlock;
int h, v;
int hit = 0, miss = 0, eviction = 0;

char t[30]; // 要读的文件名
int operation(long address, int size, Set *setPointer);
Set *initAll(int numSet, int numLine, int numBlock);
Line *initLine(int numLine);
void freeAll(Set *set);
void freeLine(Line *line);
void addLastLine(Line *LineHead, int lineOffset, int blockOffset);
void changeLine(Line *LineHead, int lineOffset, int blockOffset,
                Line *lineToEvi);
void printUsage();
void update(FILE *fp, ReadLine *readline, Set *set);
void printTest(int test);
int main(int argc, char **argv) {
  int opt;
  while (-1 != (opt = (getopt(argc, argv, "hvs:E:b:t:")))) {
    switch (opt) {
    case 'h':
      h = 1;
      printUsage();
      break;
    case 'v':
      v = 1;
      printUsage();
      break;
    case 's':
      numSetBit = atoi(optarg);
      numSet = 1 << numSetBit;
      break;
    case 'E':
      numLine = atoi(optarg);
      break;
    case 'b':
      numBlockBit = atoi(optarg);
      numBlock = 1 << numBlockBit;
      break;
    case 't':
      strcpy(t, optarg);
      break;
    default:
      printUsage();
      break;
    }
  }
  if (numSetBit <= 0 || numLine <= 0 ||
      numBlockBit <= 0) // 如果选项参数不合格就退出
  {
    printf("Something strange happened\n");
    return -1;
  }
  Set *set = initAll(numSet, numLine, numBlock);
  FILE *fp = fopen(t, "rb");
  if (fp == NULL) {
    printf("Can not open this file!\n");
    exit(0);
  }
  ReadLine *readline = malloc(sizeof(ReadLine));
  update(fp, readline, set);
  freeAll(set);
  fclose(fp);
  printSummary(hit, miss, eviction);
  return 0;
}

Set *initAll(int numSet, int numLine, int numBlock) {
  Set *setPointer = malloc(sizeof(Set));
  Set *p = setPointer;
  for (int i = 0; i < numSet - 1; i++) {
    p->linePointer = initLine(numLine);
    p->Next = malloc(sizeof(Set));
    p = p->Next;
  }
  p->linePointer = initLine(numLine);
  p->Next = NULL;
  return setPointer;
}

Line *initLine(int numLine) {
  Line *linePointer = malloc(sizeof(Line));
  Line *p = linePointer;
  for (int i = 0; i < numLine - 1; i++) {
    p->flag = FALSE;
    p->blockStart = 0;
    p->tag = 0;
    p->nextLine = malloc(sizeof(Line));
    p = p->nextLine;
  }
  p->flag = FALSE;
  p->tag = 0;
  p->nextLine = NULL;
  p->blockStart = 0;
  return linePointer;
}

int operation(long address, int size, Set *setPointer) {
  int blockOffset = address & (numBlock - 1);
  int setOffset = (address & ((numSet - 1) << numBlockBit)) >> numBlockBit;
  int lineOffset = address >> (numBlockBit + numSetBit);
  Set *p = setPointer;
  assert(setOffset < numBlock);
  for (int i = 0; i < setOffset; i++) {
    p = p->Next;
  }
  Line *lineP = p->linePointer;
  for (int i = 0; i < numLine; i++) {
    if (lineP->flag == FALSE) {
      changeLine(p->linePointer, lineOffset, blockOffset, lineP);
      return 2; // 说明没有eviction
    } else if (lineOffset == lineP->tag && size + blockOffset < numBlock) {
      changeLine(p->linePointer, lineOffset, blockOffset, lineP);
      return 1;
    } else {
      lineP = lineP->nextLine;
    }
  }
  changeLine(p->linePointer, lineOffset, blockOffset, p->linePointer);
  return 0;
}

void changeLine(Line *LineHead, int lineOffset, int blockOffset,
                Line *lineToEvi) {
  Line *lineToBeEvi = LineHead; // 替换掉lineToEvi，再最后塞入新的
  while (lineToBeEvi != lineToEvi) {
    lineToBeEvi = lineToBeEvi->nextLine;
  }
  Line *lineBeforeTheEvi = LineHead;
  while (LineHead != lineToBeEvi && lineBeforeTheEvi->nextLine != lineToBeEvi) {
    lineBeforeTheEvi = lineBeforeTheEvi->nextLine;
  }
  if (LineHead == lineToBeEvi) {
    LineHead = lineToBeEvi->nextLine;
  } else { // 此时待删除的Line在Lines中间位置
    lineBeforeTheEvi->nextLine = lineToBeEvi->nextLine;
  }
  free(lineToBeEvi);
  addLastLine(LineHead, lineOffset, blockOffset);
}

void addLastLine(Line *LineHead, int lineOffset, int blockOffset) {
  if (LineHead) {
    Line *ptr = LineHead;
    while (ptr->nextLine) {
      ptr = ptr->nextLine;
    }
    ptr->nextLine = malloc(sizeof(Line));
    ptr->tag = lineOffset;
    ptr->flag = TRUE;
    ptr->nextLine = NULL;
    ptr->blockStart = blockOffset;
  } else {
    LineHead = malloc(sizeof(Line));
    LineHead->tag = lineOffset;
    LineHead->flag = TRUE;
    LineHead->nextLine = NULL;
    LineHead->blockStart = blockOffset;
  }
}

void freeAll(Set *set) {
  Set *p = set;
  while (p->Next) {
    Set *setToFree = p;
    p = p->Next;
    freeLine(setToFree->linePointer);
    free(setToFree);
  }
  freeLine(p->linePointer);
  free(p);
}

void freeLine(Line *line) {
  Line *p = line;
  while (p->nextLine) {
    Line *lineToFree = p;
    p = p->nextLine;
    free(lineToFree);
  }
  free(p);
}

void printUsage() {
  printf("Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\n"
         "Options:\n"
         "  -h         Print this help message.\n"
         "  -v         Optional verbose flag.\n"
         "  -s <num>   Number of set index bits.\n"
         "  -E <num>   Number of lines per set.\n"
         "  -b <num>   Number of block offset bits.\n"
         "  -t <file>  Trace file.\n\n"
         "Examples:\n"
         "  linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n"
         "  linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

void update(FILE *fp, ReadLine *readline, Set *set) {
  while (fscanf(fp, " %c %lx,%d\n", &readline->type, &readline->address,
                &readline->size) == 3) {
    int test;
    printf("%c %lx,%d", readline->type, readline->address, readline->size);
    switch (readline->type) {
    case 'M':
      test = operation(readline->address, readline->size, set);
      printTest(test);
    default:
      test = operation(readline->address, readline->size, set);
      printTest(test);
    }
    printf("\n");
  }
}

void printTest(int test) {
  if (test == 1) {
    printf(" hit "); // normal hit
    hit++;
  } else if (test == 2) { // Miss without Eviction
    printf(" Miss ");
    miss++;
  } else { // Miss and eviction
    printf(" Eviction ");
    miss++;
    eviction++;
  }
}