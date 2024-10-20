#pragma once
#include <netinet/in.h>
#include "event_loop.h"
#include "tcp_conn.h"

#include "message.h"
#include "thread_pool.h"



class tcp_server{

public:
	tcp_server();
	tcp_server(event_loop *loop, const char *ip, uint16_t port); 	//带参构造函数
	~tcp_server();	//析构

	void do_accept(); 	// 提取客户端连接


	//---- 消息分发路由 ----
	static msg_router router;

	//注册消息路由回调函数
	void add_msg_router(int msgid, msg_callback *cb, void *user_data = NULL){
		router.regist_msg_router(msgid, cb, user_data);
	}

	
	//获取当前server的线程池,得到内部接口send_task供开发者使用
    thread_pool *get_thread_pool() {
        return _thread_pool;
    }

private:
	int _sockfd;	//套接字
	struct sockaddr_in _connaddr;	//客户端链接地址
	socklen_t _addrlen;	// 客户端链接地址长度
	
	//event_loop epoll事件机制
    event_loop* _loop;

	// 连接的管理以及限制
// 定义静态属性和方法的目的是让所有的类的实例共享数据和资源

public:

	static void increase_conn(int connfd, tcp_conn *conn);	// 来新的连接，连接数增加
	static void decrease_conn(int connfd);	// 断开连接，连接数减少

	static void get_conn_num(int *cur_conn);	// 获取当前在线的连接数

	static tcp_conn* *conns;	// 全部已经在线的连接信息


	 // ------- 创建链接/销毁链接 Hook 部分 -----
	// 设置的函数的调用时机在连接一旦创建成功/连接一旦销毁，就立即调用 
	static void set_conn_start(conn_callback cb, void *args = NULL){
		conn_start_cb = cb;
		conn_start_cb_args = args;
	 }

	static void set_conn_close(conn_callback cb, void *args = NULL){
		conn_close_cb = cb;
		conn_close_cb_args = args;
	 }

	 //创建链接之后要触发的 回调函数
	static conn_callback conn_start_cb;
	static void *conn_start_cb_args;

	//销毁链接之前要触发的 回调函数
	static conn_callback conn_close_cb;
	static void *conn_close_cb_args;
	 

private:
	static int _max_conns;	// 客户端连接的最大数量
	static int _cur_conns;	// 客户端当前的连接数量
	
	static pthread_mutex_t _conns_mutex;	// 保护_cur_conn修改的锁

	//线程池
    thread_pool *_thread_pool;
};

