/*
------------------------------------------------------------------------
Oświadczam, że niniejsza praca stanowiąca podstawę do uznania
osiągnięcia efektów uczenia się z przedmiotu SOP została wykonana przeze
mnie samodzielnie.
Vladyslav Shestakov 308904
------------------------------------------------------------------------
*/
#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#define DEFAULT_N 10
#define DEFAULT_T 200
#define MAXLINE 4096
#define DEFAULT_THREADCOUNT 10
#define DEFAULT_SAMPLESIZE 100

typedef struct timespec timespec_t;
typedef unsigned int UINT;
typedef struct argsGen_t {
  pthread_t* tid;
  int idx;
  UINT* seed;
} argsGen_t;

#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))

void* nadzor(void* voidPtr);
void* gen(void* voidPtr);

void usage(char* name) {
  fprintf(stderr, "Usage: %s 0<n<21 99<t<5001", name);
  exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
  pthread_t* tid;
  pthread_t nadz_tid;
  if (argc != 3) usage(argv[0]);
  int n = atoi(argv[1]);
  int t = atoi(argv[2]);
  if (n < 1 || n > 20 || t < 100 || t > 5000) usage(argv[0]);
  tid = (pthread_t*)malloc(sizeof(pthread_t) * 2 * n);
  if (NULL == tid) ERR("malloc");
  for (int i = 0; i < 2 * n; i++) {
    tid = 0;
  }
  srand(time(NULL));
  UINT seed = (UINT)rand();
  if (pthread_create(&nadz_tid, NULL, nadzor, &t)) ERR("pthread_create");
  sigset_t oldMask, newMask;
  sigemptyset(&newMask);
  sigaddset(&newMask, SIGINT);
  if (sigprocmask(SIG_BLOCK, &newMask, &oldMask)) ERR("pthread_sigmask");
  int signo;
  argsGen_t* args;
  for (;;) {
    printf("D1");
    if (sigwait(&newMask, &signo)) ERR("sigwait");
    printf("D2");
    fflush(stdout);
    switch (signo) {
      case SIGINT:
        args = (argsGen_t*)malloc(sizeof(argsGen_t));
        int idx = 0;
        while (tid[idx] != 0) {
          idx = rand() % 2 * n;
        }
        args->idx = idx;
        args->tid = tid;
        args->seed = &seed;
        if (pthread_create(&tid[idx], NULL, gen, &args)) ERR("pthread_create");
        break;
      default:
        printf("undefined signal");
        break;
    }
  }
  if (pthread_join(tid[0], NULL)) ERR("pthread_join");
  return EXIT_SUCCESS;
}

void* nadzor(void* voidPtr) {
  int t = *((int*)voidPtr);
  time_t sec = (int)(t / 1000);
  t = t - (sec * 1000);
  timespec_t req = {0};
  req.tv_sec = sec;
  req.tv_nsec = t * 1000000L;
  while (1) {
    printf("Busy\n");
    if(nanosleep(&req,&req)) ERR("nanosleep");
    fflush(stdout);
  }
}

void* gen(void* voidPtr) {
  argsGen_t* args = (argsGen_t*)voidPtr;
  int s = rand_r(args->seed) % 901 + 100;
  time_t sec = (int)(s / 1000);
  s = s - (sec * 1000);
  timespec_t req = {0};
  req.tv_sec = sec;
  req.tv_nsec = s * 1000000L;
  while (1) {
    printf("My TID: %ld\n", args->tid[args->idx]);
    if(nanosleep(&req,&req)) ERR("nanosleep");
    fflush(stdout);
  }
  return args;
}