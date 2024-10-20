#pragma once

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>  //<netinet/in.h> 主要用于定义套接字地址结构和字节序转换函数
#include <arpa/inet.h>	//而 <arpa/inet.h> 则主要用于IP地址的字符串表示和二进制表示之间的转换。
#include <string.h>
#include <cstdio>
#include "io_buf.h"
#include "event_loop.h"
#include "message.h"
#include "reactor_buf.h"

#include "net_connection.h"



class tcp_client : public net_connection{

public:
	tcp_client(event_loop *loop, const char *ip, unsigned short int port);

	int send_message(const char *data, int msglen, int msgid);
	
	int get_fd(){
		return _sockfd;
	}

	void do_connect();

	void do_read();

	void do_write();

	void clean_conn();

	~tcp_client();

	// 设置业务处理回调函数
	/*void set_msg_callback(msg_callback *msg_cb){
		this->_msg_callback = msg_cb;
	}*/

	 //注册消息路由回调函数
	 void add_msg_router(int msgid, msg_callback *msg_cb, void *user_data = NULL){
		 _router.regist_msg_router(msgid, msg_cb, user_data);	// 注册消息的回调函数
	 }

	//----- 链接创建/销毁回调Hook ----
    //设置链接的创建hook函数
    void set_conn_start(conn_callback cb, void *args = NULL) 
    {
        _conn_start_cb = cb;
        _conn_start_cb_args = args;
    }

    //设置链接的销毁hook函数
    void set_conn_close(conn_callback cb, void *args = NULL) {
        _conn_close_cb = cb;
        _conn_close_cb_args = args;
    }
    
    //创建链接之后要触发的 回调函数
    conn_callback _conn_start_cb;     
    void * _conn_start_cb_args;

    //销毁链接之前要触发的 回调函数
    conn_callback _conn_close_cb;
    void * _conn_close_cb_args;
    // ---------------------------------

	input_buf ibuf;
	output_buf obuf;

	struct sockaddr_in serv_addr;
	socklen_t _addrlen;

private:	
	int _sockfd;
	

	event_loop *_loop;

	// 客户端处理服务器业务的回调函数,单路由模式，只有一个回调业务
	//msg_callback *_msg_callback;
	
	 //处理消息的分发路由
    msg_router _router;
};
