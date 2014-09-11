/*
 * udp_server.c
 *
 *  Created on: 2014��8��9��
 *      Author: donghongqing
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include "global.h"
#include "json/json.h"
//#include <signal.h>

using namespace std;

char * SERVER  = "SERVER";
void dispatch(struct sockaddr_in rin, char *buf, int len);
void deal_log_in(string name, struct sockaddr_in rin, int msg_id);
void deal_ack_msg(PACKET * rcv_pack,  struct sockaddr_in sin);
void deal_log_off(string name, struct sockaddr_in sin, int msg_id);
void deal_pull_msg(string name , struct sockaddr_in sin);
void deal_send_msg(PACKET * rcv_pack, struct sockaddr_in sin);
void deal_heartbeat_msg(string name, struct sockaddr_in sin);
CLIENT * off_client(string name , struct sockaddr_in sin);
CLIENT * check_client(string name);
void create_and_send_com_packet(CLIENT *client, int msg_id);
void create_and_send_system_packet(CLIENT * client, int msg_id);
void parse_send_msg(unsigned char * rcv_data, int rcv_len, int *seq, char *data, int * data_len, int * seq_num, int *total_len)
{

}
void ack_send_msg(CLIENT *client,int msg_id, int seq);
void ack_log_off(CLIENT *client, int msg_id);
void notify(CLIENT * client);
void ack_log_in(CLIENT * client, int msg_id);
CLIENT * update_client_4_log_in(string name, struct sockaddr_in sin);
void * monitor_server(void * para);
void * send_msg(void * para);
void do_business();

void init() {
	struct sockaddr_in sin;

	int ret = -1;

    bzero(&sin, sizeof(sin));

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(port);

	sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (-1 == sock_fd) {
		perror("call to socket");
		exit(1);
	}
	//bind socket
	ret = bind(sock_fd, (struct sockaddr *) &sin, sizeof(sin));
	if (-1 == ret) {
			perror("call to bind");
			exit(1);
	}
}


int main()
{

    //SERVER = (char *)malloc(16);
    //memcpy(server, "server", 16);
	int ret;
	pthread_t id_1,id_2;
    init();
	/*�����߳�һ*/
	ret=pthread_create(&id_1,NULL,monitor_server,NULL);
	ret=pthread_create(&id_2,NULL,send_msg,NULL);
	do_business();

	return 0;
}

void do_business()
{
	char buf[MAXLINE];
	socklen_t address_size = sizeof(struct sockaddr_in);
	struct sockaddr_in rin;
	char str[MIDLINE];//store ip which  readable
	int len  = 0;
   
	while(1)
	{
		len = -1;
		address_size = sizeof(rin);
		len = recvfrom(sock_fd, buf, MAXLINE, 0, (struct sockaddr *) &rin, &address_size);
		if (-1 == len) {
            printf("call to recvfrom.no data\n");
            //usleep(1000*100);
            continue;
		}
		printf("you ip is %s at port %d, len:%d\n",	inet_ntop(AF_INET, &rin.sin_addr, str, sizeof(str)),
												ntohs(rin.sin_port),
												len
												);
        
    
		//�ַ���Ϣ,buf��ֻ�����һ����
		dispatch(rin, buf, len);
	}
}

