#include <cstdio>
#include <stdlib.h>
#include <string.h>
#include <strings.h>


#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <errno.h>


#include "tcp_server.h"
#include "reactor_buf.h"
#include "tcp_conn.h"	// 连接成功后为连接用一个类来进行管理
#include "config_file.h"


//连接资源管理
tcp_conn** tcp_server::conns = NULL;	// 用fd来索引当前连接

int tcp_server::_max_conns = 0;

int tcp_server::_cur_conns = 0;

pthread_mutex_t tcp_server::_conns_mutex = PTHREAD_MUTEX_INITIALIZER;

// 消息分发路由
msg_router tcp_server::router;

// 初始化 HOOK函数
conn_callback tcp_server::conn_start_cb = NULL;
conn_callback tcp_server::conn_close_cb = NULL;
void * tcp_server::conn_start_cb_args  = NULL;
void * tcp_server::conn_close_cb_args  = NULL;


// 新增一个新建的连接
void tcp_server::increase_conn(int connfd, tcp_conn *conn){
	pthread_mutex_lock(&_conns_mutex);
	
	conns[connfd] = conn;
	_cur_conns ++;	
	
	pthread_mutex_unlock(&_conns_mutex);
}

void tcp_server::decrease_conn(int connfd){
	pthread_mutex_lock(&_conns_mutex);

	conns[connfd] = NULL;
	_cur_conns --;
	
	pthread_mutex_unlock(&_conns_mutex);
}

void tcp_server::get_conn_num(int *cur_conn){
	pthread_mutex_lock(&_conns_mutex);
	
	*cur_conn = _cur_conns;
	
	pthread_mutex_unlock(&_conns_mutex);
}


//listen fd 客户端有新链接请求过来的回调函数
void accept_callback(event_loop *loop, int fd, void *args){
	tcp_server *server = (tcp_server *)args;
	server->do_accept();
}

tcp_server::tcp_server(event_loop *loop, const char *ip, uint16_t port){
	// 初始化服务器
	memset(&_connaddr, 0, sizeof(_connaddr));

	//忽略一些信号 SIGHUP, SIGPIPE
    //SIGPIPE:如果客户端关闭，服务端再次write就会产生
    //SIGHUP:如果terminal关闭，会给当前进程发送该信号
	if(signal(SIGHUP, SIG_IGN) == SIG_ERR){
		fprintf(stderr, "signal ignore SIGHUP\n");
	}
	if(signal(SIGPIPE, SIG_IGN) == SIG_ERR){
		fprintf(stderr, "signal ignore SIGPIPE\n");
	}

	_sockfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
	if(_sockfd == -1){
		fprintf(stderr, "tcp_server socket() error\n");
		perror("");
		exit(1);
	}
	struct sockaddr_in ser_addr;
	bzero(&ser_addr, sizeof(ser_addr));
	ser_addr.sin_family = AF_INET;
	ser_addr.sin_port = htons(port);
	inet_aton(ip, &ser_addr.sin_addr);

	// 设置端口复用
	int op = 1;
	if(setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &op, sizeof(op)) < 0){
		fprintf(stderr, "setsocketopt SO_REUESRADDR\n");
	}
	// 绑定
	if(bind(_sockfd, (struct sockaddr *)&ser_addr, sizeof(ser_addr)) < 0){
		perror("bind error!\n");
		exit(1);
	}
	// 监听
	if(listen(_sockfd, 128) == -1){
		perror("listen error!\n");
		exit(1);
	}


	//5 将_sockfd添加到event_loop中
    _loop = loop;


	// 6 创建线程池
	int thread_cnt = config_file::instance()->GetNumber("reactor", "threadNum", 3);	// 从配置文件读取

	if(thread_cnt > 0){
		_thread_pool = new thread_pool(thread_cnt);
		if (_thread_pool == NULL) {
            fprintf(stderr, "tcp_server new thread_pool error\n");
            exit(1);
        }
	}
	
	//7 =============  创建链接管理 ===============
	
	_max_conns = config_file::instance()->GetNumber("reactor", "maxConn", 1000);
	//创建链接信息数组
	/*这里如果开辟少了会栈溢出，因为new tcp_conn后，要统计conn的个数，是用的connfd来索引当前conn，
	 * 然而呢，在程序过程中，除了stdin, stdout, stderr会占用的文件描述符，
	 * 还有listenfd, 
	 * main-thread: epollfd, sub-thread: epollfd * threadcnt, thread_queue : _evfd * thread_num
	 * 等等，这些都会占用文件描述符	 
	 *  所以当conns的容量太小的话，索引就会越界, 就会栈溢出*/
	conns = new tcp_conn*[_max_conns + 5 + 2 * thread_cnt];	// 数组里的值为tcp_conn对象的指针
	if (conns == NULL) {
        fprintf(stderr, "new conns[%d] error\n", _max_conns);
        exit(1);
    }
		
	
	//8 注册_socket读事件-->accept处理
	//this 指向的是当前对象或者说当前类的实例，this 将指向调用 add_io_event 方法的对象的实例，也就是拥有 _loop 和 add_io_event 方法的那个类的实例。
    _loop->add_io_event(_sockfd, accept_callback, EPOLLIN, this);
}




