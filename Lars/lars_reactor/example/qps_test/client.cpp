#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>

#include "tcp_client.h"
#include "echoMessage.pb.h"

struct Qps
{
    Qps() {
        last_time = time(NULL);
        succ_cnt = 0;
    }

    long last_time;//最后一次发包时间 ms为单位
    int succ_cnt; //成功收到服务器回显的次数
};

void busi(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data){
	Qps *qps = (Qps *)user_data;
	
	qps_test::EchoMessage request, response;

	if(response.ParseFromArray(data, len) == false){
		printf("server call back data error\n");
        return;
	}

	//判断数据内容是否回显一致
    if (response.content() == "hello server!") {
        //服务器请求响应成功一次
        qps->succ_cnt ++;
    }
	
	long current_time = time(NULL);
	if(current_time - qps->last_time >= 1){
		//如果当前时间比最后一次发包时间大于1秒，那么我们进行记录
		printf("---> qps = %d <----\n", qps->succ_cnt);
		qps->last_time = current_time;//记录最后时间
        qps->succ_cnt = 0;//清空成功次数
	}

	//给服务端发送新的请求
	request.set_id(response.id() + 1);
	request.set_content(response.content());

	std::string requestString;
	request.SerializeToString(&requestString);

	conn->send_message(requestString.c_str(), requestString.size(), msgid);

}

// 创建连接成功后，hook函数,组装一个pb包发送给服务端
void conn_start(net_connection *client, void *args){
	qps_test::EchoMessage request;
	request.set_id(1);
	request.set_content("hello server!");

	std::string requestString;
	request.SerializeToString(&requestString);

	int msgid = 1;	//与server端的消息路由一致
	client->send_message(requestString.c_str(), requestString.size(), msgid);

}

void *thread_main(void *args){
	// 给服务器不断地发包
	event_loop loop;

	tcp_client client(&loop, "127.0.0.1", 7777);

	Qps qps;

	// 设置业务回调
	client.add_msg_router(1, busi, (void *)&qps);

	// 设置连接成功HOOK函数
	client.set_conn_start(conn_start);

	loop.event_process();
	
	return NULL;
	
}

int main(int argc, char *argv[]){

	if(argc == 1){
		printf("Usage : ./client [threadNum]\n");
		return 1;
	}

	// 创建thread_num个线程
	int thread_num = atoi(argv[1]);
	pthread_t *tids;
	tids = new pthread_t[thread_num];

	for(int i = 0; i < thread_num; i ++){
		pthread_create(&tids[i], NULL, thread_main, NULL);
	}

	// 回收线程资源
	for(int i = 0; i < thread_num; i++){
		pthread_join(tids[i], NULL);
	}

	return 0;


}
