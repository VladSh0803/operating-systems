/*Oświadczam, że niniejsza praca stanowiąca podstawę do uznania osiągnięcia efektów
uczenia się z przedmiotu SOP1 została wykonana przeze mnie samodzielnie.
Vladyslav Shestakov
308904
*/
#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <ftw.h>
#define MAXFD 20
#define MAX_PATH 101

#define ERR(source) (perror(source),\
  fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
  exit(EXIT_FAILURE))

int t;

int walk(const char *name, const struct stat *s, int type, struct FTW *f)
{
  if (type == t) {
    printf("%s\n", name);
  }
  return 0;
}

int main(int argc, char** argv) {
  char path[MAX_PATH];
  int c;
  while((c = getopt(argc, argv, "p:t::")) != -1) {
    switch (c) {
      case 'p':
        if (getcwd(path, MAX_PATH)==NULL) ERR("getcwd");
        if (chdir(optarg)) ERR("chdir");
        break;
      case 't':
        if (optarg[0] == 'd') t = FTW_D;
        else if (optarg[0] == 'r') t = FTW_F;
        else t = FTW_SL;
        break;
      default:
        fprintf(stderr, "Usage: %s [-p folder] [-t type]\n", argv[0]);
        return EXIT_FAILURE;
    }
  }
  if (argc > optind) { 
    fprintf(stderr, "Usage: %s [-p folder] [-t type]\n", argv[0]);
    return EXIT_FAILURE;
  }
  if (nftw(".", walk, MAXFD, FTW_PHYS) != 0) ERR("nftw");
  return EXIT_SUCCESS;
}