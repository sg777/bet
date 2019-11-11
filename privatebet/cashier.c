#include "bet.h"
#include "cashier.h"

struct cashier *cashier_info;

char notariesIP[10][25]={"159.69.23.28","159.69.23.29","159.69.23.30","159.69.23.31"};
int32_t no_of_notaries=4;

void BET_check_notary_status()
{
	int32_t retval=1;

	for(int i=0;i<no_of_notaries;i++)
	{
		printf("%s::%d::%s\n",__FUNCTION__,__LINE__,notariesIP[i]);
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

#if 1
void BET_cashier_loop(void * _ptr)
{
	int32_t recvlen=0,bytes; 
	void *ptr=NULL; 
	cJSON *msgjson=NULL; struct cashier* cashier_info= _ptr;
    uint8_t flag=1;

	
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

#endif
