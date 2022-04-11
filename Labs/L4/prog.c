/*
--------
Oświadczam, że niniejsza praca stanowiąca podstawę do uznania osiągnięcia efektów
uczenia się z przedmiotu SOP2 została wykonana przeze mnie samodzielnie.
Vladyslav Shestakov
308904
--------
*/

#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>

#define FIFO_NAME "graph.fifo"
#define ADD "add"
#define PRINT "print"
#define CONN "conn"

enum
{
    ADD_CODE,
    PRINT_CODE,
    CONN_CODE,
    RET_SUCCESS,
    RET_FAILURE
};

#define ERR(source) (perror(source),                                 \
                     fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), \
                     exit(EXIT_FAILURE))

volatile sig_atomic_t last_signal = 0;

int sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}

void sig_handler(int sig)
{
    last_signal = sig;
}

void read_from_fifo(int *fds, int n)
{
    ssize_t count;
    char buf[PIPE_BUF], *token;
    int fifo, num1, num2;
    if ((fifo = open(FIFO_NAME, O_RDONLY)) < 0)
    {
        if (errno == EINTR)
        {
            return;
        }
        ERR("open fifo");
    }
    for (;;)
    {
        if (last_signal == SIGINT)
        {
            break;
        }
        if ((count = read(fifo, buf, PIPE_BUF)) < 0)
        {
            if (errno == EINTR)
            {
                break;
            }
            ERR("read");
        }
        if (count > 0)
        {
            token = strtok(buf, " ");
            if (strstr(token, ADD) != NULL)
            {
                token = strtok(NULL, " ");
                num1 = atoi(token);
                token = strtok(NULL, " ");
                num2 = atoi(token);
                if (num1 >= n || num1 < 0 || num2 >= n || num2 < 0)
                {
                    printf("Nieprawidlowe numery wierzcholkow\n");
                    continue;
                }
                *((int *)buf) = ADD_CODE;
                *((int *)(buf + sizeof(int))) = num2;
                if (write(fds[num1], buf, sizeof(int) * 2) < 0)
                {
                    ERR("write");
                }
                *((int *)(buf + sizeof(int))) = num1;
                if (write(fds[num2], buf, sizeof(int) * 2) < 0)
                {
                    ERR("write");
                }
            }
            else if (strstr(token, PRINT) != NULL)
            {
                *((int *)buf) = PRINT_CODE;
                for (int i = 0; i < n; i++)
                {
                    *((int *)(buf + sizeof(int))) = i;
                    if (write(fds[i], buf, sizeof(int) * 2) < 0)
                    {
                        ERR("write");
                    }
                }
            }
            else if (strstr(token, CONN) != NULL)
            {
                token = strtok(NULL, " ");
                num1 = atoi(token);
                token = strtok(NULL, " ");
                num2 = atoi(token);
                if (num1 >= n || num1 < 0 || num2 >= n || num2 < 0)
                {
                    printf("Nieprawidlowe numery wierzcholkow\n");
                    continue;
                }
                *((int *)buf) = CONN_CODE;
                *((int *)(buf + sizeof(int))) = -1;
                *((int *)(buf + sizeof(int) * 2)) = num2;
                if (write(fds[num1], buf, sizeof(int) * 3) < 0)
                {
                    ERR("write");
                }
            }
            else
            {
                printf("Nieprawidlowe polecenie");
                continue;
            }
        }
        else
        {
            kill(0, SIGINT);
            break;
        }
    }
}

