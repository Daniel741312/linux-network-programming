#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MANAGE_INTERVAL 3		//管理者线程轮询间隔
#define MIN_WAIT_TASK_NUM 10	//最小任务数
#define DEFAULT_THREAD_VARY 10	//增减线程的步长

typedef struct {
	void* (*callback)(void* arg);
	void* arg;
} threadpool_task_t;

struct threadpool_t {
	pthread_mutex_t self_lock;			//锁住本结构体
	pthread_mutex_t busy_thr_num_lock;	//记录忙状态的线程个数的锁,busy_thr_num
	pthread_cond_t taskq_not_full;		//当任务队列满时,添加任务的线程阻塞,等待此条件变量
	pthread_cond_t taskq_not_empty;		//当任务队列不为空时,通知等待任务的线程

	pthread_t* worker_tids;	 //存放线程池中每个线程的tid,数组
	pthread_t manager_tid;	 //管理者线程tid

	threadpool_task_t* task_queue;	//任务队列,每一个队员是(回调函数和其参数)结构体
	int taskq_front;				// task_queue队头下标
	int taskq_rear;					// task_queue队尾下标
	int taskq_size;					// task_queue队中实际任务数
	int taskq_capacity;				// task_queue队列可容纳任务数上限

	int min_thr_num;	//线程池最小线程数
	int max_thr_num;	//线程池最大线程数
	int live_thr_num;	//当前存活的线程个数
	int busy_thr_num;	//忙的线程个数
	int dying_thr_num;	//要销毁的线程个数

	int shutdown;  //标志位,线程池使用状态,true或false
};
typedef struct threadpool_t threadpool_t;

void* process(void* arg);

threadpool_t* threadpool_create(int min_thr_num, int max_thr_num, int taskq_capacity);
void* worker_callback(void* threadpool);
void* manager_callback(void* threadpool);
int threadpool_addtask(threadpool_t* pool, void* (*callback)(void* arg), void* arg);
int threadpool_free(threadpool_t* pool);
int threadpool_destroy(threadpool_t* pool);
int is_thread_alive(pthread_t tid);
#endif