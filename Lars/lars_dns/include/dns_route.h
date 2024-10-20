#pragma once
#include <pthread.h>
#include <ext/hash_map>
#include <ext/hash_set>
#include "mysql.h"

using __gnu_cxx::hash_map;
using __gnu_cxx::hash_set;

//定义用来保存modID/cmdID与host的IP/host的port的对应的关系 数据类型
typedef hash_map<uint64_t, hash_set<uint64_t>> route_map;
typedef hash_map<uint64_t, hash_set<uint64_t>>::iterator route_map_it;

//定义用来保存host的IP/host的port的集合的 数据类型
typedef hash_set<uint64_t> host_set;
typedef hash_set<uint64_t>::iterator host_set_it;


class Route{

public:
	
	static void init(){
		_instance = new Route();
	}

	static Route* instance(){
		pthread_once(&_once, init);
		return _instance;
	}

	// 链接数据库
	void connect_db();

	// 查询数据库，创建_data_pointer 与 _temp_pointer 两个map
	void build_maps();	

	//获取modid/cmdid对应的host信息
	host_set get_hosts(int modid, int cmdid);

	// 获取当前版本,从而判断数据是否有改变
	int load_version();

	//加载RouteData到_temp_pointer
	int load_route_data();

	//将temp_pointer的数据更新到data_pointer
	void swap();

	//加载RouteChange得到修改的modid/cmdid
	void load_changes(std::vector<uint64_t> &change_list);

	//删除RouteChange的全部修改记录数据,remove_all为全部删除
	//否则默认删除当前版本之前的全部修改,清空之前的修改记录
	void remove_changes(bool remove_all);

private:
	//构造函数私有化
	Route();
	Route(const Route &);
	const Route& operator=(const Route &);
	
	//单例	
	static Route* _instance;
	// 单例锁
	static pthread_once_t _once;

	/* ---- 属性 ---- */
    //数据库
	
	MYSQL _db_conn;
	char _sql[1000];

	/* key: modid/cmdid ---> value: host(ip/port) 对应的route关系map*/
	/*_data_pointer是dns服务每次查询这种映射关系所需的表，_temp_pointer是当数据库中数据更改，先加载到这个表，然后再更新到_data_pointer中*/
	route_map *_data_pointer;	
	route_map *_temp_pointer;

	pthread_rwlock_t _map_lock; // 读写锁
	
	// 版本号
	long _version;
};

void* check_route_changes(void *args);

