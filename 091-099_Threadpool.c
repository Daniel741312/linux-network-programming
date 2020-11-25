#include<unistd.h>
#include<errno.h>
#include<stdlib.h>
#include<stdio.h>
#include<signal.h>
#include<string.h>
#include<stdbool.h>
#include<pthread.h>

#define DEFAULT_TIME 10						//管理者线程轮询间隔
#define MIN_WAIT_TASK_NUM 10				//最小任务数
#define DEFAULT_THREAD_VARY 10				//增减线程的步长

/*线程任务结构体,包含线程的回调函数和其参数*/
typedef struct{
	void* (*function)(void* arg);
	void* arg;
}threadpool_task_t;

/*描述线程池相关信息*/
struct threadpool_t{
	/*两把锁*/
	pthread_mutex_t lock;					//锁住本结构体
	pthread_mutex_t thread_counter;			//记录忙状态的线程个数的锁,busy_thr_num
	/*两个条件变量*/	
	pthread_cond_t queue_not_full;			//当任务队列满时,添加任务的线程阻塞,等待此条件变量
	pthread_cond_t queue_not_empty;			//当任务队列不为空时,通知等待任务的线程

	pthread_t* threads;						//存放线程池中每个线程的tid,数组
	pthread_t adjust_tid;					//管理者线程tid

	threadpool_task_t* task_queue;			//任务队列,每一个队员是(回调函数和其参数)结构体
	int queue_front;						//task_queue队头下标
	int queue_rear;							//task_queue队尾下标
	int queue_size;							//task_queue队中实际任务数
	int queue_max_size;						//task_queue队列可容纳任务数上限

	int min_thr_num;						//线程池最小线程数
	int max_thr_num;						//线程池最大线程数
	int live_thr_num;						//当前存活的线程个数
	int busy_thr_num;						//忙的线程个数
	int wait_exit_thr_num;					//要销毁的线程个数
	
	int shutdown;							//标志位,线程池使用状态,true或false
};
typedef struct threadpool_t threadpool_t;

/*业务处理函数*/
void* process(void* arg);

/*创建一个线程池*/
threadpool_t* threadpool_create(int min_thr_num,int max_thr_num,int queue_max_size);

/*任务线程回调函数*/
void* threadpool_thread(void* threadpool);

/*管理者线程回调函数*/
void* adjust_thread(void* threadpool);

/*给线程池的任务队列添加任务*/
int threadpool_add(threadpool_t* pool,void*(*function)(void* arg),void* arg);

/*释放锁和条件变量,以及线程池的内存*/
int threadpool_free(threadpool_t* pool);

/*销毁线程池*/
int threadpool_destory(threadpool_t* pool);


/*这个函数没实现,临时写一下*/
int is_thread_alive(pthread_t tid){
	return 0;
}


/*调用者threadpool_add:threadpool_add(thp,process,(void*)&num[i]);*/
void* process(void* arg){
	printf("Thread 0x%x working on task %d\n",(unsigned int)pthread_self(),(int)arg);
	sleep(1);
	printf("Thread %d is end\n",(int)arg);
	return NULL;
}

/*调用者main:threadpool_t* thp=threadpool_create(3,10,10)*/
threadpool_t* threadpool_create(int min_thr_num,int max_thr_num,int queue_max_size){

	int i=0;
	/*创建线程池结构体指针*/
	threadpool_t* pool=NULL;

	do{
		if((pool=(threadpool_t*)malloc(sizeof(threadpool_t)))==NULL){
			printf("malloc threadpool fail\n");
			break;
		}

		pool->min_thr_num=min_thr_num;
		pool->max_thr_num=max_thr_num;
		pool->busy_thr_num=0;
		pool->live_thr_num=min_thr_num;
		pool->wait_exit_thr_num=0;

		pool->queue_front=0;
		pool->queue_rear=0;
		pool->queue_size=0;
		pool->queue_max_size=queue_max_size;
		pool->shutdown=false;

		/*给线程数组开辟空间*/
		pool->threads=(pthread_t*)malloc(sizeof(pthread_t)*max_thr_num);
		if(pool->threads==NULL){
			printf("malloc threads fail\n");
			break;
		}
		memset(pool->threads,0,sizeof(pthread_t)*max_thr_num);

		/*给任务队列开辟空间*/
		pool->task_queue=(threadpool_task_t*)malloc(sizeof(threadpool_task_t)*queue_max_size);
		if(pool->task_queue==NULL){
			printf("malloc task_queue fail\n");
			break;
		}

		/*初始化互斥锁和条件变量*/
		if((pthread_mutex_init(&(pool->lock),NULL)!=0)||(pthread_mutex_init(&(pool->thread_counter),NULL)!=0)
			||(pthread_cond_init(&(pool->queue_not_empty),NULL)!=0)||(pthread_cond_init(&(pool->queue_not_full),NULL)!=0)){

			printf("init the lock or cond fail\n");
			break;
		}

		/*创建N个任务线程*/
		for(i=0;i<min_thr_num;++i){
			/*将tid存入任务线程数组,设置任务线程回调函数为threadpool_thread,参数为整个线程池描述体指针*/
			pthread_create(&(pool->threads[i]),NULL,threadpool_thread,(void*)pool);			//pool指向当前线程池
			printf("Start thread 0X%x...\n",(unsigned int)pool->threads[i]);
		}

		/*创建管理者线程,管理者线程的回调函数为adjust_tid,参数为整个线程池描述体指针*/
		pthread_create(&(pool->adjust_tid),NULL,adjust_thread,(void*)pool);
		/*正常情况下在这里return*/
		return pool;

	}while(0);

	/*如果前面的代码失败,释放空间,擦屁股,返回NULL*/
	threadpool_free(pool);
	return NULL;
}

