#include <string>
#include <string.h>
#include "config_file.h"
#include "tcp_server.h"
#include "echoMessage.pb.h"

void callback_busi(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data){
	// 收到数据后拆包和组包进行发送
	qps_test::EchoMessage response, request;
	
	//解包，确保data[0-len]是一个完整包
	request.ParseFromArray(data, len);

	// 组装pb数据包，进行发送
	response.set_id(request.id());
	response.set_content(request.content());

	//序列化
	std::string responseString;
	response.SerializeToString(&responseString);

	conn->send_message(responseString.c_str(), responseString.size(), msgid);
}

int main(){
	event_loop loop;

	//加载配置文件
    config_file::setPath("../../conf/reactor.conf");
    std::string ip = config_file::instance()->GetString("reactor", "ip", "0.0.0.0");
    short port = config_file::instance()->GetNumber("reactor", "port", 8888);
	
	printf("ip = %s, port = %d\n", ip.c_str(), port);

    tcp_server server(&loop, ip.c_str(), port);

	server.add_msg_router(1, callback_busi);

	loop.event_process();

	return 0;
}

