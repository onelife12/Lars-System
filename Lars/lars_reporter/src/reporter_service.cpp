#include "lars_reactor.h"
#include "lars.pb.h"
#include "store_report.h"
#include <string>

thread_queue<lars::ReportStatusRequest> **reportQueues = NULL;
int thread_cnt = 0;

void get_report_status(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data){
	lars::ReportStatusRequest req;
	
	req.ParseFromArray(data, len);

	/*// 将上报数据存储到数据库，agent给reporter上报	
	StoreReport sr;
	sr.store(req);
*/
	//轮询将消息平均发送到每个线程的消息队列中
    static int index = 0;
	reportQueues[index]->send(req);
	index++;
	index = index % thread_cnt;
}

void create_reportdb_threads(){
	thread_cnt = config_file::instance()->GetNumber("reporter", "db_thread_cnt", 3);
	// 开辟线程池对应的消息队列
	reportQueues = new thread_queue<lars::ReportStatusRequest>* [thread_cnt];
	
	if (reportQueues == NULL) {
        fprintf(stderr, "create thread_queue<lars::ReportStatusRequest>*[%d], error", thread_cnt) ;
        exit(1);
    }

	for(int i = 0; i < thread_cnt; i++){
		// 给当前线程创建一个消息队列queue
		reportQueues[i] = new thread_queue<lars::ReportStatusRequest>();

		if (reportQueues == NULL) {
            fprintf(stderr, "create thread_queue error\n");
            exit(1);
        }
		pthread_t tid;
		int ret = pthread_create(&tid, NULL, store_main, reportQueues[i]);
		if (ret == -1)  {
            perror("pthread_create");
            exit(1);
        }

		pthread_detach(tid);
	}

}

int main(){
	
	event_loop loop;

	config_file::setPath("conf/reporter.conf");
	std::string ip = config_file::instance()->GetString("reactor", "ip", "0.0.0.0");
    short port = config_file::instance()->GetNumber("reactor", "port", 7779);
	
	tcp_server server(&loop, ip.c_str(), port);

	//当agent给reporter发一个reportstatusrequest时，reporter给这个请求提供服务，添加数据上报请求处理的消息分发处理业务
	server.add_msg_router(lars::ID_ReportStatusRequest, get_report_status);

	 //为了防止在业务中出现io阻塞，那么需要启动一个线程池对IO进行操作，接受业务的请求存储消息
    create_reportdb_threads();

	loop.event_process();

	return 0;
}