void dispatch(struct sockaddr_in rin, char *buf, int len)
{
	//�������ݣ��ָ����ݰ�
	int i = 0;
	PACKET * rcv_pack = (PACKET *) buf;

    /*for test*/
    rcv_pack->out_put();
    /*for test*/

	int pack_size = sizeof(PACKET);
	//check
	if((rcv_pack->len + pack_size != len)
			||(rcv_pack->head != 0xFF)
			||(rcv_pack->order < LOG_IN)
			||(rcv_pack->order > ORDER_NUM))

	{
		//�����ݴ���
		return;
	}


	//real dispatch order

	switch(rcv_pack->order)
	{
		case LOG_IN:
			/*
			 * 1.�ж��Ƿ�����,����Ѿ����߲�������
			 * 2.�����м���������Ϣ
			 * 3.����sockaddr_in
			 * 4.���¿ͻ�������������ʱ��
			 * */
			//���ͻ����Ƿ����
            {
			string name= rcv_pack->from;

			deal_log_in( name, rin, rcv_pack->msg_id);
			break;
            }

        case LOG_OFF:
			/*
			 * 1.����û���
			 * 2.�ж��û��Ƿ����ߣ���������ߣ������κδ���
			 * 3.�ж��Ƿ���push��Ϣ�ڴ��䣬����У������������Ϣ���
			 * 4.����������״̬
			 * */
            {
			string name= rcv_pack->from;
			deal_log_off(name, rin, rcv_pack->msg_id);
			break;
            }
		case PULL_MSG:
			/*�յ��ͻ�����ȡ��Ϣ���е���Ϣ
			 * 1.ȷ�Ͽͻ�����ע�ᣬ
			 * 2.ȷ����ȡ��δ��ʼ������sockaddr
			 * 3.�����ݰ���Ƭ�����ڴ���
			 * 4.���뷢�Ͷ��У��ɷ����߳̽������ݷ���
			 * 5.���¿ͻ�������������ʱ��
			 *
			 * */
            {
                string name = rcv_pack->from;
                deal_pull_msg(name, rin);
                break;
            }
		case SEND_MSG:
			/* 1.�յ��ͻ������͵�������Ϣ
			 * 2.���ͻ����Ƿ����,������Ϣ�Ƿ��Ѿ����չ������Ѿ����գ��ظ�ack������sockaddr
			 * 3.�����msgid���б��в����ڣ�Ϊ�洢������������Ϣ����ӳ��ռ�
			 * 4.����������Ϣ
			 * 5.Ϊmsgid��seq��Ӧ����Ϣ�ظ�ack
			 * 6.����msgid��Ӧ����Ϣ�Ƿ�ȫ���������,���������ϣ�������dump�ڴ��̣���msgid��Ϊ�������״̬�����ӳ��ռ�(�ɱ���)
			 * 7.���¿ͻ�������������ʱ��
			 * */
            {
			    string name = rcv_pack->from;
			    deal_send_msg(rcv_pack, rin);
			    break;
            }
		case KEEP_ALIVE:
			/*1.�յ��ͻ������͵�������Ϣ
			 * 2.���ͻ����Ƿ���ڡ�
			 * 3.����sockaddr
			 * 4.���¿ͻ�������������ʱ��
			 * 5.������յ������Ż������ظ�����������˲�������������
			 * */
            {
                string name = rcv_pack->from;
                deal_heartbeat_msg(name, rin);
                break;
            }
		case ACK:
			/*1.����˿����յ�������ack��1��notify��ack 2)push_msg��ack
			 * 2.ȷ��client�Ƿ���ڡ��Ƿ�����
			 * 3.���¿ͻ�������������ʱ�䣬����sockaddr
			 * 4.�����notify��ack:
			 * 	  1)�����ѷ���notify
			 * 5.�����push_msg��ack
			 *    1)���¶�Ӧmsgid��seq��ӳ�����״̬��ʱ��
			 *    2)���msgid��Ӧ����Ϣ�Ƿ�ȫ��������ϣ����������ϣ���������գ���msgid��Ϊ�������״̬�����ӳ��ռ�(�ɱ���)
			 * */
            {
                string name = rcv_pack->from;
                deal_ack_msg(rcv_pack,rin);
                break;
            }
	}

}

void*  monitor_server(void * para)
{
	while(1)
	{
		/* 1.���ͻ���״̬
		 *  ��������ǰʱ��-�ͻ����ϴ�����ʱ��>15s�����ͻ���������
		 * 2.���ͻ��˵�ϵͳ��Ϣ����,�ҵ�Ҫ���͵����ݷ���ȫ�ַ��Ͷ���
		 *   a)�����ack��Ϣ�����ͺ�ֱ��ɾ��
		 *   b)�����������Ϣ�����ͺ�ֱ��ɾ��
		 *   c)�����notify��ȷ�Ͻ��յ�ack��ɾ��
		 * 3.���ͻ��˵�������Ϣ���У��ҵ��Ѿ�ȫ���յ�ack����Ϣ����ɾ��
		 *
		 * */

		map<string, CLIENT *>::iterator client_it;//���client
		for(client_it=client_map.begin();client_it!=client_map.end();++client_it)
		{
			CLIENT * client = client_it->second;

            if(client->is_on_line == 0)
                continue;

            /*fot test*/
            client->output();
            /*for test*/

            //��ǰʱ��-�ϴν�������ʱ�� > 15s
			if(9999 - client->last_recv_keep_alive_time > 15)
			{
				//���ͻ���������
                struct sockaddr_in rin;
				off_client(client->name, rin);
			}

			/*
			 * ϵͳ��Ϣ���з���*/
			client->push_sys_msg_2_queue();

			//������Ϣ����
			/*�����Ϣ�Ƿ������
			 * �Ƿ�������������Ϣ������֮��ŷ���
			 *
			 */

			client->push_com_msg_2_queue();

		}

		//����10ms
		usleep(2000*1000);
	}
    return (void *)0;
}

