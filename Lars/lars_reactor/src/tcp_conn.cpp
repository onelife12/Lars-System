#include <unistd.h>
#include <cstdio>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>

#include "tcp_conn.h"
#include "message.h"
#include "tcp_server.h"

// 	回显业务
void callback_busi(const char *data, uint32_t len, int msgid, void *args, tcp_conn *conn){
	conn->send_message(data, len, msgid);
}

//连接的读事件回调
static void conn_rd_callback(event_loop *loop, int fd, void *args){
	tcp_conn *conn = (tcp_conn *)args;
	conn->do_read();	// 从input_buf中读取数据
}

//连接的写事件回调
static void conn_wt_callback(event_loop *loop, int fd, void *args){
	tcp_conn *conn = (tcp_conn*)args;
	conn->do_write(); // 将output_buf中的数据发给对端，服务器
}

tcp_conn::tcp_conn(int connfd, event_loop *loop){
	_connfd = connfd;
	_loop = loop;

	//1. 将connfd设置成非阻塞状态
	int flag = fcntl(_connfd, F_GETFL, 0);
	fcntl(_connfd, F_SETFL, O_NONBLOCK | flag);

	//2. 设置TCP_NODELAY禁止做读写缓存，降低小包延迟
    int op = 1;
    setsockopt(_connfd, IPPROTO_TCP, TCP_NODELAY, &op, sizeof(op));//need netinet/in.h netinet/tcp.h

	//2.5 如果用户注册了链接建立Hook 则调用
	if(tcp_server::conn_start_cb){
		tcp_server::conn_start_cb(this, tcp_server::conn_start_cb_args);
	}	

	//3. 将该链接的读事件让event_loop监控
	_loop->add_io_event(_connfd, conn_rd_callback, EPOLLIN, this);
	
	//4 将该链接集成到对应的tcp_server中
	// 连接成功，将在线的连接数数量增加
	tcp_server::increase_conn(_connfd, this);
}

void tcp_conn::do_read(){
	int ret = ibuf.read_data(_connfd);
	if(ret == -1){
		fprintf(stderr, "read data from socket\n");
        this->clean_conn();
        return ;
	}
	else if(ret == 0){
		//对端正常关闭
        printf("connection closed by peer\n");
        this->clean_conn();
        return ;
	}

	//ret > 0读到数据，解析固有的消息格式，获取数据部分
	msg_head head;
	while(ibuf.length() >= MESSAGE_HEAD_LEN){
		memcpy(&head, ibuf.data(), MESSAGE_HEAD_LEN);
		if(head.msglen > MESSAGE_LENGTH_LIMIT || head.msglen < 0){
			fprintf(stderr, "data format error, need close, msglen = %d\n", head.msglen);
            this->clean_conn();
            break;
		}
		if(ibuf.length() < MESSAGE_HEAD_LEN + head.msglen){
			//缓存buf中剩余的数据，小于实际上应该接受的数据
            //说明是一个不完整的包，应该抛弃
            break;
		}

		//2.2 再根据头长度读取数据体，然后针对数据体处理 业务

		//头部处理完了，往后偏移MESSAGE_HEAD_LEN长度
        ibuf.pop(MESSAGE_HEAD_LEN);

        //处理ibuf.data()业务数据
        printf("read data: %s\n", ibuf.data());
		
		// 将读取到的数据发送到output_buf中,做回显业务
		//callback_busi(ibuf.data(), head.msglen, head.msgid, NULL, this);

		tcp_server::router.call(head.msgid, head.msglen, ibuf.data(), this); // this对应user_data

		//消息体处理完了,往后偏移msglen长度
        ibuf.pop(head.msglen);
	}
	ibuf.adjust();	// 将未使用数据提前
	return ;
}


void tcp_conn::do_write(){
	
	while(obuf.length()){
		int ret = obuf.write2fd(_connfd);

		if (ret == -1) {
            fprintf(stderr, "write2fd error, close conn!\n");
            this->clean_conn();
            return ;
        }
        if (ret == 0) {
            //不是错误，仅返回0表示不可继续写
            break;
        }
    	
	}

	if (obuf.length() == 0) {
        //数据已经全部写完，将_connfd的写事件取消掉
        _loop->del_io_event(_connfd, EPOLLOUT);
    }

    return ;

}

int tcp_conn::send_message(const char *data, int msglen, int msgid){
	printf("server send_message: %s:%d, msgid = %d\n", data, msglen, msgid);
	bool active_epollout = false;
	if(obuf.length() == 0){
		//如果现在已经数据都发送完了，那么是一定要激活写事件的
        //如果有数据，说明数据还没有完全写完到对端，那么没必要再激活等写完再激活
        active_epollout = true;
	}

	// 封装消息格式，将数据写到缓冲区output_buf中
	msg_head head;
	head.msglen = msglen;
	head.msgid = msgid;

	int ret = obuf.send_data((const char *)&head, MESSAGE_HEAD_LEN);
	if (ret != 0) {
        fprintf(stderr, "send head error\n");
        return -1;
    }

	ret = obuf.send_data(data, msglen);
	if (ret != 0) {
        //如果写消息体失败，那就回滚将消息头的发送也取消
        obuf.pop(MESSAGE_HEAD_LEN);
        return -1;
    }

	if(active_epollout){
		//2. 激活EPOLLOUT写事件
        _loop->add_io_event(_connfd, conn_wt_callback, EPOLLOUT, this);
	}

	return 0;

}


void tcp_conn::clean_conn(){
	//链接清理工作
	
	// 如果注册了链接销毁Hook函数，则调用
    if (tcp_server::conn_close_cb) {
        tcp_server::conn_close_cb(this, tcp_server::conn_close_cb_args);
    }

    //1 将该链接从tcp_server摘除掉    
    tcp_server::decrease_conn(_connfd);
    //2 将该链接从event_loop中摘除
    _loop->del_io_event(_connfd);
    //3 buf清空
    ibuf.clear(); 
    obuf.clear();
    //4 关闭原始套接字
    int fd = _connfd;
    _connfd = -1;
    close(fd);	
	
}








