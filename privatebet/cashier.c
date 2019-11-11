#include "bet.h"
#include "cashier.h"

struct cashier *cashier_info;

int32_t BET_cashier_backend(cJSON *argjson,struct cashier *cashier_info)
{
	char *method=NULL;
	int retval=1;
    if ( (method= jstr(argjson,"method")) != 0 )
    {
    	printf("%s::%d::%s",__FUNCTION__,__LINE__,method);
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

	cJSON *argjson=cJSON_CreateObject();
	cJSON_AddStringToObject(argjson,"method","cashier");
	
	bytes=nn_send(cashier_info->pubsock,cJSON_Print(argjson),strlen(cJSON_Print(argjson)),0);
	if(bytes<0)
		printf("\nThere is a problem in sending the data");
	
    while ( flag )
    {
        
        if ( cashier_info->c_pubsock >= 0 && cashier_info->c_pullsock >= 0 )
        {
        		ptr=0;
				char *tmp=NULL;
	        	recvlen= nn_recv (cashier_info->c_pubsock, &ptr, NN_MSG, 0);
				if(recvlen>0)
					tmp=clonestr(ptr);
                if ((recvlen>0) && ((msgjson= cJSON_Parse(tmp)) != 0 ))
                {
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
