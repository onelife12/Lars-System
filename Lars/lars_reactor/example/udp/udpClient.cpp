#include "udp_client.h"
#include <cstring>
#include <string>

void busi(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data){
	//得到服务端回执的数据
/*    char *str = NULL;

    str = (char*)malloc(len+1);
    memset(str, 0, len+1);
    memcpy(str, data, len);
*/
	std::string str = std::string(data, len);
    printf("recv server: [%s]\n", str.c_str());
//    printf("recv server: [%s]\n", str);
    printf("msgid: [%d]\n", msgid);
    printf("len: [%d]\n", len);
}

int main(int argc, char *argv[]){
	
	event_loop loop;

	udp_client client(&loop, "127.0.0.1", 9999);

	//注册消息路由业务
    client.add_msg_router(1, busi);

	int msgid = 1;
	const char *msg = "hello server!";
	client.send_message(msg, strlen(msg), msgid);
	
	loop.event_process();

	return 0;
}
