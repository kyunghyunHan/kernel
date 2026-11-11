
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>

#define THREAD_COUNT  2
#define LOOP_COUNT    1000000

static uint64_t counter = 0;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *worker(void *arg)
{
    int id = *(int *)arg;

    for (int i = 0; i < LOOP_COUNT; i++)
    {
        // Critical Section 시작
        pthread_mutex_lock(&mutex);

        counter++;

        pthread_mutex_unlock(&mutex);
        // Critical Section 종료
    }

    printf("Thread %d finished\n", id);

    return NULL;
}

int main(void)
{
    pthread_t threads[THREAD_COUNT];
    int ids[THREAD_COUNT] = {1, 2};

    for (int i = 0; i < THREAD_COUNT; i++)
    {
        if (pthread_create(&threads[i], NULL,
                           worker, &ids[i]) != 0)
        {
            fprintf(stderr, "pthread_create failed\n");
            return 1;
        }
    }

    for (int i = 0; i < THREAD_COUNT; i++)
    {
        pthread_join(threads[i], NULL);
    }

    printf("Expected : %d\n", THREAD_COUNT * LOOP_COUNT);
    printf("Actual   : %llu\n",
           (unsigned long long)counter);

    pthread_mutex_destroy(&mutex);

    return 0;
}
