#include "tcp_client.h"

tcp_client::tcp_client(event_loop *loop, const char *ip, unsigned short int port ){
	
	_sockfd = -1;
	_loop = loop;
	// _msg_callback = NULL;

	//创建链接之后要触发的 回调函数
    _conn_start_cb = nullptr;     

    //销毁链接之前要触发的 回调函数
    _conn_close_cb = nullptr;
  

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	inet_aton(ip, &serv_addr.sin_addr);
	serv_addr.sin_port = htons(port);
	_addrlen = sizeof(serv_addr);

	this->do_connect();

}

static void client_rd_callback(event_loop *loop, int fd, void *args){
	tcp_client *client = (tcp_client *)args;
	client->do_read();
}

static void client_wr_callback(event_loop *loop, int fd, void *args){
	tcp_client *client = (tcp_client *)args;
	client->do_write();
}

//判断链接是否是创建链接，主要是针对非阻塞socket 返回EINPROGRESS错误
static void connection_succ(event_loop *loop, int fd, void *args)
{
    tcp_client *cli = (tcp_client*)args;
    loop->del_io_event(fd);

    int result = 0;
    socklen_t result_len = sizeof(result);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &result, &result_len);
    if (result == 0) {

        printf("connect %s:%d succ!\n", inet_ntoa(cli->serv_addr.sin_addr), ntohs(cli->serv_addr.sin_port));
		
		//调用开发者客户端注册的创建链接之后的hook函数
        if (cli->_conn_start_cb != NULL) {
            cli->_conn_start_cb(cli, cli->_conn_start_cb_args);
        }

		
		loop->add_io_event(fd, client_rd_callback, EPOLLIN, cli);
			
		if (cli->obuf.length() != 0) {
            //输出缓冲有数据可写
            loop->add_io_event(fd, client_wr_callback, EPOLLOUT, cli);
        }
    }
    else {
        //链接创建失败
        fprintf(stderr, "connection %s:%d error\n", inet_ntoa(cli->serv_addr.sin_addr), ntohs(cli->serv_addr.sin_port));
    }
}

void tcp_client::do_connect(){

	if(_sockfd != -1)	close(_sockfd);

	_sockfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, IPPROTO_TCP);

	if(_sockfd == -1){
		fprintf(stderr, "create tcp client socket error\n");
        exit(1);
	}

	int ret = connect(_sockfd, (struct sockaddr *)&serv_addr, _addrlen);
	if(ret == 0){
		
		// 创建连接成功
		
		//调用开发者客户端注册的创建链接之后的hook函数
        if (_conn_start_cb != NULL) {
            _conn_start_cb(this, _conn_start_cb_args);
        }
		
		_loop->add_io_event(_sockfd, client_rd_callback, EPOLLIN, this);
		
		//如果写缓冲区有数据，那么也需要触发写回调,将数据写出去
        if (this->obuf.length() != 0) {
            _loop->add_io_event(_sockfd, client_wr_callback, EPOLLOUT, this);
        }

		connection_succ(_loop, _sockfd, this);
		printf("connect %s : %d succ \n", inet_ntoa(serv_addr.sin_addr), ntohs(serv_addr.sin_port));
	
	}
	else {
		if(errno == EINPROGRESS) {
            //fd是非阻塞的，可能会出现这个错误,但是并不表示链接创建失败
            //如果fd是可写状态，则为链接是创建成功的.
            fprintf(stderr, "do_connect EINPROGRESS\n");

            //让event_loop去触发一个创建判断链接业务 用EPOLLOUT事件立刻触发
            _loop->add_io_event(_sockfd, connection_succ, EPOLLOUT, this);
        }
        else {
            fprintf(stderr, "connection error\n");
            exit(1);
        }
	}
}	


void tcp_client::do_read(){
	
	int ret = ibuf.read_data(_sockfd);
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
		
		// 执行回显业务，此函数的注册是在client.cpp那里
		/*if(_msg_callback != NULL){
			this->_msg_callback(ibuf.data(), head.msglen, head.msgid, this, NULL);
		}*/

		// 消息路由分发
		this->_router.call(head.msgid, head.msglen, ibuf.data(), this);

		//消息体处理完了,往后偏移msglen长度
        ibuf.pop(head.msglen);
	}
	ibuf.adjust();	// 将未使用数据提前
	return ;


}


void tcp_client::do_write(){
	while(obuf.length()){
		int ret = obuf.write2fd(_sockfd);

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
        //数据已经全部写完，将_sockfd的写事件取消掉
        _loop->del_io_event(_sockfd, EPOLLOUT);
//		_loop->add_io_event(_sockfd, read_callback, EPOLLIN, this);
    }

    return ;

}

// 将数据写到output_buf中，再通过do_write发送给对端
int tcp_client::send_message(const char *data, int msglen, int msgid){
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
        _loop->add_io_event(_sockfd, client_wr_callback, EPOLLOUT, this);
	}

	return 0;


}


//释放链接资源,重置连接
void tcp_client::clean_conn()
{
    if (_sockfd != -1) {
        printf("clean conn, del socket!\n");
        _loop->del_io_event(_sockfd);
        close(_sockfd);
    }
	
	//调用开发者注册的销毁链接之前触发的Hook
    if (_conn_close_cb != NULL) {
        _conn_close_cb(this, _conn_close_cb_args);
    }

    //重新连接
    this->do_connect();
}

tcp_client::~tcp_client()
{
    close(_sockfd);
}
