#include <unistd.h>
#include <string.h>
#include "lars.pb.h"
#include "lars_reactor.h"
#include <string>


struct Option{

	Option():ip(nullptr), port(0){

	}

	char *ip;
	short port;
};

Option option;

void Usage() {
    printf("Usage: ./dns_test -h ip -p port\n");
}

void parseOption(int argc, char ** argv){
	
	for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            option.ip = argv[i + 1];
        }
        else if (strcmp(argv[i], "-p") == 0) {
            option.port = atoi(argv[i + 1]);
        }
    }

    if ( !option.ip || !option.port ) {
        Usage();
        exit(1);
    }
}

// hook函数，建立连接后发一个消息
void after_conn(net_connection *conn, void *args){
	// 发送Route请求，请求modid/cmdid对应的主机信息
	lars::GetRouteRequest req;
	req.set_modid(1);
	req.set_cmdid(1);

	std::string reqString;
	req.SerializeToString(&reqString);

	conn->send_message(reqString.c_str(), reqString.size(), lars::ID_GetRouteRequest);
}

// 客户端收到pb包的回调
void deal_get_route(const char *data, uint32_t len, int msgid, net_connection *conn, void *args){
	// 解析得到的数据
	lars::GetRouteResponse rsp;
	rsp.ParseFromArray(data, len);

	//打印数据
    printf("modid = %d\n", rsp.modid());
    printf("cmdid = %d\n", rsp.cmdid());
    printf("host_size = %d\n", rsp.host_size());

	for (int i = 0; i < rsp.host_size(); i++) {
        printf("-->ip = %u\n", rsp.host(i).ip());
        printf("-->port = %d\n", rsp.host(i).port());
    }

	//封装数据包再次请求
/*    lars::GetRouteRequest req;
    req.set_modid(rsp.modid());
    req.set_cmdid(rsp.cmdid());
    std::string reqString;

	req.SerializeToString(&reqString);
	conn->send_message(reqString.c_str(), reqString.size(), lars::ID_GetRouteRequest);
*/
}

int main(int argc, char **argv){

//	parseOption(argc, argv);

    event_loop loop;
  //  tcp_client *client = new tcp_client(&loop, option.ip, option.port);
	tcp_client *client = new tcp_client(&loop, "127.0.0.1", 7778);
	if (client == NULL) {
        fprintf(stderr, "client == NULL\n");
        exit(1);
    }

	//客户端成功建立连接，首先发送请求包
    client->set_conn_start(after_conn);

    //设置服务端回应包处理业务
    client->add_msg_router(lars::ID_GetRouteResponse, deal_get_route);

	loop.event_process();

	return 0;
}




