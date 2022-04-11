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

int connect_socket(char *name, char *port)
{
    struct sockaddr_in addr;
    int socketfd;
    socketfd = make_socket();
    addr = make_address(name, port);
    if (connect(socketfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0)
    {
        if (errno != EINTR)
            ERR("connect");
        else
        {
            fd_set wfds;
            int status;
            socklen_t size = sizeof(int);
            FD_ZERO(&wfds);
            FD_SET(socketfd, &wfds);
            if (TEMP_FAILURE_RETRY(select(socketfd + 1, NULL, &wfds, NULL, NULL)) < 0)
                ERR("select");
            if (getsockopt(socketfd, SOL_SOCKET, SO_ERROR, &status, &size) < 0)
                ERR("getsockopt");
            if (0 != status)
                ERR("connect");
        }
    }
    return socketfd;
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

void doClient(int fd, char *name)
{
    char buf[BUF_SIZE];
    int count = 0, prev_count;
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    for (;;)
    {
        if (select(fd + 1, &rfds, NULL, NULL, NULL) > 0)
        {
            prev_count = count;
            if ((count += TEMP_FAILURE_RETRY(read(fd, buf + count, BUF_SIZE))) < 0)
            {
                if (errno == EPIPE)
                {
                    break;
                }
                ERR("read");
            }
            if (strncmp(buf + prev_count, "Koniec", 7) == 0)
                break;
            if (buf[count - 1] == '\0')
            {
                if (printf("Question from %s:\n%s\n", name, buf) < 0)
                    ERR("printf");
                if (scanf("%s", buf) < 0)
                    ERR("scanf");
                if (bulk_write(fd, buf, sizeof(char)) < 0)
                    ERR("write");
                count = 0;
            }
        }
        else
        {
            if (errno != EINTR)
                ERR("select");
        }
    }
}

int main(int argc, char **argv)
{
    int fd;
    if (argc != 3)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (sethandler(SIG_IGN, SIGPIPE))
        ERR("Seting SIGPIPE:");
    fd = connect_socket(argv[1], argv[2]);
    doClient(fd, argv[1]);
    if (TEMP_FAILURE_RETRY(close(fd)) < 0)
        ERR("close");
    return EXIT_SUCCESS;
}