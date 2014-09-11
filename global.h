/*
 * global.h
 *
 *  Created on: 2014年8月9日
 *      Author: donghongqing
 */
#include <stdlib.h>
#include <time.h>
#include <map>
#include <string.h>
#include <string>
#include <pthread.h>
#include <queue>

using namespace std;

#define HEAD 0x01
#define MAXLINE 10240
#define MIDLINE 1024
int port = 44444;
int sock_fd = -1;

//命令分类
enum ORDER{
	LOG_IN = 1, LOG_OFF, SEND_MSG, NOTIFY, PUSH_MSG, PULL_MSG, KEEP_ALIVE, ACK, ORDER_NUM
} ;

//发送消息分片包
typedef struct _SEND_MSG_SEQ SEND_MSG_SEQ;
typedef struct _SEND_MSG_SEQ{
	int seq; //序号
	//unsigned char is_send; //是否发送
	unsigned char retry_send_times; //发送重试次数，接收方此值默认为0
	unsigned char is_recv_ack; //是否收到ack
	unsigned int last_send_msg_time;//上次发送msg的时间
	unsigned int last_recv_ack_time;//上次接收到ack的时间
	void * data;//分片存储数据
	int len;//数据大小
    //void init(seq, time, data, len)
	void init(int seq, void *data, int len)
	{
		this->seq = seq;
		this->last_send_msg_time = 0;
		this->last_recv_ack_time = 0;
		this->is_recv_ack = 0;
		this->retry_send_times = 0;
		this->len = len;
		/*
		 * 将data打包到json串中，MSG_SEQ*/

		this->data=malloc(len);
		memcpy(this->data, data, len);
	}
	~_SEND_MSG_SEQ()
	{
		free(data);
	}
};

//接收消息分片包
typedef struct _RECV_MSG_SEQ RECV_MSG_SEQ;
typedef struct _RECV_MSG_SEQ{
	int seq; //序号
	//unsigned char is_recv;//是否接收
	unsigned char is_send_ack;//是否发送过ack
	unsigned int last_send_ack_time;//上次发送ack的时间
	unsigned int last_recv_msg_time;//接收msg的时间
	void * data;//分片存储数据
	int len;//分片数据大小
	void init(int seq, int last_recv_msg_time, char * data, int len)
	{
        this->seq = seq;
        this->last_recv_msg_time = last_recv_msg_time;
        this->last_send_ack_time = 0;
        this->is_send_ack = 0;
        this->data=malloc(len);
        memcpy(this->data, data, len);
        this->len = len;
	}
	void set_last_send_ack_time(int lsat)
	{

	}
	void * get_data()
	{
		return data;
	}
	~_RECV_MSG_SEQ()
	{
		free(data);
	}
};

enum MSG_TYPE{
	SYS_MSG = 0,
	COM_MSG = 1
};

//能够唯一定位消息id的结构体,用于全局发送队列中
typedef struct{
	char name[32];//客户端名称
	int  msg_id;//消息id编号
	MSG_TYPE  type;//消息类型 SYS_MSG, COM_MSG
}SEND_MSG_POS;

//发送语音消息分片映射
typedef struct _SEND_MSG_MAP SEND_MSG_MAP;
typedef struct _CLIENT CLIENT;

//pthread_metux_lock global_send_queue_lock;//全局发送队列的锁
queue<SEND_MSG_POS *> global_send_queue;//全局发送队列
//pthread_mutex_lock client_map_lock;//cient_map的锁
map<string, CLIENT *> client_map;//存放client

