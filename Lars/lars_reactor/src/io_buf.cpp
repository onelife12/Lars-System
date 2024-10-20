#include <cstdio>
#include <cstring>
#include <assert.h>
#include "io_buf.h"

// 列表初始化时，注意初始化的顺序和头文件中定义的顺序要保持一致
io_buf::io_buf(int size) : head(0), length(0), capacity(size), next(NULL){
	data = new char[size];
	assert(data);
}

void io_buf::clear(){
	length = head = 0;
}

void io_buf::adjust(){
	if(head != 0){
		if(length != 0){
			memmove(data, data + head, length);
		}

		head = 0;
	}
}

void io_buf::copy(const io_buf * other){
	memcpy(data, other->data + other->head, other->length);
	head = 0;
	length = other->length;
}

void io_buf::pop(int len){
	length -= len;
	head += len;
}



