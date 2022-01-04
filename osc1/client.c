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
#include <sys/epoll.h>

#define gettid() syscall(SYS_gettid)
#define NUM_THREADS 3
#define BUFFER_SIZE 20

extern int errno;

sem_t *mutex;
int pipef[4];
int p_c=0;

#define PIPE1  "pipe1"
#define PIPE2  "pipe2"
#define PIPE3  "pipe3"
#define PIPESC  "pipesc"

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

void *client1(void *temp);
void *client2(void *temp); 
void *client3(void *temp);  

//定义缓冲区
typedef struct buffer_struct{
    int rear;
    int front;
    unsigned long int buffer[BUFFER_SIZE];
}buffer_struct;

int main(int argc, char *argv[])
{
  printf("begin\n");
  int p;
  p=mkfifo(PIPE1, 0666|S_IFIFO);
  printf("%d\n",p);
  p=mkfifo(PIPE2, 0666|S_IFIFO);
  printf("%d\n",p);
  p=mkfifo(PIPE3, 0666|S_IFIFO);
  printf("%d\n",p);
  p=mkfifo(PIPESC, 0666|S_IFIFO);
  printf("%d\n",p); 
  printf("mkfifo success!\n");
  buffer_struct bf;
  memset(&bf, 0, sizeof(buffer_struct));
  double lambda_s = atof(argv[1]);//读取lambda p转化为数字

  if (argc == 1 || argc > 2){
      fprintf(stderr,"Invalid argument!\n");
      return 1;
  }
  
  //创建3个client
  pthread_t tid[NUM_THREADS];
  pthread_attr_t attr[NUM_THREADS];
  pthread_attr_init(&attr[0]);
  pthread_create(&tid[0], &attr[0], client1, &lambda_s); 
  pthread_attr_init(&attr[1]);
  pthread_create(&tid[1], &attr[1], client2, &lambda_s); 
  pthread_attr_init(&attr[2]);
  pthread_create(&tid[2], &attr[2], client3, &lambda_s); 
  
  printf("start pipeing!!\n");
  pipef[0] = open(PIPE1, O_RDONLY);//pipe
  if(pipef[0] < 0)
  {
    printf("open pipef1 error is %s\n", strerror(errno));
    return -1;
  }
  printf("open pipe1 ok\n");
  pipef[1] = open(PIPE2, O_RDONLY);
  if(pipef[1] < 0)
  {
    printf("open pipef2 error is %s\n", strerror(errno));
    return -1;
  }
  printf("open pipe2 ok\n");
  pipef[2] = open(PIPE3, O_RDONLY);
  if(pipef[2] < 0)
  {
    printf("open pipef3 error is %s\n", strerror(errno));
    return -1;
  }
  printf("open pipe3 ok\n");
  pipef[3] = open(PIPESC, O_WRONLY);
  if(pipef[3] < 0)
  {
    printf("open pipef_sc error is %s\n", strerror(errno));
    return -1;
  }
  printf("open all pipe ok\n");
  for(int i=0; i<3; i++){
    if (fcntl(pipef[i], F_SETFL, O_NONBLOCK) < 0)  
    {  
        printf("F_SETFL error\n");  
        exit(0);  
    }  
  }
  
  for(int i=0; i<4; i++){
    printf("pipef: %d\n",pipef[i]);
  }
  
  printf("start epoll!\n");
  struct epoll_event ev;
  struct epoll_event events[4];
  // 创建一个epoll对象
  int ep_fd = epoll_create(4096);       // 创建epoll模型,ep_fd指向红黑树根节点
  printf("ep_fd: %d\n", ep_fd);
  for (int i=0; i<3; i++){
    ev.events  = EPOLLIN | EPOLLET;	// 指定监听读事件 注意:默认为水平触发LT
    ev.data.fd = pipef[i];  	// 注意:一般的epoll在这里放fd
    int q=epoll_ctl(ep_fd, EPOLL_CTL_ADD, pipef[i], &ev);	// 将listen_fd和对应的结构体设置到树上
    printf("epoll_ctl: %d\n", q);
    //printf("errno: %d\n", errno);
  }
  
  ev.events  = EPOLLOUT;
  ev.data.fd = pipef[3];  	
  epoll_ctl(ep_fd, EPOLL_CTL_ADD, pipef[3], &ev);
  
  bf.front=0;
  bf.rear=0;
  int count=0;
  unsigned long int text;
  int j;  
  do{
    int n_ready = epoll_wait(ep_fd, events, 4, -1);
    //printf("n_ready: %d\n", n_ready);
    for(int i=0; i<n_ready; i++){
    //printf("%d\t%d\n",bf.front,bf.rear);
      if(events[i].events & EPOLLIN){
        j=read(events[i].data.fd, &text, 8);
        if((j > 0) && ((bf.rear+1)%BUFFER_SIZE !=bf.front))
        {
          printf("received text :%lu .\n",text);
          bf.buffer[bf.rear] = text;
          bf.rear=(bf.rear+1) % BUFFER_SIZE;
        }
        else if ((j > 0) && ((bf.rear+1)%BUFFER_SIZE ==bf.front)){
          printf("buffer full !!! throwing text :%lu .\n",text);
          count++;
        }
        else{printf("%d\n",j);}
      }
      else if (events[i].events & EPOLLOUT){
        if(bf.rear !=bf.front)
        {
          printf("sending text :%lu  to server.\n",bf.buffer[bf.front]);
          write(events[i].data.fd, &bf.buffer[bf.front], 8);
          bf.front=(bf.front+1) % BUFFER_SIZE;
          count++;
        }
      }  
      //else{printf("%d\n",errno);
    }
  }while(count<31);

  unlink(PIPE1);
  unlink(PIPE2);
  unlink(PIPE3);
  unlink(PIPESC);
  return 0;
}

void *client1(void *temp){//生产者
    int *rec = (int *)malloc(sizeof(int));
    int pipef;
    pipef= open(PIPE1, O_WRONLY);
    
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
void *client2(void *temp){//生产者
    int *rec = (int *)malloc(sizeof(int));
    int pipef;
    pipef= open(PIPE2, O_WRONLY);
    
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
void *client3(void *temp){//生产者
    int *rec = (int *)malloc(sizeof(int));
    int pipef;
    pipef= open(PIPE3, O_WRONLY);
    
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