typedef struct _SEND_MSG_MAP{
	int msg_id;//发送消息id
	ORDER order;//命令
	char from[16];//发送方名称
	char to[16];//接收方名称
	int size;//语音数据大小
	int seq_num;//分片数量
	unsigned char is_send;//是否发送完成
	int send_seq_num;//收到ack后加1
    map<int, SEND_MSG_SEQ *> send_msg_seq_map;
	//vector  <SEND_MSG_SEQ *> send_msg_seq_map;
    //CLIENT * client;
	_SEND_MSG_MAP(){
		//init map
	}
	~_SEND_MSG_MAP()
	{
		map<int,SEND_MSG_SEQ *>::iterator it;
		for(it=send_msg_seq_map.begin();it!=send_msg_seq_map.end();++it)
		{
			SEND_MSG_SEQ * sms = it->second;
			delete sms;
		}
	}
	void init(int msg_id, ORDER order, char * from, char * to, int size, int seq_num)
	{
		this->msg_id = msg_id;
		this->order = order;
		memcpy(this->from, from, 16);
		memcpy(this->to, to, 16);
		this->size = size;
		this->seq_num = seq_num;
		this->is_send = 0;
		this->send_seq_num = 0;
	}

	SEND_MSG_SEQ * check_send_msg_seq( int seq)
	{
        map<int, SEND_MSG_SEQ *>::iterator iter;
		SEND_MSG_SEQ * sms = NULL;
		iter = send_msg_seq_map.find(seq);
        sms = iter->second;
		return sms;
	}
	void add_msg(char * data, int len)
	{
		int per_seq_size = 1500;
		int seq = 0;
		int has_init = 0;
		int index = 0;
        int data_len;
		while(len > 0)
		{

			if(per_seq_size > len)
				data_len = len;
			else
				data_len = per_seq_size;
			add_2_send_seq_map(seq, data+index, data_len, &has_init);
			len -= data_len;
			index += data_len;
			seq++;
		}
	}
	void add_2_send_seq_map(int seq, char * data, int data_len, int * has_init)
	{
		int time = 9999;//接收到包的时间，取当前时间
		SEND_MSG_SEQ * sms  = check_send_msg_seq(seq);
		if(sms == NULL)
		{
			sms = new SEND_MSG_SEQ();
			sms->init(seq, data, data_len);
			//添加到recv_msg_seq_map中
			send_msg_seq_map.insert(pair<int, SEND_MSG_SEQ *>(seq, sms));
			*has_init = 0;
		}
		else
		{
			//数据包已经存在，记录log，客户端重复发送,回复ack,不需要将recv_seq_num+1
			*has_init = 1;//已存在
		}
	}

	SEND_MSG_SEQ * get_send_msg_by_seq(int seq)
	{
        map<int, SEND_MSG_SEQ *>::iterator iter;
        SEND_MSG_SEQ * sms = NULL;
        iter = send_msg_seq_map.find(seq);
        sms = iter->second;
		return sms;
	}
	void clear_self()
	{
		//该消息已经发送完毕,删除
		 if(is_send == 1)
		{
			 //~SEND_MSG_MAP();
			 return;
		}
	}
	void push_2_queue(int msg_id, char * name)
	{
		//将消息id放入到发送队列
		SEND_MSG_POS * smp = (SEND_MSG_POS *)malloc(sizeof(SEND_MSG_POS));
		smp->msg_id = msg_id;
        memcpy(smp->name, name, 16);
		smp->type = COM_MSG;//语音消息


		//为全局发送队列加锁
		//pthread_lock(global_send_queue_mutex);
		global_send_queue.push(smp);
		//为全局发送队列解锁
		//pthread_unlock(global_send_queue_mutex);
	}
};

