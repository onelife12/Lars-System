#pragma once

/*
 * 
 * 网络通信的抽象类，任何需要进行收发消息的模块，都可以实现该类
 *
 * */


	
class net_connection{

public:
	net_connection(){

	}

	virtual int send_message(const char *data, int msglen, int msgid) = 0;		// 声明为纯虚函数
	virtual int get_fd() = 0;
	
	void *param; // 将一些开发者自定义的参数和当前链接进行绑定

};


//创建链接/销毁链接 要触发的 回调函数类型
typedef void (*conn_callback)(net_connection *conn, void *args);







