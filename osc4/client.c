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

void *client(void *temp); 

//定义缓冲区
typedef struct buffer_struct{
    int rear;
    int front;
    int flag;
    int count;
    unsigned long int buffer[BUFFER_SIZE][513];
}buffer_struct;

typedef struct in_struct{
  double d;
  int number;
}in_struct;

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
  
  buffer_struct bf1;
  memset(&bf1, 0, sizeof(buffer_struct));
  buffer_struct bf2;
  memset(&bf2, 0, sizeof(buffer_struct));
  buffer_struct bf3;
  memset(&bf3, 0, sizeof(buffer_struct));
  bf1.front=0;
  bf1.rear=0;
  bf1.count=0;
  bf1.flag=0;
  bf2.front=0;
  bf2.rear=0;
  bf2.count=0;
  bf2.flag=0;
  bf3.front=0;
  bf3.rear=0;
  bf3.count=0;
  bf3.flag=0;
  
  double lambda_s = atof(argv[1]);//读取lambda p转化为数字

  if (argc == 1 || argc > 2){
      fprintf(stderr,"Invalid argument!\n");
      return 1;
  }
  
  in_struct st1,st2,st3;
  memset(&st1, 0, sizeof(in_struct));
  memset(&st2, 0, sizeof(in_struct));
  memset(&st3, 0, sizeof(in_struct));
  st1.d=lambda_s;
  st1.number=1;
  st2.d=lambda_s;
  st2.number=2;
  st3.d=lambda_s;
  st3.number=3;
  //创建3个client
  pthread_t tid[NUM_THREADS];
  pthread_attr_t attr[NUM_THREADS];
  pthread_attr_init(&attr[0]);
  pthread_create(&tid[0], &attr[0], client, &st1); 
  pthread_attr_init(&attr[1]);
  pthread_create(&tid[1], &attr[1], client, &st2); 
  pthread_attr_init(&attr[2]);
  pthread_create(&tid[2], &attr[2], client, &st3); 
  
  printf("start pipeing!!\n");
  pipef[0] = open(PIPE1, O_RDONLY|O_NONBLOCK);//pipe
  if(pipef[0] < 0)
  {
    printf("open pipef1 error is %s\n", strerror(errno));
    return -1;
  }
  printf("open pipe1 ok\n");
  pipef[1] = open(PIPE2, O_RDONLY|O_NONBLOCK);
  if(pipef[1] < 0)
  {
    printf("open pipef2 error is %s\n", strerror(errno));
    return -1;
  }
  printf("open pipe2 ok\n");
  pipef[2] = open(PIPE3, O_RDONLY|O_NONBLOCK);
  if(pipef[2] < 0)
  {
    printf("open pipef3 error is %s\n", strerror(errno));
    return -1;
  }
  printf("open pipe3 ok\n");
  pipef[3] = open(PIPESC, O_WRONLY|O_NONBLOCK);
  if(pipef[3] < 0)
  {
    printf("open pipef_sc error is %s\n", strerror(errno));
    return -1;
  }
  printf("open all pipe ok\n");
  for(int i=0; i<=3; i++){
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
  int ep_fd = epoll_create(100000);       // 创建epoll模型,ep_fd指向红黑树根节点
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
  
  int num=0;
  unsigned long int text;
  int j;
  //int queqe=0;   
  do{
    //int que=0;
    int n_ready = epoll_wait(ep_fd, events, 100000, -1);
    //if(n_ready!=1)printf("n_ready: %d\n", n_ready);
    for(int i=0; i<n_ready; i++){
    //printf("%d\t%d\n",bf.front,bf.rear);
      //if(n_ready!=1)printf("%d\n",events[i].data.fd);
      if(events[i].events & EPOLLIN){
        j=read(events[i].data.fd, &text, 8);
        if((j==8) && (events[i].data.fd == pipef[0])){
          if((text !=0) && (bf1.flag == 0)){
            bf1.flag=1;
            printf("received text :%lu .\n",text);
            bf1.buffer[bf1.rear][bf1.count]=text;
            bf1.count=bf1.count+1;
          }
          else if(text !=0){
            if((j ==8) && ((bf1.rear+1)%BUFFER_SIZE !=bf1.front))
            {
              bf1.rear=(bf1.rear+1) % BUFFER_SIZE;
              bf1.count=0;
              printf("received text :%lu .\n",text);
              bf1.buffer[bf1.rear][bf1.count] = text;
              bf1.count=bf1.count+1;
            }
            else if ((j ==8) && ((bf1.rear+1)%BUFFER_SIZE ==bf1.front)){
              printf("buffer full !!! throwing text :%lu .\n",text);
              num++;
            }
            else{printf("%d\n",j);}
          }
          else if(text ==0){
            if((j ==8) && (bf1.count>=1)&&(bf1.count<=512))
            {
              bf1.buffer[bf1.rear][bf1.count] = text;
              bf1.count=bf1.count+1;
            }
          }
        }
        if((j==8) && (events[i].data.fd == pipef[1])){
          if((text !=0) && (bf2.flag == 0)){
            bf2.flag=1;
            printf("received text :%lu .\n",text);
            bf2.buffer[bf2.rear][bf2.count]=text;
            bf2.count=bf2.count+1;
          }
          else if(text !=0){
            if((j ==8) && ((bf2.rear+1)%BUFFER_SIZE !=bf2.front))
            {
              bf2.rear=(bf2.rear+1) % BUFFER_SIZE;
              bf2.count=0;
              printf("received text :%lu .\n",text);
              bf2.buffer[bf2.rear][bf2.count] = text;
              bf2.count=bf2.count+1;
            }
            else if ((j ==8) && ((bf2.rear+1)%BUFFER_SIZE ==bf2.front)){
              printf("buffer full !!! throwing text :%lu .\n",text);
              num++;
            }
            else{printf("%d\n",j);}
          }
          else if(text ==0){
            if((j ==8) && (bf2.count>=1)&&(bf2.count<=512))
            {
              bf2.buffer[bf2.rear][bf2.count] = text;
              bf2.count=bf2.count+1;
            }
          }
        }
        if((j==8) && (events[i].data.fd == pipef[2])){
          if((text !=0) && (bf3.flag == 0)){
            bf3.flag=1;
            printf("received text :%lu .\n",text);
            bf3.buffer[bf3.rear][bf3.count]=text;
            bf3.count=bf3.count+1;
          }
          else if(text !=0){
            if((j ==8) && ((bf3.rear+1)%BUFFER_SIZE !=bf3.front))
            {
              bf3.rear=(bf3.rear+1) % BUFFER_SIZE;
              bf3.count=0;
              printf("received text :%lu .\n",text);
              bf3.buffer[bf3.rear][bf3.count] = text;
              bf3.count=bf3.count+1;
            }
            else if ((j ==8) && ((bf3.rear+1)%BUFFER_SIZE ==bf3.front)){
              printf("buffer full !!! throwing text :%lu .\n",text);
              num++;
            }
            else{printf("%d\n",j);}
          }
          else if(text ==0){
            if((j ==8) && (bf3.count>=1)&&(bf3.count<=512))
            {
              bf3.buffer[bf3.rear][bf3.count] = text;
              bf3.count=bf3.count+1;
            }
          }
        }
      }
      else if (events[i].events & EPOLLOUT){
        int w=0;
        if((bf1.rear !=bf1.front)&&(bf1.flag !=0))//&&(queqe==que++)
        {
          printf("sending text :%lu  to server.\n",bf1.buffer[bf1.front][0]);
          for(int temp=0; temp<=512; temp++){
            do{
              w=write(events[i].data.fd, &bf1.buffer[bf1.front][temp], 8);
            }while(w!=8);
          }
          bf1.front=(bf1.front+1) % BUFFER_SIZE;
          num++;
          //queqe=(queqe+1)%3;
          continue;
        }
        else if((bf2.rear !=bf2.front)&&(bf2.flag !=0))//&&(queqe==que++)
        {
          printf("sending text :%lu  to server.\n",bf2.buffer[bf2.front][0]);
          for(int temp=0; temp<=512; temp++){
            do{
              w=write(events[i].data.fd, &bf2.buffer[bf2.front][temp], 8);
            }while(w!=8);
          }
          bf2.front=(bf2.front+1) % BUFFER_SIZE;
          num++;
          //queqe=(queqe+1)%3;
          continue;
        }
        else if((bf3.rear !=bf3.front)&&(bf3.flag !=0))//&&(queqe==que++)
        {
          printf("sending text :%lu  to server.\n",bf3.buffer[bf3.front][0]);
          for(int temp=0; temp<=512; temp++){
            do{
              w=write(events[i].data.fd, &bf3.buffer[bf3.front][temp], 8);
            }while(w!=8);
          }
          bf3.front=(bf3.front+1) % BUFFER_SIZE;
          num++;
          //queqe=(queqe+1)%3;
          continue;
        }
      }  
      //else{printf("%d\n",errno);
    }
  }while(1);

  unlink(PIPE1);
  unlink(PIPE2);
  unlink(PIPE3);
  unlink(PIPESC);
  return 0;
}