//接收语音消息分片映射
typedef struct _RECV_MSG_MAP RECV_MSG_MAP;
typedef struct _RECV_MSG_MAP{
	int msg_id;//接收消息id
	ORDER order;//命令
	char from[16];//发送方名称
	char to[16];//接收方名称
	int size;//语音数据大小，单位字节
	int seq_num;//分片数量
	int recv_seq_num;
	//unsigned char is_recv; //是否已经接收过该msgid
	map<int, RECV_MSG_SEQ *> recv_msg_seq_map;
	//vector <RECV_MSG_SEQ *> recv_msg_seq_map;
	CLIENT * client;

	~_RECV_MSG_MAP()
	{
		//将recv_msg_seq_map的RECV_MSG_SEQ进行析构

		map<int,RECV_MSG_SEQ *>::iterator it;
		for(it=recv_msg_seq_map.begin();it!=recv_msg_seq_map.end();++it)
		{
			RECV_MSG_SEQ * rms = it->second;
			delete rms;
		}
	}
	void init(int msg_id, ORDER order, char * from, int from_len,
			     char * to, int to_len, int size,int seq_num)
	{
		//init map
		this->msg_id = msg_id;
		this->order = order;
		strcpy(this->from, from);
		strcpy(this->to, to);
		this->size = size;
		this->seq_num = seq_num;
		recv_seq_num = 0;

	}
	void add_2_recv_seq_map(int seq, char * data, int data_len, int * has_init)
	{
		int time = 9999;//接收到包的时间，取当前时间
		RECV_MSG_SEQ * rms  = check_recv_msg_seq(seq);
		if(rms == NULL)
		{
			rms = new RECV_MSG_SEQ();
			recv_seq_num++;

			rms->init(seq, time, data, data_len);

			//添加到recv_msg_seq_map中
			recv_msg_seq_map.insert(pair<int, RECV_MSG_SEQ*>(seq, rms));
			*has_init = 0;
		}
		else
		{
			//数据包已经存在，记录log，客户端重复发送,回复ack,不需要将recv_seq_num+1
			*has_init = 1;//已存在
		}

	}

	RECV_MSG_SEQ * check_recv_msg_seq( int seq)
	{
        map<int, RECV_MSG_SEQ*>::iterator iter;
		RECV_MSG_SEQ * rms = NULL;
        iter = recv_msg_seq_map.find(seq);
        rms = iter->second;
		return rms;
	}

	char * dump_msg_2_disk(int msg_id)
	{
		char * data = (char *)malloc(size);
		int index = 0;
		for(int i = 0; i < recv_seq_num; i++)
		{
            map<int, RECV_MSG_SEQ*>::iterator iter;
			RECV_MSG_SEQ * rms ;
            iter = recv_msg_seq_map.find(i);
            rms = iter->second;
			memcpy(data+index, rms->data, rms->len);
		}
		return data;
	}


} ;


//非语音普通消息结构
typedef struct{
	int msg_id;//消息id
	ORDER order;//命令
	char from[16];//发送方名称
	char to[16];//接收方名称
	int size;//数据区大小

	unsigned char is_send_ok; //是否发送成功，主动消息直接判断
	unsigned char retry_send_times; //发送重试次数，接收方此值默认为0
	unsigned char is_recv_ack; //是否收到ack，如果发送的是ack，该字段无效
	unsigned int last_send_msg_time;//上次发送msg的时间
	unsigned int last_recv_ack_time;//接收到ack的时间,如果发送的是ack，该字段无效
	unsigned char * data;//数据区
	void init(ORDER order, char * from, char * to, char * data,  int size)
	{
		//初始化smm
		bzero(this->from,16);
        bzero(this->to, 16);
		this->msg_id = rand()%65536;
		this->order = order;
		this->data = (unsigned char *)malloc(size);
		memcpy(this->data, data, size);

        memcpy(this->from, from, 16);
        memcpy(this->to, to, 16);

		this->last_send_msg_time = 0;
		this->last_recv_ack_time = 0;
		this->retry_send_times = 0;
		this->is_send_ok = 0;
		this->is_recv_ack = 0;
		this->size = size;
	}


} SYSTEM_MSG_MAP;

