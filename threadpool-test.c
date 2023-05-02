#include "threadpool.h"

int main(void) {
    threadpool_t* thp = threadpool_create(3, 100, 100);

    /*模拟向线程池中添加任务*/
    int num[20], i;
    for (i = 0; i < 20; ++i) {
        num[i] = i;
        printf("add task:%d\n", i);
        threadpool_addtask(thp, process, (void*)&num[i]);
    }

    sleep(10);  // 等待子线程完成任务
    threadpool_destroy(thp);
    return 0;
}