#pragma once 

#include <netinet/in.h>
#include "event_loop.h"
#include "net_connection.h"
#include "message.h"

class udp_server : public net_connection{

public:
	udp_server(event_loop *loop, const char *ip, uint16_t port);

	virtual int send_message(const char *data, int msglen, int msgid);

	int get_fd(){
		return _sockfd;
	}

	// 注册消息路由回调函数
	void add_msg_router(int msgid, msg_callback *msg_cb, void *user_data = NULL);	

	~udp_server();

	// 处理消息业务
	void do_read();


private:
	int _sockfd;

	event_loop *_loop;

	char _read_buf[MESSAGE_LENGTH_LIMIT];
	char _write_buf[MESSAGE_LENGTH_LIMIT];

	//用于连接服务器端
	struct sockaddr_in _cli_addr;
	socklen_t _cli_len;
	
	// 消息路由分发
	msg_router _router;
};