/*调用者pthread_create:pthread_create(&(pool->threads[i]),NULL,threadpool_thread,(void*)pool);*/
void* threadpool_thread(void* threadpool){
	threadpool_t* pool=(threadpool_t*)threadpool;
	/*创建一个任务实例*/
	threadpool_task_t task;

	while(true){
		/*刚创建出线程,等待任务队列里有任务,否则阻塞等待任务队列里有任务后再唤醒接收任务*/
		pthread_mutex_lock(&(pool->lock));

		/*queue_size==0说明没有任务(并且线程池没有关闭),调用wait函数阻塞在条件变量上,若有任务,跳过该while*/
		while((pool->queue_size==0)&&(!pool->shutdown)){
			/*刚创建线程池时,没有任务,所有线程都阻塞在queue_not_empty这个条件变量上*/
			printf("thread 0X%x is waiting\n",(unsigned int)pthread_self());
			pthread_cond_wait(&(pool->queue_not_empty),&(pool->lock));

			/*清除指定数目的空闲线程,如果要结束的线程个数>0,结束线程*/
			if(pool->wait_exit_thr_num>0){
				pool->wait_exit_thr_num--;

				/*如果线程池里存活线程个数>最小值,可以结束当前线程*/
				if(pool->live_thr_num>pool->min_thr_num){
					printf("thread 0X%x is exiting\n",(unsigned int)pthread_self());
					pool->live_thr_num--;
					/*释放锁*/
					pthread_mutex_unlock(&(pool->lock));
					pthread_exit(NULL);
				}
			}
		}

		/*如果关闭了线程池*/
		if(pool->shutdown){
			pthread_mutex_unlock(&(pool->lock));
			printf("Thread 0x%x is exiting\n",(unsigned int)pthread_self());
			pthread_detach(pthread_self());
			pthread_exit(NULL);
		}

		/*从任务队列里获取任务:
			*从任务结构体中获取回调函数
			*从任务结构体中回去回调函数的参数
		*/
		task.function=pool->task_queue[pool->queue_front].function;
		task.arg=pool->task_queue[pool->queue_front].arg;

		/*调整队首指针和队列大小*/
		pool->queue_front=(pool->queue_front+1)%pool->queue_max_size;
		pool->queue_size--;

		/*通知线程有新的任务加进来*/
		pthread_cond_broadcast(&(pool->queue_not_full));

		/*任务取出后,立即将线程池锁释放*/
		pthread_mutex_unlock(&(pool->lock));

		printf("Thread 0x%x start working\n",(unsigned int)pthread_self());
		/*将忙线程数++,注意访问共享数据区加解锁保护*/
		pthread_mutex_lock(&(pool->thread_counter));
		pool->busy_thr_num++;
		pthread_mutex_unlock(&(pool->thread_counter));

		/*执行任务处理函数*/
		(*(task.function))(task.arg);

		printf("Thread 0x%x end working\n",(unsigned int)pthread_self());
		/*将忙线程数--,注意访问共享数据区加解锁保护*/
		pthread_mutex_lock(&(pool->thread_counter));
		pool->busy_thr_num--;
		pthread_mutex_unlock(&(pool->thread_counter));
	}
	pthread_exit(NULL);
}

