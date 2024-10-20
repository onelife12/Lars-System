#pragma once

#include <sys/epoll.h>
#include <ext/hash_map>
#include <ext/hash_set>
#include "event_base.h"

#define MAXEVENTS 1024

// map: fd->io_event, 给fd绑定一个io_event，处理fd上的读写业务
typedef __gnu_cxx::hash_map<int, io_event> io_event_map;	
// io_event_map的迭代器
typedef __gnu_cxx::hash_map<int, io_event>::iterator iem_it;
// 正在监听的全部的fd集合
typedef __gnu_cxx::hash_set<int> listen_fd_set;

// 定义异步任务回调函数
typedef void (*task_func)(event_loop *loop, void *args);

class event_loop{

public:
	event_loop();
	//阻塞循环处理事件
	void event_process();

	//添加一个io事件到loop中
	void add_io_event(int fd, io_callback *proc, int mask, void *args = NULL);

	 //删除一个io事件从loop中
	void del_io_event(int fd);
	
    //删除一个io事件的EPOLLIN/EPOLLOUT
	void del_io_event(int fd, int mask);
	
	//=== 异步任务task模块需要的方法 ===
	
	//添加一个任务task到ready_tasks集合中
	void add_task(task_func func, void *args);

	//执行全部的ready_tasks里面的任务
    void execute_ready_tasks();
    // ===========================================

	//====================
	//获取全部监听事件的fd集合
    void get_listen_fds(listen_fd_set &fds) {
        fds = listen_fds;
    }

private:
	int _epfd;
	  //当前event_loop 监控的fd和对应事件的关系
	io_event_map _io_evs;
	// 全部正在监听的fd集合
	listen_fd_set listen_fds;

	// 一次性最大处理事件,已经通过epoll_wait返回的被激活需要上层处理的fd集合.
	struct epoll_event _fired_evs[MAXEVENTS];
	
	/*task_func_pair: 回调函数和参数的键值对, _ready_tasks: 所有已经就绪的待执行的任务集合。*/
	typedef std::pair<task_func, void *> task_func_pair;
	
	std::vector<task_func_pair> _ready_tasks;
	

};
