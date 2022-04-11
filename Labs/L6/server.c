/*
Oświadczam, że niniejsza praca stanowiąca podstawę do uznania osiągnięcia efektów uczenia
się z przedmiotu SOP2 została wykonana przeze mnie samodzielnie.
Vladyslav Shestakov 308904
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <fcntl.h>
#include <time.h>

#define BUF_SIZE 2001

#define ERR(source) (perror(source),                                 \
                     fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), \
                     exit(EXIT_FAILURE))

volatile sig_atomic_t accept_new_conn = 1;
volatile sig_atomic_t end_work = 0;

void usage(char *name)
{
    fprintf(stderr, "USAGE: %s address port\n", name);
}

int sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}

void sigusr_handler(int sig)
{
    accept_new_conn = 0;
}

void sigint_handler(int sig)
{
    end_work = 1;
}

int make_socket(void)
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

ssize_t bulk_write(int fd, char *buf, size_t count)
{
    int c;
    size_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(write(fd, buf, count));
        if (c < 0)
            return c;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

ssize_t bulk_read(int fd, char *buf, size_t count)
{
    int c;
    size_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0)
            return c;
        if (0 == c)
            return len;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
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
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if ((nfd = TEMP_FAILURE_RETRY(accept(sfd, &addr, &len))) < 0)
    {
        if (EAGAIN == errno || EWOULDBLOCK == errno)
            return -1;
        ERR("accept");
    }
    return nfd;
}

int minRand(int max)
{
    int res = 1 + rand() % 100;
    return res > max ? max : res;
}

void doServer(int fdT, int limit, char **questions, int count)
{
    int cfds[limit], questionsNo[limit], bytesSent[limit], result, fdmax;
    struct timeval timeout;
    fd_set base_rfds, rfds;
    srand(time(NULL));
    timeout.tv_sec = 0;
    timeout.tv_usec = 330000;
    fdmax = fdT;
    FD_ZERO(&base_rfds);
    FD_SET(fdT, &base_rfds);
    for (int i = 0; i < limit; i++)
        cfds[i] = -1;
    for (;;)
    {
        if (end_work == 1)
        {
            for (int i = 0; i < limit; i++)
            {
                if (cfds[i] != -1)
                {
                    if (bulk_write(cfds[i], "Koniec", 7 * sizeof(char)) < 0)
                        ERR("write");
                    if (TEMP_FAILURE_RETRY(close(cfds[i])) < 0)
                        ERR("close");
                }
            }
            break;
        }
        rfds = base_rfds;
        if ((result = select(fdmax + 1, &rfds, NULL, NULL, &timeout)) >= 0)
        {
            if (result == 0 && timeout.tv_usec == 0)
            {
                timeout.tv_sec = 0;
                timeout.tv_usec = 330000;
                for (int i = 0; i < limit; i++)
                {
                    if (cfds[i] != -1)
                    {
                        if (bytesSent[i] < strlen(questions[questionsNo[i]]) + 1)
                        {
                            int bytesToSend = minRand(strlen(questions[questionsNo[i]] + bytesSent[i]) + 1);
                            if ((bytesSent[i] += bulk_write(cfds[i],
                                                            questions[questionsNo[i]] + bytesSent[i],
                                                            sizeof(char) * bytesToSend)) < 0)
                            {
                                if (errno == EPIPE)
                                {
                                    if (TEMP_FAILURE_RETRY(close(cfds[i])) < 0)
                                        ERR("close");
                                    FD_CLR(cfds[i], &base_rfds);
                                    cfds[i] = -1;
                                }
                                else
                                {
                                    ERR("write:");
                                }
                            }
                        }
                    }
                }
            }
            else if (result > 0)
            {
                if (accept_new_conn)
                {
                    int buf;
                    while ((buf = add_new_client(fdT)) >= 0)
                    {
                        int i;
                        for (i = 0; i < limit; i++)
                        {
                            if (cfds[i] == -1)
                            {
                                break;
                            }
                        }
                        if (i == limit)
                        {
                            if (bulk_write(buf, "NIE", 4) < 0 && errno != EPIPE)
                                ERR("write");
                            if (TEMP_FAILURE_RETRY(close(buf)) < 0)
                                ERR("close");
                        }
                        else
                        {
                            cfds[i] = buf;
                            FD_SET(cfds[i], &base_rfds);
                            if (cfds[i] > fdmax)
                                fdmax = cfds[i];
                            questionsNo[i] = rand() % count;
                            bytesSent[i] = 0;
                        }
                    }
                }
                for (int i = 0; i < limit; i++)
                {
                    if (FD_ISSET(cfds[i], &rfds))
                    {
                        char buf;
                        int res;
                        if ((res = bulk_read(cfds[i], &buf, sizeof(char))) <= 0)
                        {
                            if (res == 0)
                            {
                                if (TEMP_FAILURE_RETRY(close(cfds[i])) < 0)
                                    ERR("close");
                                FD_CLR(cfds[i], &base_rfds);
                                cfds[i] = -1;
                            }
                            else
                            {
                                ERR("read");
                            }
                        }
                        questionsNo[i] = rand() % count;
                        bytesSent[i] = 0;
                    }
                }
            }
            else
            {
                if (TEMP_FAILURE_RETRY(close(fdT)) < 0)
                    ERR("close");
                FD_ZERO(&base_rfds);
            }
        }
        else
        {
            if (errno != EINTR)
                ERR("select");
        }
    }
}

char **read_questions(char *path, int *c)
{
    int count, quantity, offset = 0, file;
    char buf[BUF_SIZE], *pch, **questions;
    if ((file = open(path, O_RDONLY)) < 0)
        ERR("open");
    if ((count = TEMP_FAILURE_RETRY(read(file, buf, BUF_SIZE))) > 0)
    {
        pch = strtok(buf, "\n");
        offset += strlen(pch) + 1;
        quantity = *c = atoi(pch);
        questions = (char **)malloc(quantity * sizeof(char *));
        for (int i = 0; i < quantity; i++)
            questions[i] = (char *)malloc(BUF_SIZE * sizeof(char));
    }
    while ((count = TEMP_FAILURE_RETRY(pread(file, buf, BUF_SIZE, offset))) > 0)
    {
        pch = strtok(buf, "\n");
        while (pch != NULL)
        {
            strcpy(questions[--quantity], pch);
            if (quantity == 0)
                break;
            offset += strlen(pch) + 1;
            pch = strtok(NULL, "\n");
        }
        if (quantity == 0)
            break;
    }
    return questions;
}

int main(int argc, char **argv)
{
    int fdT, new_flags, limit, count;
    char **questions;
    if (argc != 5)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    limit = atoi(argv[3]);
    if (sethandler(SIG_IGN, SIGPIPE))
        ERR("Seting SIGPIPE:");
    if (sethandler(sigusr_handler, SIGUSR1))
        ERR("Seting SIGUSR1");
    if (sethandler(sigint_handler, SIGINT))
        ERR("Seting SIGINT");
    questions = read_questions(argv[4], &count);
    fdT = bind_socket(argv[1], argv[2]);
    new_flags = fcntl(fdT, F_GETFL) | O_NONBLOCK;
    fcntl(fdT, F_SETFL, new_flags);
    doServer(fdT, limit, questions, count);
    for (int i = 0; i < count; i++) {
        free(questions[i]);
    }
    free(questions);
    if (TEMP_FAILURE_RETRY(close(fdT)) < 0)
        ERR("close");
    return EXIT_SUCCESS;
}