void main_output()
{
    printf("\nmain monitor:\n");
    printf("global_send_queue size:[%d]\n", global_send_queue.size());
}

void* send_msg(void * para)
{
	//��ȡȫ�ַ��Ͷ��У���������.
	while(1)
	{
		SEND_MSG_POS *smp = global_send_queue.front();
        
		if(smp == NULL)
		{
			usleep(100*1000);//û�пɷ��͵����ݣ���Ϣһ��,100ms
			continue;
		}
        global_send_queue.pop();
		CLIENT * client = check_client(smp->name);
		if(client == NULL)
		{
			//log error;
			printf("client:[%s] is null\n", smp->name);
			continue;
		}
  
		switch(smp->type)
		{
		case COM_MSG:
			create_and_send_com_packet(client, smp->msg_id);
			break;
		case SYS_MSG:
			create_and_send_system_packet( client,  smp->msg_id);
			break;
		}
	}
    return (void *)0;
}
//����˴��������Ϣ
void create_and_send_com_packet(CLIENT *client, int msg_id)
{
	SEND_MSG_MAP * smm = NULL;
	smm = client->get_send_msg_by_id(msg_id);
	if(smm == NULL)
	{
		//log error
		return;
	}

	//�����������Ϣ��ͷ
	PACKET packet;
	packet.init(smm->order, smm->size, smm->msg_id, smm->to);
	//����������Ϣ������������
	for(int i = 0; i < smm->seq_num; i++)
	{
		SEND_MSG_SEQ * sms = smm->get_send_msg_by_seq(i);

		//��ǰ���ͷ�Ƭ�Ƿ��Ѿ�����ack
		if(sms->is_recv_ack == 0)
		{
			/*�жϵ�ǰ��Ƭ�Ƿ���Ҫ�ط�
			 * ���ʱ��2s��û�յ����ط�
			 * С��2s���ȴ��´μ��
			 * */
			if(9999 - sms->last_send_msg_time < 2)
				continue;
			int send_len = sizeof(PACKET) + sms->len;
			char * send_msg = (char *)malloc(send_len);
			memcpy(send_msg, &packet, sizeof(PACKET));
			memcpy(send_msg+sizeof(PACKET), sms->data, sms->len);

			int ret = sendto(sock_fd, send_msg, send_len, 0, (struct sockaddr *) &(client->sin),sizeof(client->sin));
			free(send_msg);
			if(ret == -1)
			{
				perror("error call to sendto\n");
				return;
			}
			sms->retry_send_times++;
			sms->last_send_msg_time=9999;//�ϴη�����Ϣʱ��
		}
	}


}
//����˴������system��Ϣ
void create_and_send_system_packet(CLIENT * client, int msg_id)
{
	SYSTEM_MSG_MAP * sys_smm = NULL;
	sys_smm = client->get_send_sys_msg_by_id(msg_id);
	if(sys_smm == NULL)
	{
		//log error
		return;
	}

	PACKET pack;
	pack.init(sys_smm->order, sys_smm->size, sys_smm->msg_id, sys_smm->to);
    /*for test begin*/
    printf("send sys msg:");
    pack.out_put();
    /*for test end*/
	/*�����ݰ�ƴװ��һ�����*/
	int send_len =pack.len;
	char * send_msg = (char *)malloc(sizeof(PACKET) + sys_smm->size);
	memcpy(send_msg, &pack, sizeof(PACKET));
	memcpy(send_msg+sizeof(PACKET), sys_smm->data, sys_smm->size);
    
    /*for test begin*/
    char str[MIDLINE];
    printf("dest ip is %s at port %d, send len:%d\n", inet_ntop(AF_INET, &client->sin.sin_addr, str, sizeof(str)),
												ntohs(client->sin.sin_port),
												send_len
												);
    /*for test end*/

	int ret = sendto(sock_fd, send_msg, send_len, 0, (struct sockaddr *) &(client->sin),sizeof(client->sin));
	free(send_msg);
	if(ret == -1)
	{
		perror("error call to sendto\n");
		return;
	}
	sys_smm->retry_send_times++;
	sys_smm->last_send_msg_time = 9999;//��ǰʱ��
	if(sys_smm->order == ACK)
	{
        sys_smm->is_send_ok = 1;
        sys_smm->is_recv_ack = 1;
        
	}
}

