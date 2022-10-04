//gcc -pthread -o <exe filename> <source code file name> -lm
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include "Vector.h"
#include <time.h>
#include <pthread.h>

#define SEGMENTS 1e5

typedef struct Task {
    double (*integrate_function) (double);
    double a;
    double b;
} Task;

// shared resources
int unprocessed_tasks_amount = 0;
Task* vector = NULL;
bool end_task = false;

pthread_mutex_t mutex;
pthread_cond_t condition;

double func1(double x) {
    return x * x + x;
}

double func2(double x) {
    return sin(x);
}

double func3(double x) {
    return log(x);
}

void taskGen(int tasks_amount) {
    FILE* fp;

    if ((fp = fopen("tasks.txt", "w")) == NULL) {
        puts("No such file\n");
        return;
    }

    char* arr[3] = {"func1", "func2", "func3"};

    for (size_t i = 0; i < tasks_amount; i++)
    {
        int func_name_random_index = rand() % 3;
        double a = (double)rand() / RAND_MAX;
        double b = a + ((double)rand() / RAND_MAX);
        fprintf(fp, "%s %lf %lf\n", arr[func_name_random_index], a, b);
    }
    
    fclose(fp);
}

void producer() {
    FILE* fp;

    vector_empty(vector);

    if ((fp = fopen("tasks.txt", "r")) == NULL) {
        puts("No such file\n");
        return;
    }

    char func_name[6];
    double a = 0;
    double b = 0;

    while (true) {
        int ret = fscanf(fp, "%s %lf %lf", func_name, &a, &b);
        if (errno != 0) {
            perror("scanf:");
            break;
        }
        else if (ret == EOF) {
            pthread_mutex_lock(&mutex);
            end_task = true;
            pthread_mutex_unlock(&mutex);
            break;
        }

        Task task;

        if (strcmp(func_name, "func1") == 0)
        {
            task.a = a;
            task.b = b;
            task.integrate_function = func1;
        }
        else if (strcmp(func_name, "func2") == 0) {
            task.a = a;
            task.b = b;
            task.integrate_function = func2;
        }
        else if (strcmp(func_name, "func3") == 0) {
            task.a = a;
            task.b = b;
            task.integrate_function = func3;
        }

        pthread_mutex_lock(&mutex);
        ++unprocessed_tasks_amount;
        vector_push_back(vector, task);
        pthread_cond_signal(&condition);
        pthread_mutex_unlock(&mutex);
    }

    fclose(fp);
}

double integrate(Task task, int segments) {
    double step = (task.b - task.a) / segments;
    double x = task.a;
    double sum = 0;
    while (x < task.b) {
        sum += task.integrate_function(x);
        x += step;
    }

    return sum * step;
}

void consumer(void* args) {
    
    int segments = *((int*)args);
    FILE* fp;

    if ((fp = fopen("results.txt", "w")) == NULL) {
        puts("No such file\n");
        return;
    }

    Task task;

    int counter = 0;
    while (true) {
        pthread_mutex_lock(&mutex);
        if (end_task == true && unprocessed_tasks_amount == 0) {
            pthread_mutex_unlock(&mutex);
            return;
        }

        if (unprocessed_tasks_amount == 0)
        {
            pthread_cond_wait(&condition, &mutex);
        }

        task = vector[counter];
        unprocessed_tasks_amount--;
        pthread_mutex_unlock(&mutex);
        double result = integrate(task, segments);
        if(task.integrate_function == func1){
            fprintf(fp, "func1 %lf %lf RESULT= %lf\n", task.a, task.b, result);
        } else if (task.integrate_function == func2){
            fprintf(fp, "func2 %lf %lf RESULT= %lf\n", task.a, task.b, result);
        } else {
            fprintf(fp, "func3 %lf %lf RESULT= %lf\n", task.a, task.b, result);
        }
        counter++;
    }

    fclose(fp);
}

void doJob(int* segments)
{
    int res = 0;
    pthread_t thProducer, thConsumer;

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&condition, NULL);

    res = pthread_create(&thProducer, NULL, producer, NULL);
    if (res != 0)
    {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    res = pthread_create(&thConsumer, NULL, consumer, (void*)segments);
    if (res != 0)
    {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_join(thProducer, NULL);
    pthread_join(thConsumer, NULL);

    pthread_cond_destroy(&condition);
}

int main(void) {
    srand(time(NULL));
    taskGen(1000);
    
    int segments[] = {1e6, 1e7, 1e8};

    for (size_t i = 0; i < 3; i++)
    {
        clock_t t;
        t = clock();

		doJob(&segments[i]);

        t = clock() - t;
        double time_taken = ((double)t)/CLOCKS_PER_SEC; // calculate the elapsed time
        printf("On segments= %d, time=%f sec\n", segments[i], time_taken);
    }

    return EXIT_SUCCESS;
}