#include "thread_pool.h"
#include "event_loop.h"
#include "tcp_conn.h"
#include <unistd.h>
#include <cstdio>


/*
 * 一旦有task消息过来，这个业务是处理task消息业务的主流程
 *
 * 只要有人调用 thread_queue:: send()方法就会触发此函数
 * 触发的机理是send()内部将会往evfd写数据，触发evfd的EPOLLIN，然后evfd绑定的读回调函数为deal_task_message
 *
*/

void deal_task_message(event_loop *loop, int fd, void *args){

    //得到是哪个消息队列触发的
	thread_queue<task_msg> *queue = (thread_queue<task_msg> *)args;
	// 将队列中的全部任务取出来
	std::queue<task_msg> tasks;
	queue->recv(tasks);
	
//	printf("deal_task_message调用成功！\n");
	// 遍历队列，取出任务，判断任务类型
	while(!tasks.empty()){
		task_msg task = tasks.front();	//取出一个任务
		tasks.pop();

		if(task.type == task_msg::NEW_CONN){
			// 是一个新建连接的任务
			// 将相应的fd上树监听,这里就是在建立连接的时候，将fd和loop进行了绑定，每个线程的loop监听固定的fd
			tcp_conn *conn = new tcp_conn(task.connfd, loop);
			
			if (conn == NULL) {
                fprintf(stderr, "in thread new tcp_conn error\n");
                exit(1);
            }

            printf("[thread]: get new connection succ!\n");
		} else if(task.type == task_msg::NEW_TASK){
	//		printf("=====> 这里执行到了的《====\n");
			// 是一个普通的任务
			//当前的loop就是一个thread的事件监控loop,让当前loop触发task任务的回调
			loop->add_task(task.task_cb, task.args);


		} else{
			// 其他未识别的任务
			fprintf(stderr, "unknow task!\n");
		}

	}

}


// 每一个线程的主业务函数
void *thread_main(void *args){
	thread_queue<task_msg> *queue = (thread_queue<task_msg>*)args;

	// 每个线程都有一个event_loop 进行监听,实现对消息队列的消息的处理
	event_loop *loop = new event_loop();
	if (loop == NULL) {
        fprintf(stderr, "new event_loop error\n");
        exit(1);
    }

	//注册一个触发消息任务读写的callback函数
    queue->set_loop(loop);
    queue->set_callback(deal_task_message, queue);

	//启动阻塞监听
    loop->event_process();

    return NULL;
	
}

thread_pool::thread_pool(int thread_cnt){
	
	_queues = NULL;
	_index = 0;
	_thread_cnt = thread_cnt;
	if (_thread_cnt <= 0) {
        fprintf(stderr, "_thread_cnt < 0\n");
        exit(1);
    }

	//任务队列的个数和线程个数一致
	_queues = new thread_queue<task_msg>*[_thread_cnt]; 
	_tids = new pthread_t[_thread_cnt];
	
	int ret;
	for(int i = 0; i < _thread_cnt; i++){
		//创建一个线程
        printf("create %d thread\n", i);
        //给当前线程创建一个任务消息队列
		_queues[i] = new thread_queue<task_msg>();

		ret = pthread_create(&_tids[i], NULL, thread_main, _queues[i]);
		if (ret == -1) {
            perror("thread_pool, create thread");
            exit(1);
        }

		//将线程设置为分离模式，实现线程的自动回收
        pthread_detach(_tids[i]);
	}

}

thread_queue<task_msg>* thread_pool::get_thread()
{
    if (_index == _thread_cnt) {
        _index = 0;
    }
	printf("\n=====current thread index : %d =======\n", _index);
    return _queues[_index++];
}

void thread_pool::send_task(task_func func, void *args){
	task_msg task;

	//给当前thread_pool中的每个thread里的pool添加一个task任务
	for(int i = 0; i < _thread_cnt; i++){
		//封装一个task消息
		task.type = task_msg::NEW_TASK;
		task.task_cb = func;
		task.args = args;

		// 取出第i个thread的消息队列
		thread_queue<task_msg> *queue = _queues[i];
	//	printf("====>queue->send() <====\n");
		//发送task消息
		queue->send(task);
	}
}


