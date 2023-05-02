#include "threadpool.h"

void* process(void* arg) {
	printf("thread 0x%x working on task %ld\n", (unsigned int)pthread_self(), (long)arg);
	sleep(1);
	printf("thread %ld is end\n", (long)arg);
	return NULL;
}

threadpool_t* threadpool_create(int min_thr_num, int max_thr_num, int taskq_capacity) {
	threadpool_t* pool = NULL;
	do {
		pool = (threadpool_t*)malloc(sizeof(threadpool_t));
		if (pool == NULL) {
			printf("%s: malloc error\n", __func__);
			break;
		}
		pool->min_thr_num = min_thr_num;
		pool->max_thr_num = max_thr_num;
		pool->busy_thr_num = 0;
		pool->live_thr_num = min_thr_num;
		pool->dying_thr_num = 0;

		pool->taskq_front = 0;
		pool->taskq_rear = 0;
		pool->taskq_size = 0;
		pool->taskq_capacity = taskq_capacity;
		pool->shutdown = false;

		pool->worker_tids = (pthread_t*)malloc(sizeof(pthread_t) * max_thr_num);
		if (pool->worker_tids == NULL) {
			printf("%s: malloc error\n", __func__);
			break;
		}
		memset(pool->worker_tids, 0, sizeof(pthread_t) * max_thr_num);

		pool->task_queue = (threadpool_task_t*)malloc(sizeof(threadpool_task_t) * taskq_capacity);
		if (pool->task_queue == NULL) {
			printf("%s: malloc error\n", __func__);
			break;
		}

		/*初始化互斥锁和条件变量*/
		if (pthread_mutex_init(&(pool->self_lock), NULL) || pthread_mutex_init(&(pool->busy_thr_num_lock), NULL) || pthread_cond_init(&(pool->taskq_not_empty), NULL) || pthread_cond_init(&(pool->taskq_not_full), NULL)) {
			printf("init mutex or cond fail\n");
			break;
		}

		/*创建N个任务线程*/
		for (int i = 0; i < min_thr_num; ++i) {
			pthread_create(&(pool->worker_tids[i]), NULL, worker_callback, (void*)pool);  // pool指向当前线程池
			printf("start thread 0x%x\n", (unsigned int)pool->worker_tids[i]);
		}

		/*创建管理者线程,管理者线程的回调函数为manager_tid,参数为整个线程池描述体指针*/
		pthread_create(&(pool->manager_tid), NULL, manager_callback, (void*)pool);
		return pool;
	} while (0);
	threadpool_free(pool);
	return NULL;
}

void* worker_callback(void* threadpool) {
	threadpool_t* pool = (threadpool_t*)threadpool;
	threadpool_task_t task;

	while (true) {
		pthread_mutex_lock(&(pool->self_lock));

		/*taskq_size==0说明没有任务(并且线程池没有关闭),调用wait函数阻塞在条件变量上,若有任务,跳过该while*/
		while (pool->taskq_size == 0 && !pool->shutdown) {
			/*刚创建线程池时,没有任务,所有线程都阻塞在queue_not_empty这个条件变量上*/
			printf("thread 0x%x is waiting\n", (unsigned int)pthread_self());
			pthread_cond_wait(&(pool->taskq_not_empty), &(pool->self_lock));	//block here

			/*清除指定数目的空闲线程,如果要结束的线程个数>0,结束线程*/
			if (pool->dying_thr_num > 0) {
				pool->dying_thr_num--;

				/*如果线程池里存活线程个数>最小值,可以结束当前线程*/
				if (pool->live_thr_num > pool->min_thr_num) {
					printf("thread 0X%x is exiting\n", (unsigned int)pthread_self());
					pool->live_thr_num--;
					pthread_mutex_unlock(&(pool->self_lock));
					pthread_exit(NULL);
				}
			}
		}

		if (pool->shutdown) {
			pthread_mutex_unlock(&(pool->self_lock));
			printf("thread 0x%x is exiting\n", (unsigned int)pthread_self());
			// pthread_detach(pthread_self());
			pthread_exit(NULL);
		}

		task.callback = pool->task_queue[pool->taskq_front].callback;
		task.arg = pool->task_queue[pool->taskq_front].arg;

		pool->taskq_front = (pool->taskq_front + 1) % pool->taskq_capacity;
		pool->taskq_size--;

		/*通知线程有新的任务被处理*/
		pthread_cond_broadcast(&(pool->taskq_not_full));

		pthread_mutex_unlock(&(pool->self_lock));

		printf("thread 0x%x start working\n", (unsigned int)pthread_self());
		pthread_mutex_lock(&(pool->busy_thr_num_lock));
		pool->busy_thr_num++;
		pthread_mutex_unlock(&(pool->busy_thr_num_lock));

		/*执行任务处理函数*/
		(*(task.callback))(task.arg);

		printf("thread 0x%x finish working\n", (unsigned int)pthread_self());
		pthread_mutex_lock(&(pool->busy_thr_num_lock));
		pool->busy_thr_num--;
		pthread_mutex_unlock(&(pool->busy_thr_num_lock));
	}
	pthread_exit(NULL);
}

