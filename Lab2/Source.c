#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include "Vector.h"
#include <pthread.h>

#define SEGMENTS 1e4

// shared resource
int unprocessed_tasks_amount = 0;

pthread_mutex_t mutex;
pthread_cond_t condition;

//gcc -pthread -o lab lab.c -lm

typedef struct Task {
    double (*integrate_function) (double);
    double a;
    double b;
} Task;

// shared resource
Task* vector = NULL;
bool end_task = false;

double func1(double x) {
    return x * x + x;
}

double func2(double x) {
    return sin(x);
}

double func3(double x) {
    return log(x);
}

void taskGen() {
    FILE* fp;

    if ((fp = fopen("tasks.txt", "w")) == NULL) {
        puts("No such file\n");
        return;
    }

    char* arr[3] = {"func1", "func2", "func3"};

    int tasks_amount = rand() % 20 + 1;

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
    int counter = 0;
    while (true) {
        int ret = fscanf(fp, "%s %lf %lf", func_name, &a, &b);
        if (ret == 3)
            printf("\n%s \t %f \t %f", func_name, a, b);
        else if (errno != 0) {
            perror("scanf:");
            break;
        }
        else if (ret == EOF) {
            pthread_mutex_lock(&mutex);
            end_task = true;
            pthread_mutex_unlock(&mutex);
            break;
        }
        else {
            puts("No or partial match.\n");
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
        printf("Producer - adding task %d\n", counter);
        ++unprocessed_tasks_amount;
        vector_push_back(vector, task);
        pthread_cond_signal(&condition);
        pthread_mutex_unlock(&mutex);
        //printf("\nProducer - after mutex released %d\n", counter);
        //put task into buffer
        counter++;
    }
}

double integrate(Task task) {
    double step = (task.b - task.a) / SEGMENTS;
    double x = task.a;
    double sum = 0;
    while (x < task.b) {
        sum += task.integrate_function(x);
        x += step;
    }

    return sum * step;
}

void consumer() {
    
    FILE* fp;

    if ((fp = fopen("results.txt", "w")) == NULL) {
        puts("No such file\n");
        return;
    }

    Task task;

    int counter = 0;
    while (true) {
        //printf("\nConsumer - before mutex lock %d\n", counter);
        pthread_mutex_lock(&mutex);
        //printf("\nTasks unprocessed %d\n", unprocessed_tasks_amount);
        //printf("\nConsumer - just after mutex lock %d\n", counter);
        if (end_task == true && unprocessed_tasks_amount == 0) {
            printf("Consumer - ending task\n");
            pthread_mutex_unlock(&mutex);
            return;
        }

        if (unprocessed_tasks_amount == 0)
        {
            //printf("\nConsumer - waiting task %d\n", counter);
            pthread_cond_wait(&condition, &mutex);
        }

        task = vector[counter];
        unprocessed_tasks_amount--;
        pthread_mutex_unlock(&mutex);
        printf("Consumer processing task %d\n", counter);
        double result = integrate(task);
        fprintf(fp, "\n%lf %lf result = %lf\n", task.a, task.b, result);
        counter++;
    }

    fclose(fp);
}

int main(void) {
    srand(time(NULL));
    taskGen();
    
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

    res = pthread_create(&thConsumer, NULL, consumer, NULL);
    if (res != 0)
    {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_join(thProducer, NULL);
    pthread_join(thConsumer, NULL);

    return EXIT_SUCCESS;
}