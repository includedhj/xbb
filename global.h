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

#define HEAD 0xff
#define MAXLINE 10240
#define MIDLINE 1024
int port = 44444;
int per_seq_size = 1000;
int sock_fd = -1;
int get_time();
void get_time_str(char * time_str);

//�������
enum ORDER{
	LOG_IN = 1, LOG_OFF, SEND_MSG, NOTIFY, PUSH_MSG, PULL_MSG, KEEP_ALIVE, ACK, ORDER_NUM
} ;

//������Ϣ��Ƭ��
typedef struct _SEND_MSG_SEQ SEND_MSG_SEQ;
struct _SEND_MSG_SEQ{
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
struct _RECV_MSG_SEQ{
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
    void output()
    {
        printf("recv_msg_seq:   seq:[%d], is_send_ack:[%d], last_send_ack_time:[%d], last_recv_msg_time:[%d], data_len:[%d]\n", seq,
                is_send_ack, last_send_ack_time, last_recv_msg_time, len);
        char p_data[1024];
        bzero(p_data, 1024);
        memcpy(p_data, data, len);
        printf("recv_msg_seq data:[%s]\n", p_data);
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

 struct _SEND_MSG_MAP{
	int msg_id;//������Ϣid
	ORDER order;//����
	char from[16];//���ͷ�����
	char to[16];//���շ�����
	int size;//�������ݴ�С
	int seq_num;//��Ƭ����
	unsigned char is_send;//�Ƿ������
	int send_seq_num;//�յ�ack���1
    map<int, SEND_MSG_SEQ *> send_msg_seq_map;
    int last_send_time;
    int is_broadcast;//�Ƿ�Ϊ�㲥��Ϣ
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
	void init(int msg_id, ORDER order, char * from, char * to, int size, int is_broadcast)
	{
		this->msg_id = msg_id;
		this->order = order;
		memcpy(this->from, from, 16);
		memcpy(this->to, to, 16);
		this->size = size;

		if (size % per_seq_size == 0)
			this->seq_num = size / per_seq_size;
		else
			this->seq_num = size / per_seq_size + 1;
		
		this->is_send = 0;
		this->send_seq_num = 0;
        this->last_send_time = 0;//�ϴ�_SEND_MSG_MAP����ⷢ��ʱ��
        this->is_broadcast = is_broadcast;//�趨�㲥��Ϣ
	}

	SEND_MSG_SEQ * check_send_msg_seq( int seq)
	{
        map<int, SEND_MSG_SEQ *>::iterator iter;
		SEND_MSG_SEQ * sms = NULL;
		iter = send_msg_seq_map.find(seq);
		if (iter == send_msg_seq_map.end())
			return NULL;
        sms = iter->second;
		return sms;
	}
	void add_msg(char * data, int len)
	{
		int seq = 0;
		int index = 0;
        int data_len;
		while(len > 0)
		{

			if(per_seq_size > len)
				data_len = len;
			else
				data_len = per_seq_size;
			add_2_send_seq_map(seq, data+index, data_len);
			len -= data_len;
			index += data_len;
			seq++;
		}
	}
	void add_2_send_seq_map(int seq, char * data, int data_len)
	{
		//int time = get_time();//���յ�����ʱ�䣬ȡ��ǰʱ��
		SEND_MSG_SEQ * sms  = check_send_msg_seq(seq);
		if(sms == NULL)
		{
			sms = new SEND_MSG_SEQ();
			sms->init(seq, data, data_len);
			//��ӵ�recv_msg_seq_map��
			send_msg_seq_map.insert(pair<int, SEND_MSG_SEQ *>(seq, sms));
			
		}
		else
		{
			printf("waring:[%d] has already exist\n", seq);
		}
			
	
	}

	SEND_MSG_SEQ * get_send_msg_by_seq(int seq)
	{
        map<int, SEND_MSG_SEQ *>::iterator iter;
        SEND_MSG_SEQ * sms = NULL;
        iter = send_msg_seq_map.find(seq);
		if (iter == send_msg_seq_map.end())
			return NULL;
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
        int t = get_time();//�ϴ����ݼ��ʱ��

        //����ϴη���ʱ�����8s�����ܷ�������
	    if(t - this->last_send_time > 8)
	    {
            this->last_send_time = t;
		    global_send_queue.push(smp);
	    }
	
	}
};

//����������Ϣ��Ƭӳ��
typedef struct _RECV_MSG_MAP RECV_MSG_MAP;
 struct _RECV_MSG_MAP{
	int msg_id;//������Ϣid
	ORDER order;//����
	char from[16];//���ͷ�����
	char to[16];//���շ�����
	int size;//�������ݴ�С����λ�ֽ�
	int seq_num;//��Ƭ����
	int recv_seq_num;//���յ��ķ�Ƭ����
	int is_recv_over;//�Ƿ�ȫ��������ɣ���ֹ�ظ�����
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
    void output()
    {
        RECV_MSG_SEQ * rms;
        printf("\nrecv_msg_map:msg_id:[%d], order:[%d], from:[%s], to:[%s], size:[%d], seq_num:[%d], recv_seq_num:[%d], is_recv_over[%d]\n", 
            msg_id, order, from, to, size, seq_num, recv_seq_num, is_recv_over);
        
        for(int i = 0; i < seq_num; i++)
        {
            printf("%d  ", i);
            if(NULL == (rms=check_recv_msg_seq(i)))
            {
                printf("NULL\n");
            }
            else
            {
                rms->output();
            }
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
        is_recv_over = 0;

	}
	void add_2_recv_seq_map(int seq, char * data, int data_len, int * has_init)
	{
		
		RECV_MSG_SEQ * rms  = check_recv_msg_seq(seq);
		if(rms == NULL)
		{
			rms = new RECV_MSG_SEQ();
			recv_seq_num++;
            int t = get_time();//���յ�����ʱ�䣬ȡ��ǰʱ��
			rms->init(seq, t, data, data_len);

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
		if(iter == recv_msg_seq_map.end())
			return NULL;
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
			if (iter == recv_msg_seq_map.end())
				return NULL;
            rms = iter->second;
			memcpy(data+index, rms->data, rms->len);
            index += rms->len;
            //printf("rms len:%d\n", rms->len);
		}
        //�Ѿ��������
        is_recv_over = 1;
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
	//CLIENT * cli;
	void init(ORDER order, char * from, char * to, char * data,  int size, int msg_id)
	{
		//��ʼ��smm
		bzero(this->from,16);
        bzero(this->to, 16);
		
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
		//this->cli = cli;
		this->msg_id = msg_id;
	}


} SYSTEM_MSG_MAP;

/*�ͻ��˽ṹ��Ϣ*/
 struct _CLIENT{

	struct sockaddr_in sin;

	char name[32];
	//�Ƿ����ڷ���push��Ϣ
	int is_push_msg;
	int has_ever_login;
	//�Ƿ�����
	int is_on_line;
	//��¼ʱ��
	string login_time;

	//���һ�ν��յ�����ʱ��
	int last_recv_keep_alive_time;
	//���һ�η��͵�����ʱ��
	int last_send_keep_alive_time;

    //�ϴη���notifyʱ��
    int last_send_notify_time;




	//����������Ϣ����(����)
	map <int, RECV_MSG_MAP *> recv_msg_arr;
	//����������Ϣ����(����)
	map <int, SEND_MSG_MAP *> send_msg_arr;


    //����ϵͳ��Ϣ����
	//queue <SYSTEM_MSG_MAP *> recv_sys_msg_arr;//��Ϊ������ʷ��Ϣ
	//����ϵͳ��Ϣ����
	map <int, SYSTEM_MSG_MAP *> send_sys_msg_arr;

	pthread_mutex_t c_lock;//��������
	unsigned int msg_id ;


	_CLIENT(const char * name) {
        strncpy(this->name, name, 16);
		is_push_msg = 0;
		is_on_line = 0;
		has_ever_login = 0;
		msg_id = rand()%65535;
        last_send_notify_time = 0;//�ϴη���notifyʱ��Ϊ0
		//init vector
	}
	int gen_msgid()
	{
		return msg_id++;
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
                (int)recv_msg_arr.size(),
                (int)send_msg_arr.size(),
                (int)send_sys_msg_arr.size()
                );
        printf("\n\n");
    }

    void output_by_msgid(int mid)
    {
      int has_init;
      RECV_MSG_MAP * rmm = check_recv_msg_map(mid, &has_init);
      if(rmm == NULL)
      {
        printf("client:[%s], output_by_msgid, not exist msgid[%d]", name,mid);
      }
      rmm->output();
    }
    
	void clear_recv_msg_by_id(int msg_id)
	{
        map <int, RECV_MSG_MAP *>::iterator iter;
		RECV_MSG_MAP * rmm = NULL;
        iter = recv_msg_arr.find(msg_id);
		if(iter != recv_msg_arr.end())
		{
			rmm = iter->second;
			delete rmm;
			recv_msg_arr.erase(msg_id);
		}
        
	}
	RECV_MSG_MAP * check_recv_msg_map( int msg_id, int * has_init)
	{
        map <int, RECV_MSG_MAP *>::iterator iter;
		RECV_MSG_MAP * rmm = NULL;
        iter = recv_msg_arr.find(msg_id);
		if (iter != recv_msg_arr.end())
		{
			 rmm = iter->second;
		}
	
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
		if (iter == send_sys_msg_arr.end())
		{
			return NULL;
		}
			
			
        smm = iter->second;
		return smm;
	}
	SEND_MSG_MAP * get_send_msg_by_id(int msg_id)
	{
        map <int, SEND_MSG_MAP*>::iterator iter;
		SEND_MSG_MAP * smm;
        iter = send_msg_arr.find(msg_id);
		if (iter == send_msg_arr.end())
			return NULL;
        smm = iter->second;
		return smm;
	}
	SEND_MSG_MAP * get_send_msg_nx_by_id(int msg_id, int * has_init)//not exist
	{
        map <int, SEND_MSG_MAP*>::iterator iter;
		SEND_MSG_MAP * smm = NULL;
        iter = send_msg_arr.find(msg_id);
		if(send_msg_arr.end()!=iter)
		{
			smm = iter->second;;
		}
		
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



	int push_sys_msg_2_queue()
	{
		//ϵͳ��Ϣ���ݶ���
		/*
		 * 1.�������δ��ɣ�continue
		 * 2.���������ɣ���Ҫ����ack����δ�յ�ack�����·���(notify)
		 * 3.���������ɣ�����Ҫ����ack��ɾ��
		 * 4.���������ɣ��յ�ack��ɾ��
		 * */
		map <int, SYSTEM_MSG_MAP *>::iterator sys_smm_it;
		int msg_count = 0;
		for(sys_smm_it=send_sys_msg_arr.begin(); sys_smm_it !=  send_sys_msg_arr.end(); ++sys_smm_it)
		{
			msg_count++;
			SYSTEM_MSG_MAP * sys_smm = sys_smm_it->second;
			//��δ���ͣ�����Ҫɾ��
			if(sys_smm->is_send_ok != 1)
			{
				continue;
			}
			//������ɣ�����û�յ�ack
			else  if(sys_smm->is_send_ok == 1 && sys_smm->is_recv_ack ==0  && sys_smm->order == NOTIFY)
			{
				//�ͻ����Ѿ���ʼpull��Ϣ��ɾ��notify��Ϣ������Ҫ�ٷ���
				if(is_push_msg == 1)
				{
					printf("delte notify msg from send map\n");
					send_sys_msg_arr.erase(sys_smm->msg_id);
					delete sys_smm;
					continue;
				}
					
				//2s��û�ܵ�ack�� �ط�����Ϣ��
				int t = get_time();
				if(t - sys_smm->last_send_msg_time > 2 )
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
		return msg_count;
	}
	int push_com_msg_2_queue()
	{
        int msg_count = 0;
		//��client���������е��������ݷ���ȫ�ַ��Ͷ���,�ȴ�����
		 map<int,SEND_MSG_MAP *>::iterator it;
		 for(it=send_msg_arr.begin();it!=send_msg_arr.end();++it)
		 {
            
	//		 int msg_id = it->first;
			 SEND_MSG_MAP * smm = it->second;

			 //���Լ����
			 //smm->clear_self();
			 //��������push�����Ͷ���
			 if(this->is_push_msg && smm->is_send == 0)
				 smm->push_2_queue(smm->msg_id, name);
			 //���ͽ���
			 if(smm->is_send == 1)
			 {
				 send_msg_arr.erase(smm->msg_id);
			 }
             else{
                 //δ����������ݼ���
                 msg_count++;
             }
              

		 }
         return msg_count;
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
	int len;//ֻ��������������,json_len+data_len = len
	int msg_id;
	char from[16];
	char to[16];
	int json_len;
	unsigned char data[0];
	void init(ORDER order, int len, int msg_id, int json_len, char *to)
	{
        bzero(this->from, 16);
        bzero(this->to, 16);
		head = HEAD;
		this->order = order;
		this->len = len;
		this->json_len = json_len;
        memcpy(from, "server", 16);
		memcpy(this->to, to, 16);
		this->msg_id = msg_id;
	}
    void set_from(char * from)
    {
        memcpy(this->from, from, 16);
    }
    void output()
    {
        printf("\npacket\n");
        printf("head:[%d], order:[%d], len:[%d], msg_id:[%d], from:[%s], to:[%s], json_len:[%d], data_len:[%d]\n",
                head, order, len, msg_id, from, to, json_len, len - json_len);
        printf(" \n");
    }
} PACKET;