void* manager_callback(void* threadpool) {
	int i = 0;
	threadpool_t* pool = (threadpool_t*)threadpool;

	while (!pool->shutdown) {
		/*管理者线程每隔10s管理一次*/
		sleep(MANAGE_INTERVAL);

		/*从线程池中获取线程状态,由于要访问共享数据区,要进行加锁和解锁*/
		pthread_mutex_lock(&(pool->self_lock));
		int taskq_size = pool->taskq_size;
		int live_thr_num = pool->live_thr_num;
		pthread_mutex_unlock(&(pool->self_lock));

		pthread_mutex_lock(&(pool->busy_thr_num_lock));
		int busy_thr_num = pool->busy_thr_num;
		pthread_mutex_unlock(&(pool->busy_thr_num_lock));

		/*创建新线程:任务数大于最小线程个数,且存活线程数小于最大线程个数*/
		if ((taskq_size >= MIN_WAIT_TASK_NUM) && (live_thr_num <= pool->max_thr_num)) {
			pthread_mutex_lock(&(pool->self_lock));
			int cnt = 0;
			/*每次增加DEFAULT_THREAD_VARY个子线程*/
			for (i = 0; i < pool->max_thr_num && pool->live_thr_num < pool->max_thr_num && cnt < DEFAULT_THREAD_VARY; ++i) {
				if (pool->worker_tids[i] == 0 && !is_thread_alive(pool->worker_tids[i])) {
					pthread_create(&(pool->worker_tids[i]), NULL, worker_callback, (void*)pool);
					cnt++;
					pool->live_thr_num++;
				}
			}
			pthread_mutex_unlock(&(pool->self_lock));
		}

		/*销毁多余子线程*/
		if (busy_thr_num * 2 < live_thr_num && live_thr_num > pool->min_thr_num) {
			pthread_mutex_lock(&(pool->self_lock));
			pool->dying_thr_num = DEFAULT_THREAD_VARY;
			pthread_mutex_unlock(&(pool->self_lock));

			for (i = 0; i < DEFAULT_THREAD_VARY; ++i) {
				/*通知处在空闲状态的线程,他们会自行终止*/
				pthread_cond_signal(&(pool->taskq_not_empty));
			}
		}
	}
	pthread_exit(NULL);
}

int threadpool_addtask(threadpool_t* pool, void* (*callback)(void* arg), void* arg) {
	pthread_mutex_lock(&(pool->self_lock));

	while ((pool->taskq_size == pool->taskq_capacity) && (!pool->shutdown)) {
		pthread_cond_wait(&(pool->taskq_not_full), &(pool->self_lock));
	}

	/*如果线程池被关闭了,通知所有线程自杀*/
	if (pool->shutdown) {
		pthread_cond_broadcast(&(pool->taskq_not_empty));
		pthread_mutex_unlock(&(pool->self_lock));
		return 0;
	}

	/*任务队列的队尾元素的参数设置为NULL(为什么)*/
	if (pool->task_queue[pool->taskq_rear].arg != NULL) {
		pool->task_queue[pool->taskq_rear].arg = NULL;
	}

	/*给任务队列的队尾添加任务:设置回调函数和其参数*/
	pool->task_queue[pool->taskq_rear].callback = callback;
	pool->task_queue[pool->taskq_rear].arg = arg;
	pool->taskq_rear = (pool->taskq_rear + 1) % pool->taskq_capacity;
	pool->taskq_size++;

	/*唤醒阻塞在条件变量上的线程*/
	pthread_cond_signal(&(pool->taskq_not_empty));
	pthread_mutex_unlock(&(pool->self_lock));
	return 0;
}

int threadpool_destroy(threadpool_t* pool) {
	if (pool == NULL) {
		return -1;
	}
	pool->shutdown = true;
	pthread_join(pool->manager_tid, NULL);

	int i = 0;
	for (i = 0; i < pool->live_thr_num; ++i) {
		pthread_cond_broadcast(&(pool->taskq_not_empty));
	}

	/*销毁任务线程*/
	for (i = 0; i < pool->live_thr_num; ++i) {
		pthread_join(pool->worker_tids[i], NULL);
	}

	threadpool_free(pool);
	return 0;
}

int threadpool_free(threadpool_t* pool) {
	if (pool == NULL) {
		return -1;
	}
	if (pool->task_queue) {
		free(pool->task_queue);
	}
	if (pool->worker_tids) {
		free(pool->worker_tids);
	}
	pthread_mutex_destroy(&(pool->self_lock));
	pthread_mutex_destroy(&(pool->busy_thr_num_lock));
	pthread_cond_destroy(&(pool->taskq_not_empty));
	pthread_cond_destroy(&(pool->taskq_not_full));

	free(pool);
	return 0;
}
// TODO
int is_thread_alive(pthread_t tid) {
	return 0;
}