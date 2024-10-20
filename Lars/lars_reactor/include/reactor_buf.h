#pragma once
#include <assert.h>
#include <unistd.h>
#include <cstdio>
#include <string.h>

#include "buf_pool.h"
#include "io_buf.h"
class reactor_buf{

public:
	reactor_buf();
	~reactor_buf();

	const int length() const;
	void pop(int len);
	void clear();

protected:
	io_buf *_buf;
	

};


class input_buf : public reactor_buf {
public:
    int read_data(int fd);
 
    const char* data() const;
 
    void adjust();
};


class output_buf : public reactor_buf{
	
public:
	// 先将数据存到内存块，然后再从内存块发送到fd
	int send_data(const char *data, int datalen);

	int write2fd(int fd);
};

