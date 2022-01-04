#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <pthread.h>
#include <semaphore.h>

#define gettid() syscall(SYS_gettid)
#define NUM_THREADS 3
#define BUFFER_SIZE 20

//定义缓冲区
typedef struct buffer_struct{
    int rear;
    int front;
    int buffer[BUFFER_SIZE];
}buffer_struct;

sem_t *full;
sem_t *empty;
sem_t *mutex;
int pipef;
void *ptr;

double sleep_time(double lambda_s);//返回一个符合负指数分布的随机变量
void *server(void *temp); //生产者
void *pipe_func(void *temp);

int main(int argc, char *argv[])
{
    buffer_struct bf;
    memset(&bf, 0, sizeof(buffer_struct));
    double lambda_s = atof(argv[1]);//读取lambda p转化为数字
    
    //打开具名信号量并初始化 
    full = sem_open("sfull", O_CREAT, 0666, 0);
    empty = sem_open("sempty", O_CREAT, 0666, 0);
    mutex = sem_open("smutex", O_CREAT, 0666, 0);
    sem_init(full, 1, 0);
    sem_init(empty, 1, BUFFER_SIZE);
    sem_init(mutex, 1, 1);

    if (argc != 2){
        printf("The number of supplied arguments are false.\n");
        return -1;
    }

    if (atof(argv[1]) < 0){
        printf("The lambda entered should be greater than 0.\n");
        return -1;
    }

    //创建共享内存
    int shm_fd = shm_open("server_buf", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(buffer_struct));
    ptr = mmap(0, sizeof(buffer_struct), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    //创建3个服务器和一个管道
    pthread_t tid[NUM_THREADS];
    pthread_t tid0;
    pthread_attr_t attr[NUM_THREADS];
    pthread_attr_t attr0;

    for (int i = 0; i < NUM_THREADS; i++){
        pthread_attr_init(&attr[i]);
        pthread_create(&tid[i], &attr[i], server, &lambda_s);
    }
    pthread_attr_init(&attr0);
    pthread_create(&tid0, &attr0, pipe_func, &lambda_s);

    for (int j = 0; j < NUM_THREADS; j++){
        pthread_join(tid[j], NULL);
    }
    pthread_join(tid0, NULL);

    return 0;
}


double sleep_time(double lambda_s){
    double r;
    r = ((double)rand() / RAND_MAX);

    while(r == 0 || r == 1){
        r = ((double)rand() / RAND_MAX);
    }

    r = (-1 / lambda_s) * log(1-r);

    return r;
}

void *server(void *temp){
    double lambda_s = *(double *)temp;
    do{
        double interval_time = lambda_s;
        unsigned int sleepTime = (unsigned int)sleep_time(interval_time);
        sleep(sleepTime);
        int item = rand()%999;
        buffer_struct *shm_ptr = ((buffer_struct *)ptr);
        sem_wait(empty);
        sem_wait(mutex);
        printf("Sleep Time: %d s | Producing the data %d to buffer[%d] by thread %ld in process %d.\n", sleepTime, item,shm_ptr->rear,gettid(), getpid());
        shm_ptr->buffer[shm_ptr->rear] = item;
        shm_ptr->rear = (shm_ptr->rear+1) % BUFFER_SIZE;
        sem_post(mutex);
        sem_post(full);
    }while(1);
    pthread_exit(0);
}

void *pipe_func(void *temp)
{
    int *rec = (int *)malloc(sizeof(int));
    pipef = open("./pipe_func", O_WRONLY);
    if(pipef < 0)
	{
		printf("open pipef error is %s\n", strerror(errno));
		*rec = -1;
		pthread_exit((void *)rec);
		return NULL;
	}
    do{
        sem_wait(full);
        sem_wait(mutex);
        buffer_struct *shm_ptr = ((buffer_struct *)ptr);
        int item  = shm_ptr->buffer[shm_ptr->front];
        char str[5];
        sprintf(str+strlen(str), "%d", item);
        write(pipef, str, sizeof(str));
        shm_ptr->front = (shm_ptr->front+1) % BUFFER_SIZE;
        memset(str, 0, sizeof(str));
        sem_post(mutex);
        sem_post(empty);
    }while(1);
    pthread_exit(0);
}
