#include "lars_reactor.h"
#include "mysql.h"
#include "dns_route.h"
#include "lars.pb.h"
#include "subscribe.h"


tcp_server *server;
typedef hash_set<uint64_t> client_sub_mod_list;	// 记录下当前客户端订阅了的mod信息，给当前连接绑定这样一个属性

//订阅route 的modid/cmdid
void create_subscribe(net_connection * conn, void *args)
{
    conn->param = new client_sub_mod_list;
}

//退订route 的modid/cmdid,将当前连接请求过的mod都清除
void clear_subscribe(net_connection * conn, void *args)
{
    client_sub_mod_list::iterator it;
    client_sub_mod_list *sub_list = (client_sub_mod_list*)conn->param;

    for (it = sub_list->begin(); it  != sub_list->end(); it++) {
        uint64_t mod = *it;
        SubscribeList::instance()->unSubscribe(mod, conn->get_fd());
    }

    delete sub_list;

    conn->param = NULL;
}


// 实现针对ID_GetRouteRequest消息指令的业务处理.
void get_request(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data){

	// 解析proto包
	lars::GetRouteRequest req;

	req.ParseFromArray(data, len);

	// 得到modid, cmdid
	int modid = req.modid();
	int cmdid = req.cmdid();

	// 根据conn携带的param自定义参数，判断当前连接是否订阅过该mod模块，不重复订阅
	uint64_t mod = ((uint64_t)modid << 32) + cmdid;
	client_sub_mod_list *sub_list = (client_sub_mod_list *)conn->param;
	if(sub_list == NULL){
		fprintf(stderr, "sub_list is NULL\n");
	}else if(sub_list->find(mod) == sub_list->end()){
		// 说明当前mod是没有被订阅的，则订阅
		sub_list->insert(mod);
		// 订阅，绑定fd和mod的关系
		SubscribeList::instance()->subscribe(mod, conn->get_fd());
	}

	

	//3. 根据modid/cmdid 获取 host信息
	host_set hosts = Route::instance()->get_hosts(modid, cmdid);

	//4. 将数据打包成protobuf
	lars::GetRouteResponse rsp;
	rsp.set_modid(modid);
	rsp.set_cmdid(cmdid);

	for(host_set_it it = hosts.begin(); it != hosts.end(); it++){
		uint64_t ip_port = *it;
		lars::HostInfo host;
		host.set_ip((uint32_t)(ip_port >> 32));
		host.set_port((int)ip_port);

		rsp.add_host()->CopyFrom(host);
	}

	//5. 发送给客户端
	std::string responseString;
	rsp.SerializeToString(&responseString);
	conn->send_message(responseString.c_str(), responseString.size(), lars::ID_GetRouteResponse);
		
}



int main(int argc, char *argv[]){
	
	event_loop loop;

	//加载配置文件
	config_file::setPath("conf/dns.conf");
	std::string ip = config_file::instance()->GetString("reactor", "ip", "0.0.0.0");
    short port = config_file::instance()->GetNumber("reactor", "port", 7778);

	//创建tcp服务器
    server = new tcp_server(&loop, ip.c_str(), port);

	//注册路由业务
	
	 //==========注册链接创建/销毁Hook函数============
    server->set_conn_start(create_subscribe);
    server->set_conn_close(clear_subscribe);
  	//============================================
  	
	server->add_msg_router(lars::ID_GetRouteRequest, get_request);

	//开辟backend thread 周期性检查db数据库route信息的更新状态
	pthread_t tid;
	int ret = pthread_create(&tid, NULL, check_route_changes, NULL);
	if (ret == -1) {
        perror("pthread_create backendThread");
        exit(1);
    }
	pthread_detach(tid);


    //开始事件监听    
    printf("lars dns service run at ip : %s, port : %d....\n", ip.c_str(), port);
    loop.event_process();

    return 0;

}
