#pragma once 
#include <ext/hash_map>
#include "io_buf.h"

typedef __gnu_cxx::hash_map<int, io_buf *> pool_t;

enum MEM_CAP{
	m4K  	= 4096,
	m16K    = 16384,
    m64K    = 65536,
    m256K   = 262144,
    m1M     = 1048576,
    m4M     = 4194304,
    m8M     = 8388608
};

#define EXTRA_MEM_LIMIT (5U * 1024 * 1024)

class buf_pool{

public:
	static void init(){
		_instance = new buf_pool();
	}

	static buf_pool* get_instance(){
		pthread_once(&_once, init);
		return _instance;
	}

	io_buf* alloc_buf(int N);// 根据需要的内存查找内存块链表
	io_buf* alloc_buf(){
		return alloc_buf(m4K);	//默认分配m4k
	}

	void revert(io_buf *buffer);	//将使用完了的内存块放入对应的内存链表中

	void make_io_buf_list(int cap, int num); // cap为内存块大小(刻度)，num是内存块的数量

private:
	buf_pool();

	buf_pool(const buf_pool &);

	const buf_pool& operator=(const buf_pool &);

	pool_t pool;
	uint64_t _total_mem;	// 内存池总容量
	
	static buf_pool *_instance;
	static pthread_once_t _once;

	static pthread_mutex_t _mutex;

	
};
