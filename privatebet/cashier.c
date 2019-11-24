#include "bet.h"
#include "cashier.h"
#include "network.h"

struct cashier *cashier_info;

char notariesIP[10][25]={"159.69.23.28","159.69.23.29","159.69.23.30","159.69.23.31"};
int32_t no_of_notaries=4;

void BET_check_notary_status()
{
	int32_t c_subsock,c_pushsock,retval=1;
	
    uint16_t port = 7797+1,cashier_pushpull_port=7901,cashier_pubsub_port=7902;
    char bindaddr[128]/*="ipc:///tmp/bet.ipc"*/,bindaddr1[128]/*="ipc:///tmp/bet1.ipc"*/,hostip[20]; 
	pthread_t cashier_t[4];
	struct cashier *cashier_info;
	cashier_info=calloc(1,sizeof(struct cashier));
		
	for(int i=0;i<no_of_notaries;i++)
	{
		memset(cashier_info,0x00,sizeof(struct cashier));

		BET_transportname(0,bindaddr,notariesIP[i],cashier_pubsub_port);
		c_subsock= BET_nanosock(1,bindaddr,NN_SUB);
		
		BET_transportname(0,bindaddr1,notariesIP[i],cashier_pushpull_port);
		c_pushsock= BET_nanosock(1,bindaddr1,NN_PUSH);

		cashier_info=calloc(1,sizeof(struct cashier));
		
		cashier_info->c_subsock = c_subsock;
		cashier_info->c_pushsock = c_pushsock;

		printf("%s::%d::c_subsock::%d::c_pushsock::%d\n",__FUNCTION__,__LINE__,c_subsock,c_pushsock);
		
		if (OS_thread_create(&cashier_t[i],NULL,(void *)BET_cashier_client_loop,(void *)cashier_info) != 0 )
		{
			printf("\nerror in launching cashier");
			exit(-1);
		}
		
		if(pthread_join(cashier_t[i],NULL))
		{
		printf("\nError in joining the main thread for cashier");
		}
	}
	
}

int32_t BET_send_status(struct cashier *cashier_info)
{
	int32_t retval=1,bytes;
	char *rendered=NULL;
	cJSON *liveInfo=cJSON_CreateObject();

	cJSON_AddStringToObject(liveInfo,"method","live");
	bytes=nn_send(cashier_info->c_pubsock,cJSON_Print(liveInfo),strlen(cJSON_Print(liveInfo)),0);
	if(bytes<0)
		retval=-1;
	
	return retval;
}

int32_t BET_cashier_backend(cJSON *argjson,struct cashier *cashier_info)
{
	char *method=NULL;
	int retval=1;
    if ( (method= jstr(argjson,"method")) != 0 )
    {
    	if(strcmp(method,"live")==0)
		{
			retval=BET_send_status(cashier_info);
		}
    }	
	return retval;
}



void BET_cashier_client_loop(void * _ptr)
{
	
	int32_t recvlen=0,bytes; 
	void *ptr=NULL; 
	cJSON *argjson=NULL; struct cashier* cashier_info= _ptr;
	
	cJSON *liveInfo=cJSON_CreateObject();
	cJSON_AddStringToObject(liveInfo,"method","live");

	printf("\n%s::%d::%s\n",__FUNCTION__,__LINE__,cJSON_Print(liveInfo));
	bytes=nn_send(cashier_info->c_pushsock,cJSON_Print(liveInfo),strlen(cJSON_Print(liveInfo)),0);

	if(bytes<0)
		printf("\nThere is a problem in sending data::%s::%d\n",__FUNCTION__,__LINE__);
	

	while ( cashier_info->c_pushsock>= 0 && cashier_info->c_subsock>= 0 )
    {
		ptr=0;
        if ( (recvlen= nn_recv(cashier_info->c_subsock,&ptr,NN_MSG,0)) > 0 )
        {

		  	char *tmp=clonestr(ptr);
            if ( (argjson= cJSON_Parse(tmp)) != 0 )
            {
            	printf("%s::%d::%s\n",__FUNCTION__,__LINE__,cJSON_Print(argjson));
                free_json(argjson);
				break;
            }
			if(tmp)
				free(tmp);
			if(ptr)
            	nn_freemsg(ptr);
        }

    }
}


void BET_cashier_server_loop(void * _ptr)
{
	int32_t recvlen=0,bytes; 
	void *ptr=NULL; 
	cJSON *msgjson=NULL; struct cashier* cashier_info= _ptr;
    uint8_t flag=1;

	printf("cashier server node started\n");
	
    while ( flag )
    {
        
        if ( cashier_info->c_pubsock >= 0 && cashier_info->c_pullsock >= 0 )
        {
        		ptr=0;
				char *tmp=NULL;
	        	recvlen= nn_recv (cashier_info->c_pullsock, &ptr, NN_MSG, 0);
				if(recvlen>0)
					tmp=clonestr(ptr);
                if ((recvlen>0) && ((msgjson= cJSON_Parse(tmp)) != 0 ))
                {
                	printf("%s::%d::%s\n",__FUNCTION__,__LINE__,cJSON_Print(msgjson));
                    if ( BET_cashier_backend(msgjson,cashier_info) < 0 )
                    {
                    	printf("\nFAILURE\n");
                    	// do something here, possibly this could be because unknown commnad or because of encountering a special case which state machine fails to handle
                    }           
                    if(tmp)
						free(tmp);
					if(ptr)
						nn_freemsg(ptr);
					
                }
                
        }
        
    }
}