/*调用者pthread_create:pthread_create(&(pool->adjust_tid),NULL,adjust_thread,(void*)pool);*/
void* adjust_thread(void* threadpool){
	int i=0;
	threadpool_t* pool=(threadpool_t*)threadpool;

	while(!pool->shutdown){
		/*管理者线程每隔10s管理一次*/
		sleep(DEFAULT_TIME);

		/*从线程池中获取线程状态,由于要访问共享数据区,要进行加锁和解锁*/
		pthread_mutex_lock(&(pool->lock));
		int queue_size=pool->queue_size;
		int live_thr_num=pool->live_thr_num;
		pthread_mutex_unlock(&(pool->lock));

		pthread_mutex_lock(&(pool->thread_counter));
		int busy_thr_num=pool->busy_thr_num;
		pthread_mutex_unlock(&(pool->thread_counter));

		/*创建新线程:任务数大于最小线程个数,且存活线程数小于最大线程个数*/
		if((queue_size>=MIN_WAIT_TASK_NUM)&&(live_thr_num<=pool->max_thr_num)){
			pthread_mutex_lock(&(pool->lock));
			int add=0;
			/*每次增加DEFAULT_THREAD_VARY个子线程*/
			for(i=0;(i<pool->max_thr_num)&&(pool->live_thr_num<pool->max_thr_num)&&(add<DEFAULT_THREAD_VARY);++i){
				if((pool->threads[i]==0)&&(!is_thread_alive(pool->threads[i]))){
					pthread_create(&(pool->threads[i]),NULL,threadpool_thread,(void*)pool);
					add++;
					pool->live_thr_num++;
				}
			}
			pthread_mutex_unlock(&(pool->lock));
		}

		/*销毁多余子线程*/
		if(((busy_thr_num*2)<live_thr_num)&&(live_thr_num>pool->min_thr_num)){
			pthread_mutex_lock(&(pool->lock));
			pool->wait_exit_thr_num=DEFAULT_THREAD_VARY;
			pthread_mutex_unlock(&(pool->lock));

			for(i=0;i<DEFAULT_THREAD_VARY;++i){
				/*通知处在空闲状态的线程,他们会自行终止*/
				pthread_cond_signal(&(pool->queue_not_empty));
			}
		}
	}
}

/*调用者main:threadpool_add(thp,process,(void*)&num[i]);*/
int threadpool_add(threadpool_t* pool,void* (*function)(void* arg),void* arg){
	pthread_mutex_lock(&(pool->lock));

	while((pool->queue_size==pool->queue_max_size)&&(!pool->shutdown)){
		pthread_cond_wait(&(pool->queue_not_full),&(pool->lock));
	}

	/*如果线程池被关闭了,通知所有线程自杀*/
	if(pool->shutdown){
		pthread_cond_broadcast(&(pool->queue_not_empty));
		pthread_mutex_unlock(&(pool->lock));
		return 0;
	}

	/*任务队列的队尾元素的参数设置为NULL(为什么)*/
	if(pool->task_queue[pool->queue_rear].arg!=NULL)
		pool->task_queue[pool->queue_rear].arg=NULL;

	/*给任务队列的队尾添加任务:设置回调函数和其参数*/
	pool->task_queue[pool->queue_rear].function=function;
	pool->task_queue[pool->queue_rear].arg=arg;
	/*调整队尾指针和队列大小*/
	pool->queue_rear=(pool->queue_rear+1)%pool->queue_max_size;
	pool->queue_size++;

	/*唤醒阻塞在条件变量上的线程*/
	pthread_cond_signal(&(pool->queue_not_empty));
	/*解锁*/
	pthread_mutex_unlock(&(pool->lock));
	return 0;
}

int threadpool_destroy(threadpool_t* pool){
	int i=0;
	if(pool==NULL)
		return -1;
	pool->shutdown=true;
	/*先销毁管理者线程*/
	pthread_join(pool->adjust_tid,NULL);

	/*通知所有空闲线程*/
	for(i=0;i<pool->live_thr_num;++i)
		pthread_cond_broadcast(&(pool->queue_not_empty));

	/*销毁任务线程*/
	for(i=0;i<pool->live_thr_num;++i)
		pthread_join(pool->threads[i],NULL);

	threadpool_free(pool);

	return 0;
}

int threadpool_free(threadpool_t* pool){
	if(pool==NULL)
		return -1;

	if(pool->task_queue)
		free(pool->task_queue);

	if(pool->threads){
		pthread_mutex_lock(&(pool->lock));
		pthread_mutex_destroy(&(pool->lock));

		pthread_mutex_lock(&(pool->thread_counter));
		pthread_mutex_destroy(&(pool->thread_counter));

		pthread_cond_destroy(&(pool->queue_not_empty));
		pthread_cond_destroy(&(pool->queue_not_full));
	}
	/*释放内存,指针归零*/
	free(pool);
	pool=NULL;
	return 0;
}

int main(void){
	/*创建线程池,函数原型:threadpool_t* threadpool_create(int min_thr_num,int max_thr_num,int queue_max_size)*/
	threadpool_t* thp=threadpool_create(3,10,10);
	printf("pool inited\n");

	/*模拟向线程池中添加任务*/
	int num[20],i;
	for(i=0;i<20;++i){
		num[i]=i;
		printf("add task:%d\n",i);
		threadpool_add(thp,process,(void*)&num[i]);
	}

	sleep(10);								//等待子线程完成任务
	threadpool_destroy(thp);
	return 0;
}