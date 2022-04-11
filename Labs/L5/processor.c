/*
Oświadczam, że niniejsza praca stanowiąca podstawę do uznania osiągnięcia efektów uczenia się z przedmiotu SOP2
została wykonana przeze mnie samodzielnie.
Vladyslav Shestakov 308904
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <mqueue.h>

#define ERR(source) (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), \
                     perror(source), kill(0, SIGKILL),               \
                     exit(EXIT_FAILURE))

#define MSG_SIZE sizeof(pid_t) + sizeof(char) * 11

volatile sig_atomic_t last_signal = 0;

void read_args(int argc, char **argv, char **q2, int *t, int *p);
void end(char *q2, mqd_t qp2);
void work(mqd_t qp2, int t, int p);
void open_queue(mqd_t *qp2, char *q2);

void usage(void)
{
    fprintf(stderr, "USAGE: processor t p q2\n");
    exit(EXIT_FAILURE);
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

void sig_handler(int sig)
{
    last_signal = sig;
}

void read_args(int argc, char **argv, char **q2, int *t, int *p)
{
    if (argc != 4)
        usage();
    *q2 = (char *)malloc(strlen(argv[3]) + sizeof(char));
    if (*q2 == NULL)
        ERR("malloc");
    *t = atoi(argv[1]);
    if (*t < 1 || *t > 10)
        usage();
    *p = atoi(argv[2]);
    if (*p < 0 || *p > 100)
        usage();
    for (int i = 0; i < strlen(argv[3]); i++)
    { //strcpy(*q2, argv[3]);
        (*q2)[i] = argv[3][i];
    }
}

void end(char *q2, mqd_t qp2)
{
    if (qp2 != 0)
    {
        if (mq_close(qp2) < 0)
            ERR("mq_close");
        if (mq_unlink(q2) < 0)
            ERR("mq_unlink");
    }
    free(q2);
}

void work(mqd_t qp2, int t, int p)
{
    char buf[MSG_SIZE];
    int sec, iSec = -1;
    buf[0] = '\0';
    struct timespec abstime;

    if (sethandler(sig_handler, SIGINT))
        ERR("Setting handler");

    while (1)
    {
        if (last_signal == SIGINT) {
            break;
        }
        sec = t;
        timespec_get(&abstime, TIME_UTC);
        abstime.tv_sec++;
        if (mq_timedreceive(qp2, buf, MSG_SIZE, NULL, &abstime) < 0)
        {
            if (errno == ETIMEDOUT)
            {
                iSec++;
                if (iSec % t == 0 && buf[0] != '\0')
                {
                    printf("%d%s\n", *(pid_t *)buf, buf + sizeof(pid_t));
                }
                continue;
            }
            else if (errno == EINTR) {
                continue;
            }
            else
                ERR("mq_receive");
        }
        iSec = -1;
        while (sec > 0)
        {
            sec = sleep(sec);
        }
        *((pid_t *)buf) = getpid();
        for (int i = 1; i < 4; i++)
        {
            *(buf + sizeof(pid_t) + i * sizeof(char)) = '0';
        }

        printf("%d%s\n", *((pid_t *)buf), buf + sizeof(pid_t));

        if (1 + rand() % 100 <= p)
        {
            if (mq_send(qp2, buf, MSG_SIZE, 0)) {
                if (errno == EINTR) {
                    continue;
                }
                ERR("mq_send");
            }
        }
    }
}

void open_queue(mqd_t *qp2, char *q2)
{
    struct mq_attr attr;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MSG_SIZE;
    if ((*qp2 = TEMP_FAILURE_RETRY(mq_open(q2, O_RDWR, 0600, &attr))) == (mqd_t)-1)
    {
        if (errno == ENOENT)
        {
            fprintf(stderr, "Nie ma utworzonej kolejki");
            end(q2, *qp2);
            exit(EXIT_FAILURE);
        }
        else
        {
            ERR("mq_open");
        }
    }
}

int main(int argc, char **argv)
{
    int t, p;
    char *q2;
    mqd_t qp2;
    srand(getpid());
    read_args(argc, argv, &q2, &t, &p);
    open_queue(&qp2, q2);
    work(qp2, t, p);
    end(q2, qp2);
    return EXIT_SUCCESS;
}