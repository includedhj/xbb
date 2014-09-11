/*
 * global.h
 *
 *  Created on: 2014��8��9��
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

//�������
enum ORDER{
	LOG_IN = 1, LOG_OFF, SEND_MSG, NOTIFY, PUSH_MSG, PULL_MSG, KEEP_ALIVE, ACK, ORDER_NUM
} ;

//������Ϣ��Ƭ��
typedef struct _SEND_MSG_SEQ SEND_MSG_SEQ;
typedef struct _SEND_MSG_SEQ{
	int seq; //���
	//unsigned char is_send; //�Ƿ���
	unsigned char retry_send_times; //�������Դ��������շ���ֵĬ��Ϊ0
	unsigned char is_recv_ack; //�Ƿ��յ�ack
	unsigned int last_send_msg_time;//�ϴη���msg��ʱ��
	unsigned int last_recv_ack_time;//�ϴν��յ�ack��ʱ��
	void * data;//��Ƭ�洢����
	int len;//���ݴ�С
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
		 * ��data�����json���У�MSG_SEQ*/

		this->data=malloc(len);
		memcpy(this->data, data, len);
	}
	~_SEND_MSG_SEQ()
	{
		free(data);
	}
};

//������Ϣ��Ƭ��
typedef struct _RECV_MSG_SEQ RECV_MSG_SEQ;
typedef struct _RECV_MSG_SEQ{
	int seq; //���
	//unsigned char is_recv;//�Ƿ����
	unsigned char is_send_ack;//�Ƿ��͹�ack
	unsigned int last_send_ack_time;//�ϴη���ack��ʱ��
	unsigned int last_recv_msg_time;//����msg��ʱ��
	void * data;//��Ƭ�洢����
	int len;//��Ƭ���ݴ�С
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

//�ܹ�Ψһ��λ��Ϣid�Ľṹ��,����ȫ�ַ��Ͷ�����
typedef struct{
	char name[32];//�ͻ�������
	int  msg_id;//��Ϣid���
	MSG_TYPE  type;//��Ϣ���� SYS_MSG, COM_MSG
}SEND_MSG_POS;

//����������Ϣ��Ƭӳ��
typedef struct _SEND_MSG_MAP SEND_MSG_MAP;
typedef struct _CLIENT CLIENT;

//pthread_metux_lock global_send_queue_lock;//ȫ�ַ��Ͷ��е���
queue<SEND_MSG_POS *> global_send_queue;//ȫ�ַ��Ͷ���
//pthread_mutex_lock client_map_lock;//cient_map����
map<string, CLIENT *> client_map;//���client

typedef struct _SEND_MSG_MAP{
	int msg_id;//������Ϣid
	ORDER order;//����
	char from[16];//���ͷ�����
	char to[16];//���շ�����
	int size;//�������ݴ�С
	int seq_num;//��Ƭ����
	unsigned char is_send;//�Ƿ������
	int send_seq_num;//�յ�ack���1
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
		int time = 9999;//���յ�����ʱ�䣬ȡ��ǰʱ��
		SEND_MSG_SEQ * sms  = check_send_msg_seq(seq);
		if(sms == NULL)
		{
			sms = new SEND_MSG_SEQ();
			sms->init(seq, data, data_len);
			//��ӵ�recv_msg_seq_map��
			send_msg_seq_map.insert(pair<int, SEND_MSG_SEQ *>(seq, sms));
			*has_init = 0;
		}
		else
		{
			//���ݰ��Ѿ����ڣ���¼log���ͻ����ظ�����,�ظ�ack,����Ҫ��recv_seq_num+1
			*has_init = 1;//�Ѵ���
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
		//����Ϣ�Ѿ��������,ɾ��
		 if(is_send == 1)
		{
			 //~SEND_MSG_MAP();
			 return;
		}
	}
	void push_2_queue(int msg_id, char * name)
	{
		//����Ϣid���뵽���Ͷ���
		SEND_MSG_POS * smp = (SEND_MSG_POS *)malloc(sizeof(SEND_MSG_POS));
		smp->msg_id = msg_id;
        memcpy(smp->name, name, 16);
		smp->type = COM_MSG;//������Ϣ


		//Ϊȫ�ַ��Ͷ��м���
		//pthread_lock(global_send_queue_mutex);
		global_send_queue.push(smp);
		//Ϊȫ�ַ��Ͷ��н���
		//pthread_unlock(global_send_queue_mutex);
	}
};

//����������Ϣ��Ƭӳ��
typedef struct _RECV_MSG_MAP RECV_MSG_MAP;
typedef struct _RECV_MSG_MAP{
	int msg_id;//������Ϣid
	ORDER order;//����
	char from[16];//���ͷ�����
	char to[16];//���շ�����
	int size;//�������ݴ�С����λ�ֽ�
	int seq_num;//��Ƭ����
	int recv_seq_num;
	//unsigned char is_recv; //�Ƿ��Ѿ����չ���msgid
	map<int, RECV_MSG_SEQ *> recv_msg_seq_map;
	//vector <RECV_MSG_SEQ *> recv_msg_seq_map;
	CLIENT * client;

	~_RECV_MSG_MAP()
	{
		//��recv_msg_seq_map��RECV_MSG_SEQ��������

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
		int time = 9999;//���յ�����ʱ�䣬ȡ��ǰʱ��
		RECV_MSG_SEQ * rms  = check_recv_msg_seq(seq);
		if(rms == NULL)
		{
			rms = new RECV_MSG_SEQ();
			recv_seq_num++;

			rms->init(seq, time, data, data_len);

			//��ӵ�recv_msg_seq_map��
			recv_msg_seq_map.insert(pair<int, RECV_MSG_SEQ*>(seq, rms));
			*has_init = 0;
		}
		else
		{
			//���ݰ��Ѿ����ڣ���¼log���ͻ����ظ�����,�ظ�ack,����Ҫ��recv_seq_num+1
			*has_init = 1;//�Ѵ���
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


//��������ͨ��Ϣ�ṹ
typedef struct{
	int msg_id;//��Ϣid
	ORDER order;//����
	char from[16];//���ͷ�����
	char to[16];//���շ�����
	int size;//��������С

	unsigned char is_send_ok; //�Ƿ��ͳɹ���������Ϣֱ���ж�
	unsigned char retry_send_times; //�������Դ��������շ���ֵĬ��Ϊ0
	unsigned char is_recv_ack; //�Ƿ��յ�ack��������͵���ack�����ֶ���Ч
	unsigned int last_send_msg_time;//�ϴη���msg��ʱ��
	unsigned int last_recv_ack_time;//���յ�ack��ʱ��,������͵���ack�����ֶ���Ч
	unsigned char * data;//������
	void init(ORDER order, char * from, char * to, char * data,  int size)
	{
		//��ʼ��smm
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

/*�ͻ��˽ṹ��Ϣ*/
typedef struct _CLIENT{

	struct sockaddr_in sin;

	char name[32];
	//�Ƿ����ڷ���push��Ϣ
	int is_push_msg;
	//�Ƿ�����
	int is_on_line;
	//��¼ʱ��
	string login_time;

	//���һ�ν��յ�����ʱ��
	int last_recv_keep_alive_time;
	//���һ�η��͵�����ʱ��
	int last_send_keep_alive_time;



	//����������Ϣ����(����)
	map <int, RECV_MSG_MAP *> recv_msg_arr;
	//����������Ϣ����(����)
	map <int, SEND_MSG_MAP *> send_msg_arr;


    //����ϵͳ��Ϣ����
	//queue <SYSTEM_MSG_MAP *> recv_sys_msg_arr;//��Ϊ������ʷ��Ϣ
	//����ϵͳ��Ϣ����
	map <int, SYSTEM_MSG_MAP *> send_sys_msg_arr;

	pthread_mutex_t c_lock;//��������


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
			//�����µ�
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
			//�����µ�
			send_msg_arr.insert(pair<int, SEND_MSG_MAP*>(msg_id, smm));
			*has_init = 0;
		}
		return smm;
	}

	void add_sys_msg_by_id(int msg_id, SYSTEM_MSG_MAP * smm)
	{
		//smm���뵽client��(�ǵü�����)��
		send_sys_msg_arr.insert(pair<int, SYSTEM_MSG_MAP*>(smm->msg_id,smm));
	}



	void push_sys_msg_2_queue()
	{
		//ϵͳ��Ϣ���ݶ���
		/*
		 * 1.�������δ��ɣ�continue
		 * 2.���������ɣ���Ҫ����ack����δ�յ�ack�����·���(notify)
		 * 3.���������ɣ�����Ҫ����ack��ɾ��
		 * 4.���������ɣ��յ�ack��ɾ��
		 * */
		map <int, SYSTEM_MSG_MAP *>::iterator sys_smm_it;
		for(sys_smm_it=send_sys_msg_arr.begin(); sys_smm_it !=  send_sys_msg_arr.end(); ++sys_smm_it)
		{
			SYSTEM_MSG_MAP * sys_smm = sys_smm_it->second;
			//��δ���ͣ�����Ҫɾ��
			if(sys_smm->is_send_ok != 1)
			{
				continue;
			}
			//������ɣ�����û�յ�ack
			else  if(sys_smm->is_send_ok == 1 && sys_smm->is_recv_ack ==0  && sys_smm->order == NOTIFY)
			{
				//2s��û�ܵ�ack�� �ط�����Ϣ��
				if(9999 - sys_smm->last_send_msg_time > 2 )
				{
					SEND_MSG_POS * smp = (SEND_MSG_POS *)malloc(sizeof(SEND_MSG_POS));
					smp->msg_id = sys_smm->msg_id;
                    memcpy(smp->name, name, 16);
					smp->type = SYS_MSG;

					//���뷢�Ͷ����У�Ϊȫ�ַ��Ͷ��м���
					//pthread_lock(global_send_queue_mutex);
					global_send_queue.push(smp);
					//Ϊȫ�ַ��Ͷ��н���
					//pthread_unlock(global_send_queue_mutex);
				}

			}
			//������ɣ�����Ҫ����ack,�����Ѿ��յ�ack
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

		//��client���������е��������ݷ���ȫ�ַ��Ͷ���,�ȴ�����
		 map<int,SEND_MSG_MAP *>::iterator it;
		 for(it=send_msg_arr.begin();it!=send_msg_arr.end();++it)
		 {
			 int msg_id = it->first;
			 SEND_MSG_MAP * smm = it->second;

			 //���Լ����
			 smm->clear_self();
			 //��������push�����Ͷ���
			 if(this->is_push_msg)
				 smm->push_2_queue(smm->msg_id, name);
			 //���ͽ���
			 if(smm->is_send == 1)
			 {
				 send_msg_arr.erase(smm->msg_id);
			 }

		 }
	}


} ;




//������Ϣ����




//���ݰ�����״̬����
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
//��¼��Ϣ
//��ͷ+����+len+msg_id+from+to+data
//��Ϣ������
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

