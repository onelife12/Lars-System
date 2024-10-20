#pragma once

#include <vector>
#include <pthread.h>
#include <ext/hash_set>
#include <ext/hash_map>
#include "lars_reactor.h"
#include "lars.pb.h"
#include "dns_route.h"


using namespace __gnu_cxx;

/* 定义订阅列表关系类型，key->modid/cmdid， value->fds(订阅的客户端文件描述符)，记录当前模块有多少fd在使用，将fd存起来
 * 当一个fd使用了modid/cmdid，就代表它订阅过这个模块
*/
typedef hash_map<uint64_t, hash_set<int>> subscribe_map;

/*定义发布列表，key->fd(订阅客户端的文件描述符), value->modids，记录当前fd访问下的有变更的modid/cmdid
 *当modid/cmdid下的信息变更时，那么要通知订阅过它的fds，做出相应的更改，不然会访问出错
 */
typedef hash_map<int, hash_set<uint64_t>> publish_map;


class SubscribeList{

public:

	static void init(){
		_instance = new SubscribeList();
	}
	static SubscribeList *instance(){
		pthread_once(&_once, init);
		return _instance;
	}

	// 订阅
	void subscribe(uint64_t mod, int fd);

	// 取消订阅
	void unSubscribe(uint64_t mod, int fd);

	// 发布
	 void publish(std::vector<uint64_t> &change_mods);

	//根据在线用户fd得到需要发布的列表
	 void make_publish_map(listen_fd_set &online_fds, publish_map &need_pub);
		
	// 消息全发完后，清空pub_list的内容
	publish_map *get_pub_list(){
		return &pub_list;
	}


private:

	SubscribeList();
	SubscribeList(const SubscribeList &);
	SubscribeList & operator=(const SubscribeList &);
	
	static SubscribeList *_instance;
	static pthread_once_t _once;

	subscribe_map sub_list;
	pthread_mutex_t sub_mutex;

	publish_map pub_list;
	pthread_mutex_t pub_mutex;

};