void deal_heartbeat_msg(string name, struct sockaddr_in sin)
{

	CLIENT * client = check_client(name);
	if(client == NULL)
	{
		return;
	}
    memcpy(&client->sin, &sin, sizeof(struct sockaddr_in));
	client->last_recv_keep_alive_time = 9999;//��ǰʱ��
}
/*
 * ���տͻ��˷��͵�����message,*/
void deal_send_msg(PACKET * rcv_pack, struct sockaddr_in sin)
{
	CLIENT * client = check_client(rcv_pack->from);
	if(client == NULL)
	{
		//�ظ�δ��¼
		return;
	}
	//client����
    //lock(client->c_lock);
    memcpy(&client->sin, &sin, sizeof(struct sockaddr_in));
	client->last_recv_keep_alive_time = 9999;//��ǰʱ��
    //unlock(client->c_lock);
	//����send��Ϣ
	int seq = -1;
	char * data = NULL;
	int data_len = -1;
	int total_len = 0;
	int seq_num = 0;
	int has_init = 0;


	//�������������鿴�ǽ��յ�[msg_id, seq]�������洢
	parse_send_msg(rcv_pack->data, rcv_pack->len, &seq, data,
				   &data_len, &seq_num, &total_len);

	//��鷢����Ϣ�����Ƿ񴴽�����ʼ���˸�msgid
	RECV_MSG_MAP * rmm = client->check_recv_msg_map(rcv_pack->msg_id,  & has_init);
	//�����յ���msgid�İ���Ϊ�������ݣ���ʼ��
	if(has_init == 0)
	{
		rmm->init(rcv_pack->msg_id,  SEND_MSG, rcv_pack->from, sizeof(rcv_pack->from),
							     rcv_pack->to, sizeof(rcv_pack->to), total_len, seq_num);
	}

	//Ϊ�ְ�ӳ�俪�ٿռ�
	rmm->add_2_recv_seq_map(seq, data, data_len, &has_init);
	//log is has_init
	ack_send_msg(client,rcv_pack->msg_id, seq);

	//�����Ƿ�ȫ���������
	if(rmm->recv_seq_num == seq_num)
	{
		//���ݻ�д���̣��Ѵ���������recv��ժ��
		char * data = rmm->dump_msg_2_disk(rcv_pack->msg_id);
		client->clear_recv_msg_by_id(rcv_pack->msg_id);

		//���·�Ƭ�����ص�to��send map�ϣ��ȹ��أ����ж�pull_msg��
		CLIENT * to_client = check_client(rcv_pack->to);

		//���һ������msgid
		int to_msg_id = rand()%65536;
		SEND_MSG_MAP * smm = to_client->get_send_msg_nx_by_id(to_msg_id, &has_init);
		if(has_init == 1)
		{
			//log �Ѿ�����ͬ��msg_id,exit
			printf("has same msgid:[%d]\n", to_msg_id);
			exit(0);
		}
		//�����ݷ��뷢��ӳ���ϵ���ݱ���
		smm->add_msg(data, total_len);

		//�ͷ����ݿռ�
		free(data);

		//���to�Ƿ��Ѿ�������pull_msg����������ˣ�ֱ�ӹ��أ����÷���notify
		if(to_client->is_push_msg == 0)
		{
			notify(to_client);
		}
	}
}

