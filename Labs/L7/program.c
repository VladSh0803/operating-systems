/*
------------------------------------------------------------------------
Oświadczam, że niniejsza praca stanowiąca podstawę do uznania
osiągnięcia efektów uczenia się z przedmiotu SOP została wykonana przeze
mnie samodzielnie.
Vladyslav Shestakov 308904
------------------------------------------------------------------------
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <semaphore.h>
#include <time.h>
#include <pthread.h>
#include <netdb.h>
#include <signal.h>

#define MEM_NAME "/mem"
#define SEM_NAME "/sem1"
#define SEM2_NAME "/sem2"
#define STRING_NUM 10
#define STRING_LEN 64
#define BACKLOG 3
#define ADDRESS "localhost"
#define PORT "5051"

#define ERR(source) (perror(source),                                 \
                     fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), \
                     exit(EXIT_FAILURE))

typedef struct
{
    int id;
    int clientfd;
    //int sent;
    int *idlethreads;
    int *socket;
    int *condition;
    char *ptr;
    pthread_cond_t *cond;
    pthread_mutex_t *mutex;
    sem_t *sem;
} thread_arg;

typedef struct
{
    int n;
    //int *sents[STRING_NUM];
    int *clientfds[STRING_NUM];
    char *ptrs[STRING_NUM];
    sem_t *sem2;
    //sem_t *sem;
} revthread_arg;

void usage(char *name)
{
    fprintf(stderr, "USAGE: %s -p or %s -k \n", name, name);
}

int make_socket()
{
    int sock;
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        ERR("socket");
    return sock;
}

struct sockaddr_in make_address(char *address, char *port)
{
    int ret;
    struct sockaddr_in addr;
    struct addrinfo *result;
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    if ((ret = getaddrinfo(address, port, &hints, &result)))
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        exit(EXIT_FAILURE);
    }
    addr = *(struct sockaddr_in *)(result->ai_addr);
    freeaddrinfo(result);
    return addr;
}

int bind_socket(char *address, char *port)
{
    struct sockaddr_in addr;
    int socketfd, t = 1;
    socketfd = make_socket();
    addr = make_address(address, port);
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)))
        ERR("setsockopt");
    if (bind(socketfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        ERR("bind");
    if (listen(socketfd, 1) < 0)
        ERR("listen");
    return socketfd;
}

int add_new_client(int sfd)
{
    int nfd;
    if ((nfd = TEMP_FAILURE_RETRY(accept(sfd, NULL, NULL))) < 0)
    {
        if (EAGAIN == errno || EWOULDBLOCK == errno)
            return -1;
        ERR("accept");
    }
    return nfd;
}

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0x00, sizeof(struct sigaction));
    act.sa_handler = f;

    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void threadwork(int *cliendfd, char *ptr, sem_t *sem)
{
    char buffer[STRING_LEN];
    int size;
    for (;;)
    {
        memset(buffer, 0x00, STRING_LEN);
        if ((size = TEMP_FAILURE_RETRY(read(*cliendfd, buffer, STRING_LEN))) < 0)
        {
            ERR("read");
        }
        else if (size == 0)
        {
            if (close(*cliendfd) == -1)
                ERR("close");
            break;
        }
        if (sem_wait(sem) == -1)
            ERR("sem_wait");
        strcpy(ptr, buffer);
        //*sent = 1;
        if (sem_post(sem) == -1)
            ERR("sem_post");
    }
}

void cleanup(void *arg)
{
    pthread_mutex_unlock((pthread_mutex_t *)arg);
}

void *threadfunc(void *arg)
{
    thread_arg *targ = (thread_arg *)arg;
    for (;;)
    {
        pthread_cleanup_push(cleanup, (void *)targ->mutex);
        if (pthread_mutex_lock(targ->mutex) != 0)
            ERR("pthread_mutex_lock");
        (*targ->idlethreads)++;
        while (!*targ->condition)
            if (pthread_cond_wait(targ->cond, targ->mutex) != 0)
                ERR("pthread_cond_wait");
        *targ->condition = 0;
        (*targ->idlethreads)--;
        targ->clientfd = *targ->socket;
        pthread_cleanup_pop(1);
        threadwork(&targ->clientfd, targ->ptr, targ->sem);
    }
    return NULL;
}

void *threadRevWork(void *arg)
{
    revthread_arg *targ = (revthread_arg *)arg;
    for (;;)
    {
        if (sem_wait(targ->sem2) == -1)
            ERR("sem_wait");
        for (int i = 0; i < targ->n; i++)
        {
            if (*targ->ptrs[i] != '\0')
            {
                if (TEMP_FAILURE_RETRY(send(*targ->clientfds[i], targ->ptrs[i], STRING_LEN, 0)) == -1)
                    ERR("send");
                *targ->ptrs[i] = '\0';
                //*(targ->ptrs[i] + 1) = 'a';
                //*targ->sents[i] = 0;
            }
        }
        if (sem_post(targ->sem2) == -1)
            ERR("sem_post");
    }
    return NULL;
}

void init(pthread_t *thread, pthread_t *revthread, thread_arg *targ, pthread_cond_t *cond, pthread_mutex_t *mutex, int *idlethreads, int *socket, int *condition, int n, char *ptr, sem_t *sem, revthread_arg *revtarg, sem_t *sem2)
{
    revtarg->n = n;
    //revtarg->sem = sem;
    revtarg->sem2 = sem2;
    for (int i = 0; i < n; i++)
    {
        targ[i].id = i;
        targ[i].cond = cond;
        targ[i].mutex = mutex;
        targ[i].idlethreads = idlethreads;
        targ[i].socket = socket;
        targ[i].condition = condition;
        targ[i].ptr = ptr + STRING_LEN * i;
        targ[i].sem = sem;
        //targ[i].sent = 0;
        targ[i].clientfd = -1;
        revtarg->ptrs[i] = targ[i].ptr;
        //revtarg->sents[i] = &(targ[i].sent);
        revtarg->clientfds[i] = &(targ[i].clientfd);
        if (pthread_create(&thread[i], NULL, threadfunc, (void *)&targ[i]) != 0)
            ERR("pthread_create");
    }
    if (pthread_create(revthread, NULL, threadRevWork, (void *)revtarg) != 0)
        ERR("pthread_create");
}

void prepare(int *socket)
{
    int new_flags;
    sethandler(SIG_IGN, SIGPIPE);
    *socket = bind_socket(ADDRESS, PORT);
    new_flags = fcntl(*socket, F_GETFL) | O_NONBLOCK;
    if (fcntl(*socket, F_SETFL, new_flags) == -1)
        ERR("fcntl");
}

void dowork(int socket, pthread_t *thread, thread_arg *targ, pthread_cond_t *cond, pthread_mutex_t *mutex, int *idlethreads, int *cfd, int *condition)
{
    int clientfd;
    fd_set base_rfds, rfds;
    FD_ZERO(&base_rfds);
    FD_SET(socket, &base_rfds);
    printf("Listening on 5051\n");
    for (;;)
    {
        rfds = base_rfds;
        if (select(socket + 1, &rfds, NULL, NULL, NULL) > 0)
        {
            if ((clientfd = add_new_client(socket)) == -1)
                continue;
            if (pthread_mutex_lock(mutex) != 0)
                ERR("pthread_mutex_lock");
            if (*idlethreads == 0)
            {
                if (TEMP_FAILURE_RETRY(close(clientfd)) == -1)
                    ERR("close");
                if (pthread_mutex_unlock(mutex) != 0)
                    ERR("pthread_mutex_unlock");
            }
            else
            {
                *cfd = clientfd;
                if (pthread_mutex_unlock(mutex) != 0)
                    ERR("pthread_mutex_unlock");
                *condition = 1;
                if (pthread_cond_signal(cond) != 0)
                    ERR("pthread_cond_signal");
            }
        }
        else
        {
            ERR("pselect");
        }
    }
}

void producent(int n)
{
    sem_t *sem, *sem2;
    int shm_fd, condition = 0, socket, cfd, idlethreads = 0;
    pthread_t thread[n];
    pthread_t revthread;
    thread_arg targ[n];
    revthread_arg revtarg;
    char *ptr;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    prepare(&socket);
    if (sem_unlink(SEM_NAME) == -1)
    {
        if (errno != ENOENT)
            ERR("sem_unlink");
    }
    if (sem_unlink(SEM2_NAME) == -1)
    {
        if (errno != ENOENT)
            ERR("sem_unlink");
    }
    if ((sem = sem_open(SEM_NAME, O_CREAT, 0666, 1)) == SEM_FAILED)
        ERR("sem_open");
    if ((sem2 = sem_open(SEM2_NAME, O_CREAT, 0666, 0)) == SEM_FAILED)
        ERR("sem_open");
    if (shm_unlink(MEM_NAME) == -1)
    {
        if (errno != ENOENT)
            ERR("shm_unlink");
    }
    if ((shm_fd = shm_open(MEM_NAME, O_CREAT | O_RDWR, 0777)) == -1)
        ERR("shm_open");
    if (ftruncate(shm_fd, STRING_LEN * STRING_NUM) == -1)
        ERR("ftruncate");
    if ((ptr = (char *)mmap(NULL, STRING_LEN * STRING_NUM, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == (char *)-1)
        ERR("mmap");
    memset(ptr, 0x00, STRING_NUM * STRING_LEN);
    init(thread, &revthread, targ, &cond, &mutex, &idlethreads, &cfd, &condition, n, ptr, sem, &revtarg, sem2);
    dowork(socket, thread, targ, &cond, &mutex, &idlethreads, &cfd, &condition);
    if (munmap(ptr, STRING_LEN * STRING_NUM) == -1)
        ERR("munmap");
    if (sem_close(sem) == -1)
        ERR("sem_close");
    if (pthread_cond_broadcast(&cond) != 0)
        ERR("pthread_cond_broadcast");
    for (int i = 0; i < n; i++)
        if (pthread_join(thread[i], NULL) != 0)
            ERR("pthread_join");
    if (TEMP_FAILURE_RETRY(close(socket)) < 0)
        ERR("close");
}

void reverseString(char *ptr)
{
    int end;
    char buffer[STRING_LEN];
    strcpy(buffer, ptr);
    for (end = 0; end < STRING_LEN; end++)
    {
        if (buffer[end] == '\0' || buffer[end] == '\n')
            break;
    }
    for (int i = end - 1; i >= 0; i--)
    {
        ptr[end - 1 - i] = buffer[i];
    }
}

void konsument()
{
    int shm_fd;
    //int reverse[STRING_NUM];
    int reverse = 0;
    char *ptr;
    sem_t *sem, *sem2;
    if ((sem = sem_open(SEM_NAME, O_CREAT, 0666, 0)) == SEM_FAILED)
        ERR("sem_open");
    if ((sem2 = sem_open(SEM2_NAME, O_CREAT, 0666, 0)) == SEM_FAILED)
        ERR("sem_open");
    if ((shm_fd = shm_open(MEM_NAME, O_RDWR, 0666)) == -1)
        ERR("shm_open");
    if ((ptr = (char *)mmap(NULL, STRING_LEN * STRING_NUM, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == (char *)-1)
        ERR("mmap");

    for (;;)
    {
        if (sem_wait(sem) == -1)
            ERR("sem_wait");
        for (int i = 0; i < STRING_NUM; i++)
        {
            if (*(ptr + STRING_LEN * i) != '\0')
            {
                printf("Received: %s", ptr + STRING_LEN * i);
                sleep(1);
                reverseString(ptr + STRING_LEN * i);
                printf("Sent: %s", ptr + STRING_LEN * i);
                reverse = 1;
            }
        }
        if (reverse == 1)
        {
            if (sem_post(sem2) == -1)
                ERR("sem_post");
            sleep(1);
            if (sem_wait(sem2) == -1)
                ERR("sem_post");
            reverse = 0;
        }
        if (sem_post(sem) == -1)
            ERR("sem_post");
    }
    if (munmap(ptr, STRING_LEN * STRING_NUM) == -1)
        ERR("munmap");
    if (sem_close(sem) == -1)
        ERR("sem_close");
}

int main(int argc, char **argv)
{
    char c;
    int n;
    if (argc != 2 && argc != 4)
    {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    if ((c = getopt(argc, argv, "pk")) != -1)
    {
        switch (c)
        {
        case 'p':
            if ((c = getopt(argc, argv, "n:")) != -1)
            {
                n = atoi(optarg);
                if (n < 1 || n > 10)
                {
                    usage(argv[0]);
                    exit(EXIT_FAILURE);
                }
            }
            producent(n);
            break;
        case 'k':
            konsument();
            break;
        default:
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}