/*客户端结构信息*/
typedef struct _CLIENT{

	struct sockaddr_in sin;

	char name[32];
	//是否正在发送push消息
	int is_push_msg;
	//是否在线
	int is_on_line;
	//登录时间
	string login_time;

	//最后一次接收的心跳时间
	int last_recv_keep_alive_time;
	//最后一次发送的心跳时间
	int last_send_keep_alive_time;



	//接收语音消息向量(文字)
	map <int, RECV_MSG_MAP *> recv_msg_arr;
	//发送语音消息向量(文字)
	map <int, SEND_MSG_MAP *> send_msg_arr;


    //接收系统消息向量
	//queue <SYSTEM_MSG_MAP *> recv_sys_msg_arr;//作为保留历史消息
	//发送系统消息向量
	map <int, SYSTEM_MSG_MAP *> send_sys_msg_arr;

	pthread_mutex_t c_lock;//自旋锁？


	_CLIENT(const char * name) {
        strncpy(this->name, name, 16);
		is_push_msg = 0;
		is_on_line = 0;
		//init vector
	}
    void output()
    {
        printf("\nclient output:\n");
        printf("name:[%s], is_push_msg:[%d], is_on_line:[%d], login_time:[%s], " \
               " last_recv_keep_alive_time:[%d], "
               " last_send_keep_alive_time:[%d], "
               " recv_msg_arr size:[%d], "
               " send_msg_arr size:[%d], "
               " send_sys_msg_arr size:[%d]\n", 
                name, is_push_msg, is_on_line, login_time.c_str(), 
                last_recv_keep_alive_time,
                last_send_keep_alive_time,
                recv_msg_arr.size(),
                send_msg_arr.size(),
                send_sys_msg_arr.size()
                );
        printf("\n\n");
    }


	void clear_recv_msg_by_id(int msg_id)
	{
        map <int, RECV_MSG_MAP *>::iterator iter;
		RECV_MSG_MAP * rmm = NULL;
        iter = recv_msg_arr.find(msg_id);
        rmm = iter->second;
		delete rmm;
		recv_msg_arr.erase(msg_id);
	}
	RECV_MSG_MAP * check_recv_msg_map( int msg_id, int * has_init)
	{
        map <int, RECV_MSG_MAP *>::iterator iter;
		RECV_MSG_MAP * rmm;
        iter = recv_msg_arr.find(msg_id);
        rmm = iter->second;
		*has_init = 1;
		if(rmm == NULL)
		{
			rmm = new RECV_MSG_MAP();
			//设置新的
			recv_msg_arr[msg_id] = rmm;
			*has_init = 0;
		}
		return rmm;
	}
	SYSTEM_MSG_MAP * get_send_sys_msg_by_id(int msg_id)
	{
        map <int, SYSTEM_MSG_MAP*>::iterator iter;
		SYSTEM_MSG_MAP * smm;
        iter = send_sys_msg_arr.find(msg_id);
        smm = iter->second;
		return smm;
	}
	SEND_MSG_MAP * get_send_msg_by_id(int msg_id)
	{
        map <int, SEND_MSG_MAP*>::iterator iter;
		SEND_MSG_MAP * smm;
        iter = send_msg_arr.find(msg_id);
        smm = iter->second;
		return smm;
	}
	SEND_MSG_MAP * get_send_msg_nx_by_id(int msg_id, int * has_init)//not exist
	{
        map <int, SEND_MSG_MAP*>::iterator iter;
		SEND_MSG_MAP * smm ;
        iter = send_msg_arr.find(msg_id);
        smm = iter->second;
		*has_init = 1;
		if(smm == NULL)
		{
			smm = new SEND_MSG_MAP();
			//设置新的
			send_msg_arr.insert(pair<int, SEND_MSG_MAP*>(msg_id, smm));
			*has_init = 0;
		}
		return smm;
	}

	void add_sys_msg_by_id(int msg_id, SYSTEM_MSG_MAP * smm)
	{
		//smm加入到client中(记得加锁噢)，
		send_sys_msg_arr.insert(pair<int, SYSTEM_MSG_MAP*>(smm->msg_id,smm));
	}



	void push_sys_msg_2_queue()
	{
		//系统消息数据队列
		/*
		 * 1.如果发送未完成，continue
		 * 2.如果发送完成，需要接收ack但是未收到ack，重新发送(notify)
		 * 3.如果发送完成，不需要接收ack，删除
		 * 4.如果发送完成，收到ack，删除
		 * */
		map <int, SYSTEM_MSG_MAP *>::iterator sys_smm_it;
		for(sys_smm_it=send_sys_msg_arr.begin(); sys_smm_it !=  send_sys_msg_arr.end(); ++sys_smm_it)
		{
			SYSTEM_MSG_MAP * sys_smm = sys_smm_it->second;
			//还未发送，不需要删除
			if(sys_smm->is_send_ok != 1)
			{
				continue;
			}
			//发送完成，但是没收到ack
			else  if(sys_smm->is_send_ok == 1 && sys_smm->is_recv_ack ==0  && sys_smm->order == NOTIFY)
			{
				//2s内没受到ack， 重发此消息包
				if(9999 - sys_smm->last_send_msg_time > 2 )
				{
					SEND_MSG_POS * smp = (SEND_MSG_POS *)malloc(sizeof(SEND_MSG_POS));
					smp->msg_id = sys_smm->msg_id;
                    memcpy(smp->name, name, 16);
					smp->type = SYS_MSG;

					//放入发送队列中，为全局发送队列加锁
					//pthread_lock(global_send_queue_mutex);
					global_send_queue.push(smp);
					//为全局发送队列解锁
					//pthread_unlock(global_send_queue_mutex);
				}

			}
			//发送完成，不需要接收ack,或者已经收到ack
			else if((sys_smm->is_send_ok == 1 && sys_smm->is_recv_ack == 1)
					||
					((sys_smm->is_send_ok == 1 ) && (sys_smm->order == ACK||sys_smm->order == KEEP_ALIVE)))
			{
				send_sys_msg_arr.erase(sys_smm->msg_id);
				delete sys_smm;
			}

		}
	}
	void push_com_msg_2_queue()
	{

		//将client发送向量中的语音数据放入全局发送队列,等待发送
		 map<int,SEND_MSG_MAP *>::iterator it;
		 for(it=send_msg_arr.begin();it!=send_msg_arr.end();++it)
		 {
			 int msg_id = it->first;
			 SEND_MSG_MAP * smm = it->second;

			 //将自己清空
			 smm->clear_self();
			 //语音数据push到发送队列
			 if(this->is_push_msg)
				 smm->push_2_queue(smm->msg_id, name);
			 //发送结束
			 if(smm->is_send == 1)
			 {
				 send_msg_arr.erase(smm->msg_id);
			 }

		 }
	}


} ;




//离线消息队列




//数据包接收状态分类
/*
 enum
 {
 RECV_START,
 RECV_HEAD,
 RECV_ORDER,
 RECV_LEN,
 RECV_MSGID,
 RECV_DATA,
 RECV_END,
 RECV_ERROR

 }RECV_PACKET_STATE;
 */
//登录消息
//包头+命令+len+msg_id+from+to+data
//消息包定义
typedef struct _PACKET{
	int head;
	ORDER order;
	int len;
	int msg_id;
	char from[16];
	char to[16];
	unsigned char data[0];
	void init(ORDER order, int len, int msg_id, char *to)
	{
        bzero(this->from, 16);
        bzero(this->to, 16);
		head = HEAD;
		order = order;
		this->len = sizeof(_PACKET) + len;
        memcpy(from, "server", 16);
		memcpy(this->to, to, 16);
		msg_id = msg_id;
	}
    void out_put()
    {
        printf("\npacket\n");
        printf("head:[%d], order:[%d], len:[%d], msg_id:[%d], from:[%s], to:[%s]\n",
                head, order, len, msg_id, from, to);
        printf("\n\n");
    }
} PACKET;

