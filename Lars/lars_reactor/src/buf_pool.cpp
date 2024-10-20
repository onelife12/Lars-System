#include <cstdio>
#include <cstring>
#include "buf_pool.h"
#include <assert.h>
#include <unistd.h>
buf_pool* buf_pool::_instance = NULL;
pthread_once_t buf_pool::_once = PTHREAD_ONCE_INIT;
pthread_mutex_t buf_pool::_mutex = PTHREAD_MUTEX_INITIALIZER;


//构造函数 主要是预先开辟一定量的空间
//这里buf_pool是一个hash，每个key都是不同空间容量
//对应的value是一个io_buf集合的链表
//buf_pool -->  [m4K] -- io_buf-io_buf-io_buf-io_buf...
//              [m16K] -- io_buf-io_buf-io_buf-io_buf...
//              [m64K] -- io_buf-io_buf-io_buf-io_buf...
//              [m256K] -- io_buf-io_buf-io_buf-io_buf...
//              [m1M] -- io_buf-io_buf-io_buf-io_buf...
//              [m4M] -- io_buf-io_buf-io_buf-io_buf...
//              [m8M] -- io_buf-io_buf-io_buf-io_buf...

buf_pool::buf_pool() : _total_mem(0){
	
	make_io_buf_list(m4K, 5000);
	make_io_buf_list(m16K, 1000);
	make_io_buf_list(m64K, 500);
	make_io_buf_list(m256K, 200);
	make_io_buf_list(m1M, 50);
	make_io_buf_list(m4M, 20);
	make_io_buf_list(m8M, 10);

}


void buf_pool::make_io_buf_list(int cap, int num){
	io_buf *prev;

	pool[cap] = new io_buf(cap); // 分配一块size大小的内存块出来
	if(!pool[cap]){
		fprintf(stderr, "new io_buf %d error\n", cap);
		exit(1);
	}
	prev = pool[cap]; // 头节点
	for(int i = 0; i < num; i++){
		prev->next = new io_buf(cap);
		if (prev->next == NULL) {
			fprintf(stderr, "new inside io_buf %d error", cap);
			exit(1);
		}
		prev = prev->next;
	}
	_total_mem += cap / 1024 * num;

	
}



//开辟一个io_buf
//1 如果上层需要N个字节的大小的空间，找到与N最接近的buf hash组，取出，
//2 如果该组已经没有节点使用，可以额外申请
//3 总申请长度不能够超过最大的限制大小 EXTRA_MEM_LIMIT
//4 如果有该节点需要的内存块，直接取出，并且将该内存块从pool摘除
io_buf* buf_pool::alloc_buf(int N){
	//1 找到N最接近哪hash 组
    int index;
    if (N <= m4K) {
        index = m4K;
    }
    else if (N <= m16K) {
        index = m16K;
    }
    else if (N <= m64K) {
        index = m64K;
    }
    else if (N <= m256K) {
        index = m256K;
    }
    else if (N <= m1M) {
        index = m1M;
    }
    else if (N <= m4M) {
        index = m4M;
    }
    else if (N <= m8M) {
        index = m8M;
    }
    else {
        return NULL;
    }


	pthread_mutex_lock(&_mutex);
	if(pool[index] == NULL){
		if (_total_mem + index/1024 >= EXTRA_MEM_LIMIT) {
            //当前的开辟的空间已经超过最大限制
            fprintf(stderr, "already use too many memory!\n");
            exit(1);
        }

		io_buf *new_buf = new io_buf(index);
		if (new_buf == NULL) {
            fprintf(stderr, "new io_buf error\n");
            exit(1);
        }
			
		_total_mem += index / 1024;
		pthread_mutex_unlock(&_mutex);
		return new_buf;

	}

	//池中还有能分配的内存， 从池中摘下index对应的内存块
	io_buf *target = pool[index];
	pool[index] = target->next;


	pthread_mutex_unlock(&_mutex);
	target->next = NULL;

	return target;
	

}


void buf_pool::revert(io_buf *buffer){

	int index = buffer->capacity;
	buffer->clear();	// 清空缓冲区内容,重置head和length

	pthread_mutex_lock(&_mutex);
	//找到对应的hash组 buf首届点地址
    assert(pool.find(index) != pool.end());

	buffer->next = pool[index];
	pool[index] = buffer;
	
	pthread_mutex_unlock(&_mutex);

}
