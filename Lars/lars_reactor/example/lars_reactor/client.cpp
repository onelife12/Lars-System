#include "tcp_client.h"
#include <cstdio>
#include <cstring>

//客户端业务
void busi(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data)
{
    //得到服务端回执的数据
    printf("recv server: [%s]\n", data);
    printf("msgid: [%d]\n", msgid);
    printf("len: [%d]\n", len);
}

// 客户端连接上服务器后，主动发一个消息
void after_conn_succ(net_connection *conn, void *args){
	int msgid = 1;
	const char *msg = "hello server, I am FHY!";
	conn->send_message(msg, strlen(msg), msgid);


    int msgid2 = 2;
	const char *msg2 = "hello server, my name is FHY! nice to meet you!";
    conn->send_message(msg2, strlen(msg2), msgid2);
}
// 客户端销毁的回调
void before_conn_close(net_connection *conn, void *args){
	printf("before_conn_close...\n");
    printf("Client is lost!\n");
}



int main()
{
    event_loop loop;

    //创建tcp客户端
    tcp_client client(&loop, "127.0.0.1", 7777);

    //注册回调业务,通过set_message设置回调业务是单路由模式，只能绑定一个业务，通过router集合，可以绑定多个
    //client.set_msg_callback(busi);
	/*客户端和服务端的router定义不一样，因为服务端是所有客户端共享的，它的router定义为静态的，不依赖于对象实例而存在，直接通过类名访问
	 * 而客户端的router是依赖于对象的,定义为非静态成员
	
	*/
	
	/*client.add_msg_router(1, busi);		// 针对收到的不同的消息，定义不同的处理业务
	client.add_msg_router(101, busi);
	client.add_msg_router(404, busi);		
	client.add_msg_router(2, busi);	*/
	//设置hook函数
    client.set_conn_start(after_conn_succ);
    client.set_conn_close(before_conn_close);

    //开启事件监听
    loop.event_process();

    return 0;
}