//临时的收发消息
struct message{
    char data[m4K];
    char len;
};
struct message msg;

void server_rd_callback(event_loop *loop, int fd, void *args);
void server_wt_callback(event_loop *loop, int fd, void *args);

void server_rd_callback(event_loop *loop, int fd, void *args){
	int ret = 0;
	input_buf ibuf;

	struct message *msg = (struct message *)args;
	ret = ibuf.read_data(fd);	// fd可读，将数据读到buf中，再从buffer发给对端
	if(ret == -1){
		fprintf(stderr, "ibuf read_data error\n");
        //删除事件
		loop->del_io_event(fd);
		//对端关闭
        close(fd);

        return;
	}
	else if(ret == 0){
		//删除事件
        loop->del_io_event(fd);
        
        //对端关闭
        close(fd);
        return ;

	}
	printf("ibuf.length() = %d\n", ibuf.length());

	 //将读到的数据放在msg中
	msg->len = ibuf.length();
	memset(msg->data, 0, sizeof(msg->len));
	memcpy(msg->data, ibuf.data(), msg->len);

	ibuf.pop(msg->len);
	ibuf.adjust();

	 printf("recv data = %s\n", msg->data);


    //删除读事件，添加写事件
    loop->del_io_event(fd, EPOLLIN);
    loop->add_io_event(fd, server_wt_callback, EPOLLOUT, msg);

}

void server_wt_callback(event_loop *loop, int fd, void *args){

    struct message *msg = (struct message*)args;
    output_buf obuf;

    //回显数据
    obuf.send_data(msg->data, msg->len);
    while(obuf.length()) {
        int write_ret = obuf.write2fd(fd);
        if (write_ret == -1) {
            fprintf(stderr, "write connfd error\n");
            return;
        }
        else if(write_ret == 0) {
            //不是错误，表示此时不可写
            break;
        }
    }

    //删除写事件，添加读事件
    loop->del_io_event(fd, EPOLLOUT);
    loop->add_io_event(fd, server_rd_callback, EPOLLIN, msg);
}


void tcp_server::do_accept(){	// 提取客户端连接
	
	int connfd;
	while(true){
		printf("===========begin accecpt!=============\n");
		
		connfd = accept(_sockfd, (struct sockaddr *)&_connaddr, &_addrlen);
		
		printf("=======current connfd: %d======== \n", connfd);
		if(connfd == -1){
			if (errno == EINTR) {
                fprintf(stderr, "accept errno=EINTR\n");
                continue;
            }
            else if (errno == EMFILE) {
                //建立链接过多，资源不够
                fprintf(stderr, "accept errno=EMFILE\n");
            }
			// 代表当前不可读
            else if (errno == EAGAIN) {
                fprintf(stderr, "accept errno=EAGAIN\n");
                break;
            }
            else {
                fprintf(stderr, "accept error");
                exit(1);
            }
		}
		else{
			// accept succ!
			// 建立连接成功后，判断是否达到最大连接数，进行链接管理，注册connfd的可读事件
			int cur_conn;
			get_conn_num(&cur_conn);
			
			//printf("====cur_conn: %d===== \n", cur_conn);

			if(cur_conn >= _max_conns){
				fprintf(stderr, "so many connections, max = %d\n", _max_conns);
			   	close(connfd);
				//exit(1);	
				//break;	
			}else{
				// ========= 将新连接交由线程池处理 ==========
				if(_thread_pool){
					//启动多线程模式 创建链接
                    //1 选择一个线程来处理
					thread_queue<task_msg> *queue = _thread_pool->get_thread();
					//2 创建一个新建链接的消息任务
				
					printf("******_thread_pool->get_thread()====\n");
					task_msg task;
					task.type = task_msg::NEW_CONN;
					task.connfd = connfd;
					
					queue->send(task); // 主线程只负责监听，然后将连接分发给子线程，子线程监听分发到它头上的所有连接，处理连接上来的事件

				} else{
					tcp_conn *conn = new tcp_conn(connfd, _loop);
			
					if (conn == NULL) {
         	      	 	fprintf(stderr, "new tcp_conn error\n");
                		exit(1);
            		}
            		// ============================================
           			printf("get new connection succ!\n");
      					
				}
	
			
			}
//			this->_loop->add_io_event(connfd, server_rd_callback, EPOLLIN, &msg);
          break;

		}

	}
}	




tcp_server::~tcp_server(){
	close(_sockfd);
}

