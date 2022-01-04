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
#include <fcntl.h>

#define gettid() syscall(SYS_gettid)
#define NUM_THREADS 3
#define BUFFER_SIZE 20

extern int errno;

sem_t *mutex;
int pipef[4];
int p_c=0;

double sleep_time(double lambda_s);//返回一个符合负指数分布的随机变量
double sleep_time(double lambda_s){
    double r;
    r = ((double)rand() / RAND_MAX);

    while(r == 0 || r == 1){
        r = ((double)rand() / RAND_MAX);
    }

    r = (-1 / lambda_s) * log(1-r);

    return r;
}

void *client(void *temp); 

int main(int argc, char *argv[])
{
    double lambda_s = atof(argv[1]);//读取lambda p转化为数字

    if (argc != 2){
        printf("The number of supplied arguments are false.\n");
        return -1;
    }

    if (atof(argv[1]) < 0){
        printf("The lambda entered should be greater than 0.\n");
        return -1;
    }
    
    //pipe
    pipef[0] = open("./pipe1", O_RDONLY);
    if(pipef[0] < 0)
    {
	printf("open pipef1 error is %s\n", strerror(errno));
	return -1;
    }
    pipef[1] = open("./pipe2", O_RDONLY);
    if(pipef[1] < 0)
    {
	printf("open pipef2 error is %s\n", strerror(errno));
	return -1;
    }
    pipef[2] = open("./pipe3", O_RDONLY);
    if(pipef[2] < 0)
    {
	printf("open pipef3 error is %s\n", strerror(errno));
	return -1;
    }
    pipef[3] = open("./pipe_sc", O_WRONLY);
    if(pipef[3] < 0)
    {
	printf("open pipef_sc error is %s\n", strerror(errno));
	return -1;
    }

    //创建3个client
    pthread_t tid[NUM_THREADS];
    pthread_attr_t attr[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++){
        pthread_attr_init(&attr[i]);
        pthread_create(&tid[i], &attr[i], client, &lambda_s);
    }
    
    
    int count=0;
    unsigned long int text;
    if (fcntl(pipef[0], F_SETFL, O_NONBLOCK) < 0)  
    {  
        printf("F_SETFL error");  
        exit(0);  
    }  
    if (fcntl(pipef[1], F_SETFL, O_NONBLOCK) < 0)  
    {  
        printf("F_SETFL error");  
        exit(0);  
    }  
    if (fcntl(pipef[2], F_SETFL, O_NONBLOCK) < 0)  
    {  
        printf("F_SETFL error");  
        exit(0);  
    }  
    do{
        if(read(pipef[0], &text, 8) > 0)
	{
            printf("send text :%lu .\n",text);
            write(pipef[3], &text, 8);
            count=count+1;
	}
	if(read(pipef[1], &text, 8) > 0)
	{
            printf("send text :%lu .\n",text);
            write(pipef[3], &text, 8);
            count=count+1;
	}
	if(read(pipef[2], &text, 8) > 0)
	{
            printf("send text :%lu .\n",text);
            write(pipef[3], &text, 8);
            count=count+1;
	}
	//else{printf("%d\n",errno);}
    }while(count<31);

    return 0;
}

void *client(void *temp){//生产者
    int *rec = (int *)malloc(sizeof(int));
    int pipef;
    if(p_c == 0){pipef= open("./pipe1", O_WRONLY);p_c++;}
    else if(p_c == 1){pipef= open("./pipe2", O_WRONLY);p_c++;}
    else{pipef= open("./pipe3", O_WRONLY);}
    
    if(pipef < 0)
    {
	printf("open pipef error is %s\n", strerror(errno));
	*rec = -1;
	pthread_exit((void *)rec);
	return NULL;
    }
    double lambda_s = *(double *)temp;
    int n=0;
    do{
        pthread_t pthread_id = pthread_self();
        unsigned long int text= pthread_id;
        double interval_time = lambda_s;
        unsigned int sleepTime = (unsigned int)sleep_time(interval_time);
        sleep(sleepTime);
        printf("%d Sleep Time: %d s | Producing by thread %lu in process %d.\n", n, sleepTime, text, getpid());
        write(pipef, &text, 8);
    }while(n++ < 9);
    close(pipef);
}
