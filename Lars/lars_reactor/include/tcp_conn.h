#pragma once 

#include "event_loop.h"
#include "reactor_buf.h"
#include "net_connection.h"

class tcp_conn : public net_connection{

public:
	tcp_conn(int connfd, event_loop *loop);

	void do_read();	// 从读缓冲区中读取数据，input_buf

	void do_write();  // 将output_buf中的数据发给服务器

	void clean_conn();

	// 给服务器发消息的方法，发送到写缓冲区后，通过do_write发送给服务器
	int send_message(const char *data, int msglen, int msgid);
	int get_fd(){
		return _connfd;
	}
private:
	int _connfd;

	event_loop *_loop;
	
	output_buf obuf;
	
	input_buf ibuf;
};
