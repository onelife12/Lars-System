#pragma once

#include <ext/hash_map>

struct msg_head{
	int msgid;
	int msglen;
};

#define MESSAGE_HEAD_LEN 8
#define MESSAGE_LENGTH_LIMIT (65535 - MESSAGE_HEAD_LEN)

// msg 业务回调函数原型
//为开发者提供回调业务注册API，可以自定义自己的业务处理函数，名为消息路由分发，根据不同的消息，注册不同的函数
class net_connection;
typedef void msg_callback(const char *data, uint32_t len, int msgid, net_connection *net_conn, void *user_data);

// 消息路由分发机制
class msg_router{

public:

	msg_router() : _router(), _args(){

		printf("msg router init ...\n");
	}

	//给一个消息ID注册一个对应的回调业务函数
	int regist_msg_router(int msgid, msg_callback *msg_cb, void *user_data){
		if(_router.find(msgid) != _router.end()){
			// 回调已存在，则退出
			return -1;
		}
		// 该msgid不存在回调函数,就给msgid注册回调函数
		printf("add msg cb msgid = %d\n", msgid);
		_router[msgid] = msg_cb;
		_args[msgid] = user_data;
			
		return 0;
	}

	// 调用为msgid注册的对应的回调函数
	void call(int msgid, uint32_t msglen, const char *data, net_connection *net_conn){
		printf("=========call msgid = %d=======\n", msgid);
		if(_router.find(msgid) == _router.end()){
			fprintf(stderr, "msgid %d is not register!\n", msgid);
            return;
		}
		//直接取出回调函数，执行
		msg_callback *callback = _router[msgid]; 	// 从集合中取出为msgid绑定的回调函数
		void *user_data = _args[msgid];		// 从集合中取出为msgid绑定的参数
		callback(data, msglen, msgid, net_conn, user_data);
		printf("=======msgid：%d 的业务调用成功!========\n", msgid);

	}

private:

	 //针对消息的路由分发，key为msgID, value为注册的回调业务函数
	__gnu_cxx::hash_map<int, msg_callback *> _router;	// msgid 和 msg_callback的绑定，映射
	// 回调业务函数对应的参数，key为msgID, value为对应的参数
	__gnu_cxx::hash_map<int, void *> _args;	
};