void deal_pull_msg(string name , struct sockaddr_in sin)
{
	CLIENT * client = check_client(name);
	if(client == NULL)
	{
		//�ظ�δ��¼
		return;
	}
	//lock(client->c_lock);
	client->sin = sin;//����sock addr
	client->last_recv_keep_alive_time = 9999;//��ǰʱ��
	client->is_push_msg = 1;

	//unlock(client->c_lock);
}

void deal_ack_msg(PACKET * rcv_pack,  struct sockaddr_in sin)
{
	char *data = (char *)rcv_pack->data;
	CLIENT * client = check_client(rcv_pack->from);
	client->sin = sin;
	client->last_recv_keep_alive_time = 9999;//��ǰʱ��
	if(client == NULL)
	{
		//log client Ϊ��
		printf("client:[%s] is null, ignore msg", rcv_pack->from);
		return;
	}

	//ACK_MSG_ID,��ȡmsg_id
	//ACK_MSG_ORDER�� ��ȡmsg����
    //�ȴ��޸�
	int msg_id ;//= data["ACK_MSG_ID"];
	ORDER ack_order;// =(int)data["ACK_MSG_ORDER"];
	//�����notify����Ӧ����notify��Ϣ��Ϊ�Ѿ�����
	if(ack_order == NOTIFY)
	{
		SYSTEM_MSG_MAP * smm = client->get_send_sys_msg_by_id(msg_id);
		if(smm == NULL)
		{
			//log error,msg_idΪ��
			printf("SYSTEM_MSG_MAP is null, msgid:[%d]", msg_id);
		}
		smm->is_recv_ack = 1;
		smm->last_recv_ack_time = 9999;//��ǰʱ��
	}
	else if(ack_order == PUSH_MSG)
	{
		int seq;// = (int)data["ACK_MSG_SEQ"];
		SEND_MSG_MAP * smm = client->get_send_msg_by_id(msg_id);
		if(smm == NULL)
		{
			return;
		}
		SEND_MSG_SEQ * sms = smm->get_send_msg_by_seq(seq);
		sms->is_recv_ack = 1;
		sms->last_recv_ack_time = 9999;//ȡ��ǰʱ��
	}

}

void deal_log_off(string name, struct sockaddr_in sin, int msg_id)
{
	CLIENT * client =	off_client(name, sin);
	//��ack,�����ack����Ϣ����
	if (client != NULL)
		ack_log_off(client, msg_id);
}

void deal_log_in(string name, struct sockaddr_in  rin, int msg_id)
{
	CLIENT * client = update_client_4_log_in(name, rin);

	//����ǰ������Ϣ�����ѷ��ͱ�־λȫ����Ϊ0�������Ҫȫ������
	//send_msg_arr ���ڷ��͵���շ��ͱ��
	ack_log_in(client, msg_id);


	//�ж�send_msg_arr���Ƿ���������Ϣ�� ��notify����ϵͳ��Ϣ����,֪ͨ�ͻ�����pull����

	if(client->send_msg_arr.size() != 0)
	{
		//����notify��Ϣ
		notify(client);

	}
}

CLIENT * off_client(string  name , struct sockaddr_in sin)
{
	CLIENT * client = check_client(name);

	if(client == NULL)
	{
		//log error
		//��ack
		return NULL;
	}
	client->sin = sin;
	client->last_recv_keep_alive_time = 9999;//��ǰʱ��
	if(client->is_on_line == 0)
	{
		//�����ߣ���¼�ͻ����ظ�����������Ϣ

	}
	//����push������Ϣ
	client->is_push_msg = 0;
	//���ڷ��ͺͽ��յ�������Ϣ���
	//client->recv_msg_arr.clear();
	//client->send_msg_arr.clear();
	return client;
}


CLIENT * update_client_4_log_in(string name, struct sockaddr_in rin)
{
	CLIENT * client = check_client(name);
	if(client == NULL)
	{
		client = new CLIENT(name.c_str());
		//client_map����
		//lock(client_map_lock);
		//�����¼��Ϣ
		client_map.insert(pair<string, CLIENT*>(name, client));
		//client_map����
		//unlock(client_map_lock);
	}
	//client ����
	//lock(client->c_lock);
	//�������ݵ��ͻ���
	//��������ʱ��
	client->last_recv_keep_alive_time = 9999;//��ǰʱ��
	client->last_send_keep_alive_time = 0;

	client->is_on_line = 1;//�Ƿ�����
	client->is_push_msg = 0;//�Ƿ���push
	client->login_time = "2000-01-01 00:00:00";
	//client->sin = sin;//����
	memcpy(&client->sin, &rin, sizeof(struct sockaddr_in));
	//client ����
	//unlock(client->c_lock);

	return client;
}

