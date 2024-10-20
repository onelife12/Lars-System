#pragma once
/*
 * 定义一些IO复用机制或者其他异常触发机制的事件封装
 *	定义一个事件拥有的基本成员
 * */

class event_loop;

// 定义IO事件触发的回调函数, io_callback是一个函数指针, 两种定义方式，理解为定义函数类型
// typedef void (*io_callback)(event_loop *loop, int fd, void *args);
typedef void io_callback(event_loop *loop, int fd, void *args);

struct io_event{
	io_event() : read_callback(NULL), write_callback(NULL), rcb_args(NULL), wcb_args(NULL){
	
}
// 一个io_event对象应该包含 一个epoll的事件标识EPOLLIN/EPOLLOUT,和对应事件的处理函数read_callback,write_callback。他们都应该是io_callback类型。然后对应的函数形参。
	int mask; // EPOLLIN, EPOLLOUT
	io_callback *read_callback;
	io_callback *write_callback;
	void *rcb_args;
	void *wcb_args;
};


