#include "tcp_server.h"
#include "config_file.h"
#include <string>
#include <cstring>

tcp_server *server;

// 定义普通消息业务
void print_lars_task(event_loop *loop, void *args){
	printf("======= Active Task Func! ========\n");
	
	listen_fd_set fds;
	loop->get_listen_fds(fds);//这里是获取当前loop下监听的全部的fd的集合, 不同线程的loop，返回的fds是不同的

	 //可以向所有fds都发这个消息业务，但是并不是每个fds对应的loop都会执行消息业务，因为？
    listen_fd_set::iterator it;
	// 遍历fds
	for(it = fds.begin(); it != fds.end(); it++){
		int fd = *it;
		tcp_conn *conn = tcp_server::conns[fd];
		if (conn != NULL) {
            int msgid = 404;
            const char *msg = "Hello I am a Task!";
            conn->send_message(msg, strlen(msg), msgid);
        }

	}
}

//定义一些业务回调函数，将其和msgid进行绑定，然后注册
// 回显业务
void echo_busi(const char *data, uint32_t len, int msgid, net_connection *net_conn, void *user_data){
	printf("echo_busi...\n");
	net_conn->send_message(data, len, msgid);
	
//	printf("conn param = %s\n", (const char *)net_conn->param);
}

// 打印业务
void print_busi(const char *data, uint32_t len, int msgid, net_connection *net_conn, void *user_data)
{
	printf("recv from client: [%s]\n", data);
	printf("msgid: [%d]\n", msgid);
    printf("len: [%d]\n", len);
}

// 新的客户端链接进来,服务器主动发送一个消息业务，然后客户端针对消息ID做一个业务回调，如打印业务
void after_conn_succ(net_connection *conn, void *args){
	int msgid = 101;
	const char *msg = "welcome FHY client!...";
	conn->send_message(msg, strlen(msg), msgid);

	 //将当前的net_connection 绑定一个自定义参数，供我们开发者使用
//    const char *conn_param_test = "I am the conn for you!";
//    conn->param = (void*)conn_param_test;
	
	//创建链接成功之后触发任务
	server->get_thread_pool()->send_task(print_lars_task);
}

void before_conn_close(net_connection *conn, void *args){
	printf("connection close .... !\n");
}


int main(int argc, char* argv[]){
	event_loop loop;


	// 加载配置文件
	config_file::setPath("../../conf/reactor.conf");
	std::string ip = config_file::instance()->GetString("reactor", "ip", "0.0.0.0");
	short port = config_file::instance()->GetNumber("reactor", "port", 8888);

	printf("ip = %s, port = %d\n", ip.c_str(), port);

	server = new tcp_server(&loop, ip.c_str(), port);

	// 为server注册消息业务路由
	server->add_msg_router(1, echo_busi);
	server->add_msg_router(2, print_busi);

	//注册链接hook回调
	server->set_conn_start(after_conn_succ);
	server->set_conn_close(before_conn_close);	

	loop.event_process();
	return 0;
	
}