void create_system_msg(CLIENT * client, ORDER order,char * from, char * to, char * buf, int len, MSG_TYPE type)
{

	//�½�ack��Ϣ�ṹ��,��������client��
	SYSTEM_MSG_MAP * smm  =	(SYSTEM_MSG_MAP *)malloc( sizeof(SYSTEM_MSG_MAP));

	//��ʼ��SYSTEM_MSG_MAP
	smm->init( order,  from,  to,  buf, len);


	//smm���뵽client��(�ǵü�����)��
	client->add_sys_msg_by_id(smm->msg_id,smm);


	//����Ϣid���뵽���Ͷ���
	SEND_MSG_POS * smp = (SEND_MSG_POS *)malloc(sizeof(SEND_MSG_POS));
	smp->msg_id = smm->msg_id;
	memcpy(smp->name ,  client->name, 16);
	smp->type = type;


	//Ϊȫ�ַ��Ͷ��м�������������Ϣ�̣߳��ڶ�Ӧ��client�ṹ����Ѱ�Ҷ�Ӧ��data���ݣ�Ȼ���ͣ�
	//����ǲ���Ҫ��Ӧ����Ϣ������������client��ɾ�����������Ҫ��Ӧ����Ϣ����recv�߳��д���
	//pthread_lock(global_send_queue_mutex);
	global_send_queue.push(smp);
	//Ϊȫ�ַ��Ͷ��н���
	//pthread_unlock(global_send_queue_mutex);


}

CLIENT * check_client(string name)
{
	CLIENT * client = NULL;

    map<string, CLIENT*>::iterator iter;
    iter = client_map.find(name);
	if((client=iter->second) == NULL)
	{
		return NULL;
	}
	return client;
}

void ack_send_msg(CLIENT *client,int msg_id, int seq)
{
	string data;
	Json::Value value;
	value["ACK_MSG_ID"] = int(msg_id);
	value["ACK_MSG_ORDER"] = SEND_MSG;
	value["ACK_MSG_SEQ"] = int(seq);

	Json::FastWriter writer;
	data = writer.write( value );
    //��ʱ
    char * buf = (char *)malloc(data.length()+1);
    memset(buf, 0, data.length()+1);
    strcpy(buf, data.c_str());
    //��ʱ����
	int len = data.length();
	create_system_msg(client, ACK, SERVER, client->name, buf, len, SYS_MSG );

}
//�������ߵ�ack
void ack_log_off(CLIENT *client, int msg_id)
{
	string data;
	Json::Value value;
	value["ACK_MSG_ID"] = int(msg_id);
	value["ACK_MSG_ORDER"] = LOG_OFF;

	Json::FastWriter writer;
	data = writer.write( value );
    //��ʱ
    char * buf = (char *)malloc(data.length()+1);
    memset(buf, 0, data.length()+1);
    strcpy(buf, data.c_str());
    //��ʱ����
    int len = data.length();
	create_system_msg(client, ACK, SERVER, client->name,buf, len, SYS_MSG );

}
//���͵�¼��ack
void ack_log_in(CLIENT * client, int msg_id)
{
	string data;
	Json::Value value;
	value["ACK_MSG_ID"] = int(msg_id);
	value["ACK_MSG_ORDER"] = LOG_IN;

	Json::FastWriter writer;
	data = writer.write( value );
    //��ʱ
    char * buf = (char *)malloc(data.length()+1);
    memset(buf, 0, data.length()+1);
    strcpy(buf, data.c_str());
    //��ʱ����
    int len = data.length();

	create_system_msg(client, ACK,SERVER, client->name,buf, len, SYS_MSG );
}
//����֪ͨ��Ϣ
void notify(CLIENT * client)
{
	create_system_msg(client, NOTIFY,SERVER, client->name,NULL, 0, SYS_MSG);
	//������notify�󣬵ȴ��ͻ��˵�pull��Ϣ��client�в����仯
}
