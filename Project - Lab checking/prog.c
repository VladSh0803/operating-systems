/*
Oswiadczam, ze niniejsza praca stanowiaca
podstawe do uznania osiagniecia efektow uczenia sie
z przedmitu SOP1 zostala wykonana przeze mnie samodzielnie.
Vladyslav Shestakov
308904
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>

// default arguments
#define SERVER "nowakj@ssh.mini.pw.edu.pl:/home2/samba/sobotkap/unix/*"
#define STATEMENT "Oswiadczam"
#define PATH "."
#define NAME "errors.log"

// sizes of char []
#define MAXSIZE 200 + 1
#define BUFSIZE 512

// file types
#define CFILE ".c"
#define MFILE1 "makefile"
#define MFILE2 "Makefile"
#define TGZ ".tar.gz"
#define TBZ2 ".tar.bz2"
#define TXZ ".tar.xz"
#define ZIP ".zip"

// file types
enum ftype
{
  IS_CFILE,
  IS_MKFILE,
  IS_TGZ,
  IS_TBZ2,
  IS_TXZ,
  IS_ZIP,
  IS_UDFILE
};

// command to unpack archives
#define UNTBZ2 "tar -xjf"
#define UNTGZ "tar -xzf"
#define UNTXZ "tar -xJf"
#define TTO "-C"
#define UNZIP "unzip"
#define ZTO "-d"

#define CURDIR "."
#define UPDIR ".."

// error messages
#define NO_STM "Brak oświadczenia"
#define UNDEF_FILE "Plik nie jest ani plikiem źródłowym ani plikiem makefile"
#define EMPTY_DIR "Pusty katalog"
#define NO_SF "Brak pliku źródłowego"
#define NO_MF "Brak makefile"
#define MANY_MFS "Więcej niż jeden makefile"
#define BUF_OVERFLOW "Nie udało się odczytać pliku (przepełnienie bufora)"

#define ERR(source) (perror(source),                                 \
                     fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), \
                     exit(EXIT_FAILURE))

typedef struct argsDownload
{
  pthread_t tid;
  pthread_t *check_tid;
  pthread_t *error_tid;
  char *server;
  sigset_t *pMask;
} argsDownload;

typedef struct argsCheck
{
  pthread_t tid;
  pthread_t *error_tid;
  char *Error;
  char *stm;
  int mflg;
  sigset_t *pMask;
  pthread_mutex_t *mxError;
} argsCheck;

typedef struct argsError
{
  pthread_t tid;
  pthread_t *check_tid;
  char *name;
  char *Error;
  sigset_t *pMask;
  pthread_mutex_t *mxError;
} argsError;

int ReadArguments(int argc, char *argv[], char *stm, char *server, char *path, char *name);
void make_threads(argsCheck *argsC, argsDownload *argsD, argsError *argsE);
void *download(void *argsD);
void unpack(char *name, char *cm, char *type, char *to);
void *error(void *argsE);
void *check(void *argsC);
void checkdir(char *name, argsCheck *args);
int checkfile(char *name, argsCheck *args);
int str_ends_with(char *s, char *suffix);
void checkc(char *name, argsCheck *args);
void gw_err(char *name, char *message, argsCheck *args);

void usage(char *pname)
{
  fprintf(stderr, "USAGE:%s [-m] [-o text] [-s server] [-d path] [-b name]\n", pname);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
  int mflg;
  char Error[PATH_MAX + MAXSIZE];
  char stm[MAXSIZE], server[PATH_MAX], pathcwd[PATH_MAX], path[PATH_MAX], name[MAXSIZE];
  pthread_mutex_t mxError = PTHREAD_MUTEX_INITIALIZER;
  sigset_t oldMask, newMask;
  argsError argsE;
  argsCheck argsC;
  argsDownload argsD;

  // read arguments from "argv"
  mflg = ReadArguments(argc, argv, stm, server, path, name);

  //go to path
  if (getcwd(pathcwd, PATH_MAX) == NULL)
    ERR("getcwd");
  if (chdir(path))
    ERR("chdir");

  // set sigmask for threads
  sigemptyset(&newMask);
  sigaddset(&newMask, SIGUSR1);
  sigaddset(&newMask, SIGUSR2);
  if (pthread_sigmask(SIG_BLOCK, &newMask, &oldMask))
    ERR("pthread_sigmask");

  // arguments for thread "error"
  argsE.pMask = &newMask;
  argsE.check_tid = &argsC.tid;
  argsE.name = name;
  argsE.Error = Error;
  argsE.mxError = &mxError;

  // arguments for thread "check"
  argsC.Error = Error;
  argsC.stm = stm;
  argsC.mxError = &mxError;
  argsC.error_tid = &argsE.tid;
  argsC.pMask = &newMask;
  argsC.mflg = mflg;

  // arguments for thread "download"
  argsD.check_tid = &argsC.tid;
  argsD.error_tid = &argsE.tid;
  argsD.pMask = &newMask;
  argsD.server = server;

  make_threads(&argsC, &argsD, &argsE);

  pthread_kill(argsD.tid, SIGUSR1);

  if (pthread_join(argsD.tid, NULL))
    ERR("pthread_join");

  if (pthread_join(argsE.tid, NULL))
    ERR("pthread_join");

  if (pthread_join(argsC.tid, NULL))
    ERR("pthread_join");

  // return to cwd
  if (chdir(pathcwd))
    ERR("chdir");

  return EXIT_SUCCESS;
}

int ReadArguments(int argc, char *argv[], char *stm, char *server, char *path, char *name)
{
  int c, mflg = 0, oflg = 0, sflg = 0, dflg = 0, bflg = 0;
  if (argc > 10)
    usage(argv[0]);
  while ((c = getopt(argc, argv, "mo:s:d:b:")) != -1)
    switch (c)
    {
    case 'm': // need to check statement
      mflg = 1;
      break;
    case 'o': // text of statement
      if (!oflg)
      {
        if (strlen(optarg) > MAXSIZE)
          ERR("Too much chars!");
        strcpy(stm, optarg);
        oflg++;
      }
      break;
    case 's': // server to download archives
      if (!sflg)
      {
        if (strlen(optarg) > PATH_MAX)
          ERR("Too much chars!");
        strcpy(server, optarg);
        sflg++;
      }
      break;
    case 'd': // directory to download and unpack
      if (!dflg)
      {
        if (strlen(optarg) > PATH_MAX)
          ERR("Too much chars!");
        strcpy(path, optarg);
        dflg++;
      }
      break;
    case 'b': // name for log file
      if (!bflg)
      {
        if (strlen(optarg) > MAXSIZE)
          ERR("Too much chars!");
        strcpy(name, optarg);
        bflg++;
      }
      break;
    default:
      usage(argv[0]);
    }
  if (!oflg)
    strcpy(stm, STATEMENT);
  if (!sflg)
    strcpy(server, SERVER);
  if (!dflg)
    strcpy(path, PATH);
  if (!bflg)
    strcpy(name, NAME);
  return mflg;
}

void make_threads(argsCheck *argsC, argsDownload *argsD, argsError *argsE)
{
  // thread error
  if (pthread_create(&argsE->tid, NULL, error, argsE))
    ERR("pthread_create");

  // thread check
  if (pthread_create(&argsC->tid, NULL, check, argsC))
    ERR("pthread_create");

  // thread download
  if (pthread_create(&argsD->tid, NULL, download, argsD))
    ERR("pthread_create");
}

void *download(void *argsD)
{
  argsDownload *args = (argsDownload *)argsD;
  char *command = (char *)malloc(strlen("scp") + (strlen(args->server) + strlen(CURDIR) + 3) * sizeof(char));
  if (command == NULL)
    ERR("malloc");
  DIR *dir;
  struct dirent *dp;
  struct stat filestat;
  int signo;

  // wait for starting download
  if (sigwait(args->pMask, &signo))
    ERR("sigwait");
  if (signo != SIGUSR1)
  {
    printf("Unexpected signal %d\n", signo);
    exit(EXIT_FAILURE);
  }

  // download files
  sprintf(command, "scp %s %s", args->server, CURDIR);
  if (system(command) != 0)
    ERR("system");
  free(command);

  if ((dir = opendir(CURDIR)) == NULL)
    ERR("opendir");
  do
  {
    errno = 0;
    if ((dp = readdir(dir)) != NULL)
    {
      if (lstat(dp->d_name, &filestat))
        ERR("lstat");
      if (S_ISREG(filestat.st_mode))
      {
        switch (checkfile(dp->d_name, NULL))
        {
        case IS_TBZ2:
          unpack(dp->d_name, UNTBZ2, TBZ2, TTO);
          break;
        case IS_TGZ:
          unpack(dp->d_name, UNTGZ, TGZ, TTO);
          break;
        case IS_TXZ:
          unpack(dp->d_name, UNTXZ, TXZ, TTO);
          break;
        case IS_ZIP:
          unpack(dp->d_name, UNZIP, ZIP, ZTO);
          break;
        }
      }
    }
  } while (dp != NULL);

  if (errno != 0)
    ERR("readdir");
  if (closedir(dir))
    ERR("closedir");

  // start logging errors
  pthread_kill(*args->error_tid, SIGUSR1);

  // start checking errors
  pthread_kill(*args->check_tid, SIGUSR1);
  return NULL;
}

// Unpack archive
void unpack(char *name, char *cm, char *type, char *to)
{
  struct stat filestat;
  char *folder = (char *)malloc((strlen(name) - strlen(type) + 1) * sizeof(char));
  char *command;
  if (folder == NULL)
    ERR("malloc");
  strncpy(folder, name, strlen(name) - strlen(type));
  folder[strlen(name) - strlen(type)] = '\0';
  command = (char *)malloc((strlen(cm) + strlen(name) + strlen(to) + strlen(folder) + 4) * sizeof(char));
  if (command == NULL)
    ERR("malloc");
  if (lstat(folder, &filestat) == -1)
    mkdir(folder, 0777);
  sprintf(command, "%s %s %s %s", cm, name, to, folder);
  if (system(command))
    ERR("system");
  free(folder);
  free(command);
}

void *error(void *argsE)
{
  argsError *args = (argsError *)argsE;
  int signo, out, qflg = 0;

  if (sigwait(args->pMask, &signo))
    ERR("sigwait");
  if (signo != SIGUSR1)
  {
    printf("Unexpected signal %d\n", signo);
    exit(EXIT_FAILURE);
  }

  if ((out = open(args->name, O_WRONLY | O_CREAT | O_APPEND, 0777)) < 0)
    ERR("open");
  if (write(out, "------------------------------------------------\n", 49) <= 0)
    ERR("write");
  while (1)
  {
    // send "ready to write" to thread "check"
    pthread_kill(*args->check_tid, SIGUSR1);

    // wait for signal from check
    if (sigwait(args->pMask, &signo))
      ERR("sigwait");
    switch (signo)
    {
    case SIGUSR1:
      // write to log
      pthread_mutex_lock(args->mxError);
      if (write(out, args->Error, strlen(args->Error)) <= 0)
        ERR("write");
      pthread_mutex_unlock(args->mxError);
      break;
    case SIGUSR2:
      qflg++;
      break;
    default:
      printf("Unexpected signal %d\n", signo);
      exit(EXIT_FAILURE);
    }
    if (qflg)
      break;
  }
  if (close(out))
    ERR("close");
  return NULL;
}

void *check(void *argsC)
{
  argsCheck *args = (argsCheck *)argsC;
  DIR *dir;
  struct dirent *dp;
  struct stat filestat;
  int signo;

  if (sigwait(args->pMask, &signo))
    ERR("sigwait");
  if (signo != SIGUSR1)
  {
    printf("Unexpected signal %d\n", signo);
    exit(EXIT_FAILURE);
  }

  // go throw directory
  if ((dir = opendir(CURDIR)) == NULL)
    ERR("opendir");
  do
  {
    errno = 0;
    if ((dp = readdir(dir)) != NULL)
    {
      if (lstat(dp->d_name, &filestat))
      {
        ERR("lstat");
      }

      // if directory - check it
      if (S_ISDIR(filestat.st_mode) && strcmp(dp->d_name, CURDIR) && strcmp(dp->d_name, UPDIR))
      {
        checkdir(dp->d_name, args);
      }
    }
  } while (dp != NULL);

  if (errno != 0)
    ERR("readdir");
  if (closedir(dir))
    ERR("closedir");

  pthread_kill(*args->error_tid, SIGUSR2);

  return NULL;
}

void checkdir(char *name, argsCheck *args)
{
  DIR *dir;
  struct dirent *dp;
  struct stat filestat;

  // sc - amount of *.c, mf - -||- makefiles, ned - is not empty directory
  int sc = 0, mf = 0, ned = 0;

  // change working directory to "name"
  if (chdir(name))
    ERR("chdir");

  // go throw directory
  if ((dir = opendir(CURDIR)) == NULL)
    ERR("opendir");
  do
  {
    errno = 0;
    if ((dp = readdir(dir)) != NULL)
    {
      if (lstat(dp->d_name, &filestat))
        ERR("lstat");
      if (S_ISREG(filestat.st_mode))
      { // if regular file - check it
        switch (checkfile(dp->d_name, args))
        {
        case IS_CFILE:
          checkc(dp->d_name, args);
          ned++;
          sc++;
          break;
        case IS_MKFILE:
          ned++;
          mf++;
          break;
        case IS_UDFILE:
          gw_err(dp->d_name, UNDEF_FILE, args);
          ned++;
          break;
        case IS_TBZ2:
          gw_err(dp->d_name, UNDEF_FILE, args);
          ned++;
          break;
        case IS_TGZ:
          gw_err(dp->d_name, UNDEF_FILE, args);
          ned++;
          break;
        case IS_TXZ:
          gw_err(dp->d_name, UNDEF_FILE, args);
          ned++;
          break;
        case IS_ZIP:
          gw_err(dp->d_name, UNDEF_FILE, args);
          ned++;
          break;
        default:
          printf("Unexpected result of \"checkfile\"\n");
          exit(EXIT_FAILURE);
        }
      }
    }

  } while (dp != NULL);

  if (errno != 0)
    ERR("readdir");
  if (closedir(dir))
    ERR("closedir");

  if (ned == 0)
  {
    gw_err("", EMPTY_DIR, args);
    return;
  }
  if (sc == 0)
    gw_err("", NO_SF, args);
  if (args->mflg && mf == 0)
    gw_err("", NO_MF, args);
  if (args->mflg && mf > 1)
    gw_err("", MANY_MFS, args);

  // one level up
  if (chdir(UPDIR))
    ERR("chdir");
}

// Check file type
int checkfile(char *name, argsCheck *args)
{
  if (args && str_ends_with(name, CFILE))
  {
    return IS_CFILE;
  }
  if (args && (strcmp(name, MFILE1) == 0 || strcmp(name, MFILE2) == 0))
    return IS_MKFILE;
  if (str_ends_with(name, TBZ2))
    return IS_TBZ2;
  if (str_ends_with(name, TGZ))
    return IS_TGZ;
  if (str_ends_with(name, TXZ))
    return IS_TXZ;
  if (str_ends_with(name, ZIP))
    return IS_ZIP;
  return IS_UDFILE;
}

// Check if string "s" ends with "suffix"
int str_ends_with(char *s, char *suffix)
{
  size_t slen = strlen(s);
  size_t suffix_len = strlen(suffix);

  return suffix_len <= slen && !strcmp(s + slen - suffix_len, suffix);
}

// Check if file .c contains statement
void checkc(char *name, argsCheck *args)
{
  char buf[BUFSIZE], *token;
  int in, count, step, stmflg = 0, start = 0;
  if ((in = open(name, O_RDONLY)) < 0)
    ERR("open");

  // checking statement
  while ((count = pread(in, buf, BUFSIZE - 1, start)) > 0) // count == 0 - EOF
  {
    buf[count] = '\0';
    step = 0;
    token = strchr(buf, '\n');
    if (!token)
    {
      char *tmp = (char *)malloc(sizeof(char));
      if (tmp == NULL)
        ERR("malloc");
      count = pread(in, tmp, 1, start + strlen(buf));
      if (count == 1)
      {
        gw_err(name, BUF_OVERFLOW, args);
      }
      else if (count == 0)
      {
        if (strstr(buf, args->stm))
          stmflg++;
      }
      free(tmp);
      break;
    }
    while (token[step] == '\n')
      step++;
    token = strtok(buf, "\n");
    if (strstr(token, args->stm))
    {
      stmflg++;
      break;
    }
    start += strlen(token) + step;
  }

  if (count < 0)
    ERR("read");

  if (close(in))
    ERR("close");

  // if there is no statement
  if (!stmflg)
    gw_err(name, NO_STM, args);
}

// Generate error message like "[DD.MM.YYYY_HH:mm] (<name>) <message>", write to "args->Error" and send SIGUSR1 to thread "error"
void gw_err(char *name, char *message, argsCheck *args)
{
  char path[PATH_MAX];
  time_t rtime;
  struct tm *ntime;
  int signo;
  time(&rtime);
  ntime = localtime(&rtime);

  if (getcwd(path, PATH_MAX) == NULL)
    ERR("getcwd");

  // wait for "ready to write" (SIGUSR2)
  if (sigwait(args->pMask, &signo))
    ERR("sigwait");
  if (signo != SIGUSR1)
  {
    printf("Unexpected signal %d\n", signo);
    exit(EXIT_FAILURE);
  }

  // generate error message and write to "args->Error"
  pthread_mutex_lock(args->mxError);
  sprintf(args->Error, "[%02d.%02d.%d_%02d:%02d] (%s/%s) %s\n", ntime->tm_mday, ntime->tm_mon + 1, ntime->tm_year + 1900, ntime->tm_hour, ntime->tm_min, path, name, message);
  pthread_mutex_unlock(args->mxError);

  // send "log error" to thread "error"
  pthread_kill(*args->error_tid, SIGUSR1);
}