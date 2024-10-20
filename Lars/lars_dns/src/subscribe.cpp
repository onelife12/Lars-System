#include "subscribe.h"

extern tcp_server *server;

// 单例对象
SubscribeList * SubscribeList::_instance = NULL;
pthread_once_t  SubscribeList::_once = PTHREAD_ONCE_INIT;

SubscribeList::SubscribeList(){
	pthread_mutex_init(&sub_mutex, NULL);

	pthread_mutex_init(&pub_mutex, NULL);
	

}

void SubscribeList::subscribe(uint64_t mod, int fd){
	pthread_mutex_lock(&sub_mutex);

	sub_list[mod].insert(fd);

	pthread_mutex_unlock(&sub_mutex);
}

void SubscribeList::unSubscribe(uint64_t mod, int fd){
	pthread_mutex_lock(&sub_mutex);

	if(sub_list.find(mod) != sub_list.end()){
		sub_list[mod].erase(fd);
		if(sub_list[mod].empty()){
			sub_list.erase(mod);
		}
	}

	pthread_mutex_unlock(&sub_mutex);
}

// 组装消息进行发送
void push_change_task(event_loop *loop, void *args){
	
	 SubscribeList *subscribe = (SubscribeList*)args;

	 //1 获取全部的在线客户端fd
    listen_fd_set online_fds;
    loop->get_listen_fds(online_fds);

	//2 从subscribe的pub_list中 找到与online_fds集合匹配，放在一个新的publish_map里
    publish_map need_publish;
    subscribe->make_publish_map(online_fds, need_publish);

	//3 依次从need_publish取出数据 发送给对应客户端链接
	for(auto pub = need_publish.begin(); pub != need_publish.end(); pub ++){
		int fd = pub->first;

		//遍历 fd对应的 modid/cmdid集合
		for(auto hs = pub->second.begin(); hs != pub->second.end(); hs++){
			int modid = int((*hs) >> 32);
			int cmdid = int(*hs);

			lars::GetRouteResponse rsp;
			rsp.set_modid(modid);
			rsp.set_cmdid(cmdid);

			//通过route查询对应的host ip/port信息 进行组装
			host_set hosts = Route::instance()->get_hosts(modid, cmdid);
			for(auto it = hosts.begin(); it != hosts.end(); it++){
				uint64_t ip_port = *it;
				lars::HostInfo host;
				host.set_ip((uint32_t)(ip_port >> 32));
				host.set_port((int)ip_port);

				rsp.add_host()->CopyFrom(host);
			}

			 //给当前fd 发送一个更新消息
            std::string rspString;
            rsp.SerializeToString(&rspString);

			//通过fd取出链接信息
			net_connection *conn = tcp_server::conns[fd];
			if(conn != NULL){
				conn->send_message(rspString.c_str(), rspString.size(), lars::ID_GetRouteResponse);
			}

			// 给当前fd发完消息后，将其从pub_list删除
			SubscribeList::instance()->get_pub_list()->erase(fd); 
		}
	}
}

/*若当前客户端订阅的模块中有变更且还在线就给他发
*/
void SubscribeList::make_publish_map(listen_fd_set &online_fds, publish_map &need_pub){
	
	pthread_mutex_lock(&pub_mutex);
	//遍历pub_list 找到 online_fds匹配的数据，放到need_publish中
	for(auto it = pub_list.begin(); it != pub_list.end(); it++){
		if(online_fds.find(it->first) != online_fds.end()){
		// 若fd在线，将当前关系移到need_pub中

		// XXX 这里是直接指针赋值，两个指针指向同一块内存，释放了就没了，浅拷贝错误
/*			need_pub[it->first] = pub_list[it->first];
			//当该组数据从pub_list中删除掉
            pub_list.erase(it);
*/		// XXX 需要做成深拷贝
		for(auto st = pub_list[it->first].begin(); st != pub_list[it->first].end(); st++){
			need_pub[it->first].insert(*st);
		}	
		}
	}

	pthread_mutex_unlock(&pub_mutex);	

}


/*发布功能：这个被修改了的集合是由外部提供，当知道哪些模块被修改了，就要通知订阅了这些模块的客户端做出变更，
 *根据修改的mod，得到对应的fd，往发布列表填充数据，fd ---> mod
 *
*/
void SubscribeList::publish(std::vector<uint64_t> &change_mods){
	//1 将change_mods已经修改的mod对应的fd 放到 发布清单_push_list中 
	pthread_mutex_lock(&sub_mutex);
	pthread_mutex_lock(&pub_mutex);

	for(auto it = change_mods.begin(); it != change_mods.end(); it++){
		uint64_t mod = *it; // 根据这个mod去sub_list里去查fd,查到加入pub_list中

		if(sub_list.find(mod) != sub_list.end()){
			//将mod下面的fd_set集合拷迁移到 pub_list中
			for(auto fds_it = sub_list[mod].begin(); fds_it != sub_list[mod].end(); fds_it++){
				int fd = *fds_it;
				pub_list[fd].insert(mod);	// 建立fd ---> mod 对应关系，fd访问的哪些模块更改了
			}
		}

	}
	pthread_mutex_unlock(&pub_mutex);
	pthread_mutex_unlock(&sub_mutex);
	
	//2 通知各个线程去执行推送任务
	server->get_thread_pool()->send_task(push_change_task, this);

}


