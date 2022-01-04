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

double sleep_time(double lambda_c);//返回一个符合负指数分布的随机变量
void *client(void *temp);//消费者
void *pipe_func(void *temp);

int main(int argc, char *argv[])
{
    buffer_struct bf;
    memset(&bf, 0, sizeof(buffer_struct));
    double lambda_c = atof(argv[1]);//读取lambda c转化为数字
    
    //打开具名信号量
    full = sem_open("cfull", O_CREAT, 0666, 0);
    empty = sem_open("cempty", O_CREAT, 0666, 0);
    mutex = sem_open("cmutex", O_CREAT, 0666, 0);
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
    int shm_fd = shm_open("client_buf", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(buffer_struct));
    ptr = mmap(0, sizeof(buffer_struct), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    //创建3个客户和一个管道
    pthread_t tid[NUM_THREADS];
    pthread_t tid0;
    pthread_attr_t attr[NUM_THREADS];
    pthread_attr_t attr0;
    
    for (int i = 0; i < NUM_THREADS; i++){
        pthread_attr_init(&attr[i]);
        pthread_create(&tid[i], &attr[i], client, &lambda_c);
    }
    pthread_attr_init(&attr0);
    pthread_create(&tid0, &attr0, pipe_func, &lambda_c);

    for (int j = 0; j < NUM_THREADS; j++){
        pthread_join(tid[j], NULL);
    }
    pthread_join(tid0, NULL);

    return 0;
}


double sleep_time(double lambda_c){
    double r;
    r = ((double)rand() / RAND_MAX);

    while(r == 0 || r == 1){
        r = ((double)rand() / RAND_MAX);
    }

    r = (-1 / lambda_c) * log(1-r);

    return r;
}

void *client(void *temp){
    double lambda_c = *(double *)temp;
    do{
        double interval_time = lambda_c;
        unsigned int sleepTime;
        sleepTime = (unsigned int)sleep_time(interval_time);
        sleep(sleepTime);
        buffer_struct *shm_ptr = ((buffer_struct *)ptr);
        sem_wait(full);
        sem_wait(mutex);
        int item  = shm_ptr->buffer[shm_ptr->front];
        printf("Sleep Time: %d s | Consuming the data %d from the buffer[%d] by the thread %ld in  the process %d.\n", sleepTime, item, shm_ptr->front, gettid(), getpid());
        shm_ptr->front = (shm_ptr->front+1) % BUFFER_SIZE;
        sem_post(mutex);
        sem_post(empty);
    }while(1);
    pthread_exit(0);
}

void *pipe_func(void *temp)
{
    int *rec = (int *)malloc(sizeof(int));
    pipef = open("./pipe_func",O_RDONLY);
    if(pipef < 0)
	{
		printf("open pipef error is %s\n", strerror(errno));
		*rec = -1;
		pthread_exit((void *)rec);
		return NULL;
	}
    do{
        int item;
        char str[5];
        if(read(pipef, str, sizeof(str)) > 0)
		{
			sem_wait(empty);
            sem_wait(mutex);
            buffer_struct *shm_ptr = ((buffer_struct *)ptr);
            sscanf(str,"%d",&item);
            shm_ptr->buffer[shm_ptr->rear] = item;
            shm_ptr->rear = (shm_ptr->rear+1) % BUFFER_SIZE;
            memset(str, 0, sizeof(str));
            sem_post(mutex);
            sem_post(full);
		}
    }while(1);
    close(pipef);
	*rec = 0;
	printf("thread end\n");
	pthread_exit((void *)rec);
}
