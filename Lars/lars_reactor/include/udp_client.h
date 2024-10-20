#pragma once

#include "net_connection.h"
#include "message.h"
#include "event_loop.h"

class udp_client : public net_connection{

public:

	udp_client(event_loop *loop, const char *ip, uint16_t port);

	~udp_client();

	void do_read();

	void add_msg_router(int msgid, msg_callback *msg_cb, void *user_data = NULL);

	virtual int send_message(const char *data, int msglen, int msgid);
	
	int get_fd(){
		return _sockfd;
	}
	

private:
	
	int _sockfd;

	char _read_buf[MESSAGE_LENGTH_LIMIT];
	char _write_buf[MESSAGE_LENGTH_LIMIT];

	msg_router _router;

	event_loop *_loop;
};




