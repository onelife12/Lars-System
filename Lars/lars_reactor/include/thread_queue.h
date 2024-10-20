#ifndef THREAD_QUEUE_H
#define THREAD_QUEUE_H

#include <queue>
#include <pthread.h>
#include <sys/eventfd.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "event_loop.h"

/*每个子线程都对应一个fd, 一个event_loop, 一个消息队列，event_loop监控fd的变化*/
template <typename T>
class thread_queue{
	
public:
	thread_queue(){
		_loop = NULL;
		pthread_mutex_init(&_queue_mutex, NULL);
		_evfd = eventfd(0, EFD_NONBLOCK);
		if (_evfd == -1) {
            perror("evenfd(0, EFD_NONBLOCK)");
            exit(1);
        }

	}

	~thread_queue(){
		pthread_mutex_destroy(&_queue_mutex);
		close(_evfd);
	}

	//向队列添加一个任务
	void send(const T &task){
		
		//触发消息事件的占位传输内容,当给队列添加任务之后，给队列对应的fd写一个无关数据，激活fd上的IO事件，这样子就可以通知线程取数据了
        unsigned long long idle_num = 1;
		pthread_mutex_lock(&_queue_mutex);
		//将任务添加到队列
		_queue.push(task);

		//向_evfd写，触发对应的EPOLLIN事件,来处理该任务
		int ret = write(_evfd, &idle_num, sizeof(unsigned long long));
		if (ret == -1) {
            perror("_evfd write");
        }

        pthread_mutex_unlock(&_queue_mutex);

	}

	// 获取任务队列，队列中有数据
	void recv(std::queue<T> &new_queue){
		unsigned long long idle_num = 1;
		pthread_mutex_lock(&_queue_mutex);
        //把占位的数据读出来，确保底层缓冲没有数据存留
        int ret = read(_evfd, &idle_num, sizeof(unsigned long long));
        if (ret == -1) {
            perror("_evfd read");
        }

		//将当前的队列拷贝出去,将一个空队列换回当前队列,同时清空自身队列，确保new_queue是空队列
        std::swap(new_queue, _queue);	// 交换两个指针的指向

		pthread_mutex_unlock(&_queue_mutex);
	}	

	//设置当前thead_queue是被哪个事件触发event_loop监控
    void set_loop(event_loop *loop) {
        _loop = loop;  
    }

    //设置当前消息任务队列的 每个任务触发的回调业务
    void set_callback(io_callback *cb, void *args = NULL)
    {
        
		if (_loop != NULL) {
            _loop->add_io_event(_evfd, cb, EPOLLIN, args); // 为消息队列对应的fd绑定回调
        }
    }

    //得到当前loop
    event_loop * get_loop() {
        return _loop;
    }
		


private:
	int _evfd;	//触发消息任务队列读取的每个消息业务的fd,当消息队列绑定的fd触发读写事件，则和其绑定的线程就执行对应业务逻辑
	event_loop *_loop;	// 当前消息任务队列所绑定在哪个event_loop事件触发机制中，机制为main_reactor And sub_reactor;这个loop为子线程服务
	std::queue<T> _queue;	// 存储数据的结构
	pthread_mutex_t _queue_mutex;	// 进行添加任务、读取任务的保护锁

};


#endif

