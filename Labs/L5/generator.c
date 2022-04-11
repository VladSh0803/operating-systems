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
#include <limits.h>

#define ERR(source) (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), \
                     perror(source), kill(0, SIGKILL),               \
                     exit(EXIT_FAILURE))

#define SMALLMSG_SIZE sizeof(pid_t) + sizeof(char) * 5
#define BIGMSG_SIZE SMALLMSG_SIZE + sizeof(char) * 6

volatile sig_atomic_t last_signal = 0;

void create(char *q1, char *q2, mqd_t *qp1, mqd_t *qp2, int n);
void send_messages(int n, mqd_t qp1);
void work(mqd_t qp1, mqd_t qp2, int t, int p);
void end(char *q1, char *q2, mqd_t qp1, mqd_t qp2);
void read_args(int argc, char **argv, int *n, char **q1, char **q2, int *t, int *p);

void usage(void)
{
    fprintf(stderr, "USAGE: generator t p q1 q2 n\n");
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

void create(char *q1, char *q2, mqd_t *qp1, mqd_t *qp2, int n)
{
    struct mq_attr attr;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = SMALLMSG_SIZE;

    if (n == 0)
    {
        if ((*qp1 = TEMP_FAILURE_RETRY(mq_open(q1, O_RDWR, 0600, &attr))) == (mqd_t)-1)
        {
            if (errno == ENOENT)
            {
                fprintf(stderr, "Nie ma utowrzonych kolejek\n");
                end(q1, q2, *qp1, *qp2);
                exit(EXIT_FAILURE);
            }
            else
                ERR("mq_open");
        }

        attr.mq_msgsize = BIGMSG_SIZE;
        if ((*qp2 = TEMP_FAILURE_RETRY(mq_open(q2, O_WRONLY, 0600, &attr))) == (mqd_t)-1)
        {
            if (errno == ENOENT)
            {
                fprintf(stderr, "Nie ma utowrzonych kolejek\n");
                end(q1, q2, *qp1, *qp2);
                exit(EXIT_FAILURE);
            }
            else
                ERR("mq_open");
        }
    }
    else
    {
        if (mq_unlink(q1) < 0)
            if (errno != ENOENT)
                ERR("mq_unlink");
        if (mq_unlink(q2) < 0)
            if (errno != ENOENT)
                ERR("mq_unlink");

        if ((*qp1 = TEMP_FAILURE_RETRY(mq_open(q1, O_RDWR | O_CREAT, 0600, &attr))) == (mqd_t)-1)
            ERR("mq_open");

        attr.mq_msgsize = BIGMSG_SIZE;
        if ((*qp2 = TEMP_FAILURE_RETRY(mq_open(q2, O_WRONLY | O_CREAT, 0600, &attr))) == (mqd_t)-1)
            ERR("mq_open");
    }
}

void send_messages(int n, mqd_t qp1)
{
    char buf[SMALLMSG_SIZE];

    *((pid_t *)buf) = getpid();
    *(buf + sizeof(pid_t)) = '/';
    *(buf + sizeof(pid_t) + 4 * sizeof(char)) = '\0';

    for (int i = 0; i < n; i++)
    {
        for (int j = 1; j < 4; j++)
        {
            *(buf + sizeof(pid_t) + j * sizeof(char)) = ('a' + rand() % ('z' - 'a'));
        }

        if (TEMP_FAILURE_RETRY(mq_send(qp1, buf, SMALLMSG_SIZE, 0)))
            ERR("mq_send");
    }
}

void work(mqd_t qp1, mqd_t qp2, int t, int p)
{
    char big_buf[BIGMSG_SIZE], small_buf[SMALLMSG_SIZE];
    int sec;
    *(big_buf + BIGMSG_SIZE - 1) = '\0';

    if (sethandler(sig_handler, SIGINT))
        ERR("Setting handler");

    for (;;)
    {
        if (last_signal == SIGINT)
        {
            break;
        }
        sec = t;
        if (mq_receive(qp1, small_buf, SMALLMSG_SIZE, NULL) < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            ERR("mq_receive");
        }
        while (sec > 0)
        {
            sec = sleep(sec);
        }
        printf("Generator received message: %d%s\n", *((pid_t *)small_buf), small_buf + sizeof(pid_t));
        if (1 + rand() % 100 <= p)
        {
            for (int i = 0; i < SMALLMSG_SIZE; i++)
            { //memcpy(big_buf, small_buf, SMALLMSG_SIZE);
                big_buf[i] = small_buf[i];
            }
            *(big_buf + SMALLMSG_SIZE - 1) = '/';
            for (int i = 0; i < 5; i++)
            {
                *(big_buf + SMALLMSG_SIZE + i * sizeof(char)) = 'a' + rand() % ('z' - 'a');
            }
            if (mq_send(qp2, big_buf, BIGMSG_SIZE, 1))
            {
                if (errno == EINTR)
                {
                    continue;
                }
                ERR("mq_send");
            }
            printf("Generator sent message: %d%s\n", *((pid_t *)big_buf), big_buf + sizeof(pid_t));
        }
        if (mq_send(qp1, small_buf, SMALLMSG_SIZE, 0))
        {
            if (errno == EINTR)
            {
                continue;
            }
            ERR("mq_send");
        }
    }
}

void end(char *q1, char *q2, mqd_t qp1, mqd_t qp2)
{
    if (qp1 > 0)
    {
        if (mq_close(qp1) < 0)
            ERR("mq_close");
        if (mq_unlink(q1) < 0)
            ERR("mq_unlink");
    }
    if (qp2 > 0)
    {
        if (mq_close(qp2) < 0)
            ERR("mq_close");
        if (mq_unlink(q2) < 0)
            ERR("mq_unlink");
    }
    free(q1);
    free(q2);
}

void read_args(int argc, char **argv, int *n, char **q1, char **q2, int *t, int *p)
{
    if (argc < 5 || argc > 6)
        usage();
    *q1 = (char *)malloc(strlen(argv[3]) + sizeof(char));
    if (*q1 == NULL)
        ERR("malloc");
    *q2 = (char *)malloc(strlen(argv[4]) + sizeof(char));
    if (*q2 == NULL)
        ERR("malloc");
    if (argc == 5)
        *n = 0;
    else
    {
        *n = atoi(argv[5]);
        if (*n < 1 || *n > 10)
            usage();
    }
    *t = atoi(argv[1]);
    if (*t < 1 || *t > 10)
        usage();
    *p = atoi(argv[2]);
    if (*p < 0 || *p > 100)
        usage();
    for (int i = 0; i < strlen(argv[3]); i++)
    { //strcpy(*q1, argv[3]);
        (*q1)[i] = argv[3][i];
    }
    for (int i = 0; i < strlen(argv[4]); i++)
    { //strcpy(*q2, argv[4]);
        (*q2)[i] = argv[4][i];
    }
}

int main(int argc, char **argv)
{
    char *q1, *q2;
    int n, t, p;
    mqd_t qp1, qp2;
    srand(getpid());

    read_args(argc, argv, &n, &q1, &q2, &t, &p);

    create(q1, q2, &qp1, &qp2, n);

    send_messages(n, qp1);
    work(qp1, qp2, t, p);

    end(q1, q2, qp1, qp2);
    return EXIT_SUCCESS;
}