void *client(void *temp){//生产者
    int *rec = (int *)malloc(sizeof(int));
    int pipef;
    
    in_struct st = *(in_struct *)temp;
    double lambda_s=st.d;
    if(st.number ==1) pipef= open(PIPE1, O_WRONLY|O_NONBLOCK);
    else if(st.number ==2) pipef= open(PIPE2, O_WRONLY|O_NONBLOCK);
    else if(st.number ==3) pipef= open(PIPE3, O_WRONLY|O_NONBLOCK);
    
    if(pipef < 0)
    {
	printf("open pipef error is %s\n", strerror(errno));
	*rec = -1;
	pthread_exit((void *)rec);
	return NULL;
    }
    unsigned long int a[512];
    for(int i=0; i<512; i++){
      a[i]=0;
    }
    int n=0;
    int w=0;
    pthread_t pthread_id = pthread_self();
    unsigned long int text= pthread_id;
    do{
        double interval_time = lambda_s;
        unsigned int sleepTime = (unsigned int)sleep_time(interval_time);
        sleep(sleepTime);
        printf("%d Sleep Time: %d s | Producing by thread %lu in process %d.\n", n, sleepTime, text, getpid());
        do{
          w=write(pipef, &text, 8);
          //printf("%d",w);
        }while(w!=8);
        for(int i=0; i<512; i++){
          do{
            w=write(pipef, &a[i], 8);
          }while(w!=8);
        }
        n++;
    }while(1);
    close(pipef);
}