void child_work(int fd, int *fds, int n, int num)
{
    ssize_t count;
    int *edge, command, source, dest, *conn, tmp;
    sethandler(sig_handler, SIGINT);
    if ((edge = (int *)malloc(sizeof(int) * n)) == NULL)
    {
        ERR("malloc");
    }
    if ((conn = (int *)malloc(sizeof(int) * 3)) == NULL)
    {
        ERR("malloc");
    }
    for (int i = 0; i < n; i++)
    {
        edge[i] = 0;
    }
    for (;;)
    {
        if (last_signal == SIGINT)
        {
            break;
        }
        if ((count = read(fd, &command, sizeof(int))) < 0)
        {
            if (errno == EINTR)
            {
                break;
            }
            ERR("read");
        }

        if (count > 0)
        {
            if (last_signal == SIGINT)
            {
                break;
            }
            switch (command)
            {
            case ADD_CODE:
                if (read(fd, &command, sizeof(int)) < 0)
                {
                    ERR("read");
                }
                edge[command] = 1;
                break;
            case PRINT_CODE:
                if (read(fd, &command, sizeof(int)) < 0)
                {
                    ERR("read");
                }
                for (int i = command; i < n; i++)
                {
                    if (edge[i] == 1)
                    {
                        printf("Krawedz %d-%d\n", num, i);
                    }
                }
                break;
            case CONN_CODE:
                if (read(fd, &source, sizeof(int)) < 0)
                {
                    ERR("read");
                }
                if (read(fd, &dest, sizeof(int)) < 0)
                {
                    ERR("read");
                }
                if (dest == num)
                {
                    if (source == -1)
                    {
                        printf("Istnieje polaczenie %d-%d\n", num, dest);
                        break;
                    }
                    command = RET_SUCCESS;
                    if (write(fds[source], &command, sizeof(int)) < 0)
                    {
                        ERR("write");
                    }
                    break;
                }
                conn[0] = CONN_CODE;
                conn[1] = num;
                conn[2] = dest;
                for (int i = 0; i < n; i++)
                {
                    if (edge[i] == 1)
                    {
                        if (write(fds[i], conn, sizeof(int) * 3) < 0)
                        {
                            ERR("write");
                        }
                        do
                        {
                            if (read(fd, &command, sizeof(int)) < 0)
                            {
                                ERR("read");
                            }
                            if (command == CONN_CODE)
                            {
                                if (read(fd, &tmp, sizeof(int)) < 0)
                                {
                                    ERR("read");
                                }
                                if (read(fd, &command, sizeof(int)) < 0)
                                {
                                    ERR("read");
                                }
                                command = RET_FAILURE;
                                if (write(fds[tmp], &command, sizeof(int)) < 0)
                                {
                                    ERR("write");
                                }
                                command = CONN_CODE;
                            }
                        } while (command == CONN_CODE);
                        if (command == RET_SUCCESS)
                        {
                            break;
                        }
                    }
                }
                if (source != -1)
                {
                    if (write(fds[source], &command, sizeof(int)) < 0)
                    {
                        ERR("write");
                    }
                }
                else
                {
                    if (command == RET_FAILURE)
                    {
                        printf("Nie ma polaczenia %d-%d\n", num, dest);
                    }
                    else
                    {
                        printf("Istnieje polaczenie %d-%d\n", num, dest);
                    }
                }
                break;
            }
        }
    }
}

void parent_work(int *fds, int n)
{
    sethandler(sig_handler, SIGINT);
    read_from_fifo(fds, n);

    for (int i = 0; i < n; i++)
    {
        wait(NULL);
        if (fds[i] && TEMP_FAILURE_RETRY(close(fds[i])))
        {
            ERR("close");
        }
    }
}

void create_nodes(int n, int *fds)
{
    int tmpfd[2], num, *child_fd, fd;
    if ((child_fd = (int *)malloc(sizeof(int) * n)) == NULL)
    {
        ERR("malloc");
    }
    for (int i = 0; i < n; i++)
    {
        if (pipe(tmpfd))
        {
            ERR("pipe");
        }

        child_fd[i] = tmpfd[0];
        fds[i] = tmpfd[1];
    }

    for (int i = 0; i < n; i++)
    {
        switch (fork())
        {
        case 0:
            num = i;
            for (int j = 0; j < n; j++)
            {
                if (j != i)
                {
                    if (child_fd[j] && TEMP_FAILURE_RETRY(close(child_fd[j])))
                    {
                        ERR("close");
                    }
                }
            }
            fd = child_fd[i];
            free(child_fd);
            child_work(fd, fds, n, num);
            if (fd && TEMP_FAILURE_RETRY(close(fd)))
            {
                ERR("close");
            }
            for (int i = 0; i < n; i++)
            {
                if (fds[i] && TEMP_FAILURE_RETRY(close(fds[i])))
                {
                    ERR("close");
                }
            }
            exit(EXIT_SUCCESS);
        case -1:
            ERR("fork");
        }
    }
    free(child_fd);
}

void usage(char *name)
{
    fprintf(stderr, "USAGE: %s n\n", name);
    fprintf(stderr, "0<n<=10 - number of children\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    int n, *fds;
    if (argc != 2)
    {
        usage(argv[0]);
    }
    n = atoi(argv[1]);
    if (n <= 0 || n > 10)
    {
        usage(argv[0]);
    }
    if ((fds = (int *)malloc(sizeof(int) * n)) == NULL)
    {
        ERR("malloc");
    }
    if (unlink(FIFO_NAME) < 0)
    {
        if (errno != ENOENT)
        {
            ERR("Remove fifo");
        }
    }
    if (mkfifo(FIFO_NAME, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) < 0)
    {
        ERR("create fifo");
    }
    create_nodes(n, fds);
    parent_work(fds, n);
    free(fds);
    if (unlink(FIFO_NAME) < 0)
    {
        ERR("remove fifo");
    }
    return EXIT_SUCCESS;
}
