#include "lars_reactor.h"
#include "lars.pb.h"
#include "store_report.h"

struct Args{
	thread_queue<lars::ReportStatusRequest>* first;
    StoreReport *second;
};

//typedef void io_callback(event_loop *loop, int fd, void *args);
void thread_report(event_loop *loop, int fd, void *args){
	//1. 从queue里面取出需要report的数据(需要thread_queue)
    thread_queue<lars::ReportStatusRequest>* queue = ((Args*)args)->first;
    StoreReport *sr = ((Args*)args)->second;

	std::queue<lars::ReportStatusRequest> report_msg;

	//1.1 从消息队列中取出全部的消息元素集合
	queue->recv(report_msg);
	while(!report_msg.empty()){
		lars::ReportStatusRequest msg = report_msg.front();
        report_msg.pop();

		//2. 将数据存储到DB中(需要StoreReport)
        sr->store(msg);
	}


}

void* store_main(void *args){
	 //得到对应的thread_queue
    thread_queue<lars::ReportStatusRequest> *queue = (thread_queue<lars::ReportStatusRequest>*)args;

	event_loop loop;

	//定义一个存储对象
	StoreReport sr;

	Args callback_args;
    callback_args.first = queue;
    callback_args.second = &sr;

	queue->set_loop(&loop);
	queue->set_callback(thread_report, &callback_args);

	loop.event_process();

	return NULL;
}
