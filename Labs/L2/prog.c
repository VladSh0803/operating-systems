/*
Oświadczam, że niniejsza praca stanowiąca podstawę do uznania osiągnięcia efektów
uczenia się z przedmiotu SOP1 została wykonana przeze mnie samodzielnie.
Vladyslav Shestakov
308904
*/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#define ERR(source) (fprintf(stderr, "%s:%d\n",__FILE__,__LINE__),\
                     perror(source), kill(0,SIGKILL),\
		     		         exit(EXIT_FAILURE))

volatile sig_atomic_t sig_count = 0;

void usage(char *name){
	fprintf(stderr, "USAGE: %s 2<n<10\n", name);
	exit(EXIT_FAILURE);
}

void sethandler( void (*f)(int), int sigNo) {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if (-1 == sigaction(sigNo, &act, NULL)) ERR("sigaction");
}

void sigchl_handler(int sig) {
	switch (sig) {
    case SIGINT:
      exit(EXIT_SUCCESS);
    case SIGUSR1:
      sig_count++;
      break;
  }
}

void sigch_handler(int sig) {
  struct timespec t;
}

void child_work(int s, int k, int t) {
  sethandler(sigch_handler, SIGUSR1);
	printf("Process with PID %d\n", getpid());
  wait(NULL);
  exit(EXIT_SUCCESS);
}

void last_child_work(int t) {
  sethandler(sigchl_handler, SIGINT);
  sethandler(sigchl_handler, SIGUSR1);
  printf("Process with PID %d\n", getpid());
  sleep(1);
  exit(EXIT_SUCCESS);
}

void parent_work(int s, int k) {
  char word[20];
  struct timespec t = {0, k * 1000000};
  while (scanf("%s", word)) {
    if (strcmp(word, "exit")) {
      for (int i = 0; i < strlen(word); i++) {
        kill(s, SIGUSR1);
        nanosleep(&k, NULL);
      }
    }
    else {
      kill(0, SIGINT);
      break;
    }
  }
}

void create_line(int n, int k, int t) {
	pid_t s;
  int i;
	for (i = 0; i < n; i++) {
    if ((s = fork()) < 0) ERR("Fork:");
    if (s) break;
    if (i == n - 1) last_child_work(t);
  }
  if (i == 0) parent_work(s, k);
  child_work(s, k, t);
}

int main(int argc, char** argv) {
	if (argc != 4)  usage(argv[0]);
  int n = atoi(argv[1]), k = atoi(argv[2]), t = atoi(argv[3]);
  if (n < 2 || n > 10 || k < 1 || k > 1000) usage(argv[0]);
	create_line(n, k, t);
  wait(NULL);
	return EXIT_SUCCESS;
}