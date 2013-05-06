#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int cntdwn = 5;

void * thread_a(void * dummy)
{
    pthread_mutex_lock(&mutex);
    while (cntdwn)
    {
        pthread_cond_wait(&cond, &mutex);
        printf("thread: %d\n", cntdwn);
    }
    pthread_mutex_unlock(&mutex);
    return dummy;
}

int main(void)
{
    pthread_t tid;
    pthread_create(&tid, NULL, &thread_a, NULL);
    void * ret = NULL;
    pthread_mutex_lock(&mutex);
    while (cntdwn > 0)
    {
        --cntdwn;
        printf("main: %d\n", cntdwn);
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&mutex);
        sleep(1);
        pthread_mutex_lock(&mutex);
    }
    pthread_mutex_unlock(&mutex);
    pthread_join(tid, &ret);
    return 0;
}

