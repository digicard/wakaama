/*******************************************************************************
 *
 * Copyright (c) 2017, 2018 Digicard Sistemas
 * All rights reserved.
 *
 * Castro Pizzo, Julián Agustín - jcastropizzo@digicard.net
 *    
 *******************************************************************************/

/*
 Copyright (c) 2017, 2018 Digicard Sistemas

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

     * Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
     * Neither the name of Intel Corporation nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 THE POSSIBILITY OF SUCH DAMAGE.

 Julián Agustín Castro Pizzo <jcastropizzo@digicard.net>

*/


#include "liblwm2m.h"
#include "lwm2mclient.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <cjson/cJSON.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_PACKET_SIZE 1024

/*
 * Multiple instance objects can use userdata to store data that will be shared between the different instances.
 * The lwm2m_object_t object structure - which represent every object of the liblwm2m as seen in the single instance
 * object - contain a chained list called instanceList with the object specific structure prv_instance_t:
 */

#define my_free__resource(R) my_free_(R->value);my_free_(R);

//long N=0;


void * my_malloc_(size_t T)
{
    //N+=1;
    //printf("N:\"%ld\" malloc\n",N);
    return malloc(T);
}


void my_free_(void * T)
{
    //N-=1;
    //printf("N:\"%ld\" FREE\n",N);
    return free(T);
}
typedef struct _DSDinamic_ObjResourceList_t
{
    struct _DSDinamic_ObjResourceList_t * next;
    uint16_t resourceId;
    char * resourceName;
    char * resourceType;
    char * resourceInstances;
    char * resourceRequired;
    char * resourceOperations;
} DSDinamic_ObjResourceList_t;

typedef struct 
{
    char * objectName;
    char * objectInstances;
    char * objectMandatory;
    DSDinamic_ObjResourceList_t * res;
} DSDinamic_ObjUserData_t;

enum{
    DS_OK,
    DS_ERROR
};

typedef struct _DSDinamic_resource_t {
    struct _DSDinamic_resource_t * next;
    uint8_t resourceID;
    char * value;
} DSDinamic_resource_t;

typedef struct _DSDinamic_instance_t
{
    /*
     * The first two are mandatories and represent the pointer to the next instance and the ID of this one. The rest
     * is the instance scope user data (uint8_t test in this case)
     */
    struct _DSDinamic_instance_t * next;   // matches lwm2m_list_t::next
    uint16_t shortID;               // matches lwm2m_list_t::id

    char * desc;
    DSDinamic_resource_t * instanceResources;
} DSDinamic_instance_t;


uint16_t DSDinamic_ObjResourceList_countList(
    DSDinamic_ObjResourceList_t * head);
void DSDinamic_ObjResourceList_listRes(
    DSDinamic_ObjResourceList_t * head);
int DS_atoi(
    char * S, 
    uint16_t * dest);

static uint8_t DSDinamic_searchForResource (
    uint16_t resourceID,
    DSDinamic_instance_t * instance, 
    DSDinamic_resource_t * retResource)
{
    /*
     * Coloca el valor del recurso en retResource.
     */
    DSDinamic_resource_t * DS_resource=instance->instanceResources;
    while(DS_resource!=NULL &&DS_resource->resourceID!=resourceID){
        DS_resource=DS_resource->next;
    }
    if (DS_resource==NULL){
        return DS_ERROR;
    }
    if (retResource!=NULL){
        (retResource)->value=my_malloc_(sizeof(char)*(strlen(DS_resource->value)+1));
        strcpy(retResource->value,DS_resource->value);
        retResource->value[strlen(DS_resource->value)]=0;
        retResource->resourceID=DS_resource->resourceID;
        retResource->next=NULL;
    }
    return DS_OK;
}

static uint8_t DSDinamic_writeResource (
    uint16_t resourceID,
    DSDinamic_instance_t * instance, 
    char * data,
    uint16_t dataLen)
{
    /*
     * Escribe el string apuntado en data con el largo dataLen
     */
    DSDinamic_resource_t * DS_resource=instance->instanceResources;

    while(DS_resource!=NULL &&DS_resource->resourceID!=resourceID){
        DS_resource=DS_resource->next;
    }
    if (DS_resource==NULL){
        return DS_ERROR;
    }
    my_free_(DS_resource->value);
    DS_resource->value=my_malloc_(sizeof(char)*(strlen(data)+1));
    strncpy(DS_resource->value,data,dataLen);
    DS_resource->value[dataLen]=0;
    return DS_OK;
}

static uint8_t DSDinamic_addResource (
    uint8_t newResourceID,
    DSDinamic_instance_t * instance, 
    char * val)
{
    /*
     * Agrega el recurso con id newResourceID en la instancia instance y le asigna el valor val
     */
    DSDinamic_resource_t * cursorRes=instance->instanceResources;

    if(cursorRes==NULL)
    {
        instance->instanceResources=my_malloc_(sizeof(DSDinamic_resource_t));
        cursorRes=instance->instanceResources;
        cursorRes->value=my_malloc_(sizeof(char)*(strlen(val)+1));
        cursorRes->next=NULL;
        cursorRes->resourceID=newResourceID;
        strcpy(cursorRes->value,val);
        cursorRes->value[strlen(val)]=0;
        return DS_OK;
    }

    if (DSDinamic_searchForResource(newResourceID,instance,NULL)==DS_OK)
        return DS_ERROR; // se busca el atributo y si existe, no se crea

    while(cursorRes->next!=NULL){
        cursorRes=cursorRes->next;
    }

    cursorRes->next=my_malloc_(sizeof(DSDinamic_resource_t));
    if (cursorRes->next==NULL){
        return DS_ERROR;        
    }

    cursorRes->next->value=my_malloc_(sizeof(char)*(strlen(val)+1));

    if (cursorRes->next->value==NULL){
        my_free_(cursorRes->next);
        return DS_ERROR;        
    }

    cursorRes->next->resourceID=newResourceID;

    strcpy(cursorRes->next->value,val);
    cursorRes->next->value[strlen(val)]=0;
    cursorRes->next->next=NULL;
    return DS_OK;

}

static uint8_t DSDinamic_removeResource (
    uint8_t rmResourceId
    ,DSDinamic_instance_t * instance)
{
    /*
     * Remueve el atributo especificado de la instancia indicada
     */
    DSDinamic_resource_t * cursorRes=instance->instanceResources;
    DSDinamic_resource_t * auxRes;
    if(DSDinamic_searchForResource(rmResourceId,instance,NULL)==DS_ERROR)
        return DS_ERROR;

    if(cursorRes->resourceID==rmResourceId)
    {
        instance->instanceResources=instance->instanceResources->next;
    }
    else
    {
        while(cursorRes->next->resourceID!=rmResourceId)
        {
            cursorRes=cursorRes->next;
        }
        auxRes=cursorRes;
        cursorRes=cursorRes->next;
        auxRes->next=auxRes->next->next;

    }


    my_free_(cursorRes->value);
    my_free_(cursorRes);
    return DS_OK;
}

static uint8_t prv_read(uint16_t instanceId,
    int * numDataP,
    lwm2m_data_t ** dataArrayP,
    lwm2m_object_t * objectP)
{
    DSDinamic_instance_t * targetP;
    uint16_t i;

    targetP = (DSDinamic_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    if (*numDataP == 0)
    {
        *dataArrayP = lwm2m_data_new(1);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = 1;
        (*dataArrayP)[0].id = targetP->shortID;
    }

    for (i = 0 ; i < *numDataP ; i++)
    {
        DSDinamic_resource_t retResource;
        DSDinamic_searchForResource (
            (*dataArrayP)[i].id,
            targetP, 
            &retResource);
        printf("_%s_aaaaaaaaaaa\n",retResource.value);
        lwm2m_data_encode_string(retResource.value,*dataArrayP + i);
        my_free_(retResource.value);
    }
    return COAP_205_CONTENT;
}

static uint8_t prv_discover(uint16_t instanceId,
    int * numDataP,
    lwm2m_data_t ** dataArrayP,
    lwm2m_object_t * objectP)
{
    uint16_t i;
    uint16_t count=DSDinamic_ObjResourceList_countList(
        ((DSDinamic_ObjUserData_t *)(objectP->userData))->res
        );
    DSDinamic_instance_t * targetP;
    targetP = (DSDinamic_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);

    // is the server asking for the full object ?
    if (*numDataP == 0)
    {
        *numDataP=count;
        *dataArrayP = lwm2m_data_new(count);
        for (i = 0; i < count; i++)
        {
            (*dataArrayP)[i].id=i;
        }
    }
    else
    {  
        for (i = 0; i < *numDataP; i++)
        {
            uint16_t aux=(*dataArrayP)[i].id;
            if(DSDinamic_searchForResource (aux,targetP,NULL)==DS_ERROR){
                return COAP_404_NOT_FOUND;
            }
        }
    }

    return COAP_205_CONTENT;
}

void display_DSDinamic(lwm2m_object_t * object)
{
    printf("  /%u: Objeto Dinamico Digicard: \r\n", object->objID);
    printf("\tName: %s\r\n",((DSDinamic_ObjUserData_t *)object->userData)->objectName);
    printf("\tMandatory: %s\r\n",((DSDinamic_ObjUserData_t *)object->userData)->objectMandatory);
    printf("\tInstances: %s\r\n",((DSDinamic_ObjUserData_t *)object->userData)->objectInstances);
    DSDinamic_ObjResourceList_listRes(((DSDinamic_ObjUserData_t *)(object->userData))->res);

}
static uint8_t prv_write(uint16_t instanceId,
 int numData,
 lwm2m_data_t * dataArray,
 lwm2m_object_t * objectP)
{
    DSDinamic_instance_t * targetP;
    int i;
    targetP = (DSDinamic_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;


    if((dataArray->value.asBuffer.buffer)[0]=='J')
    {
        cJSON * root = cJSON_Parse(&((dataArray->value.asBuffer.buffer)[1]));
        cJSON * curr=root->child; 
        while(curr!=NULL) //CHEQUEO QUE TODOS LOS RECURSOS EXISTAN Y SEAN NUMEROS
        {   
            uint16_t resource_id;
            if(DS_atoi(curr->string,&resource_id)){
                return COAP_400_BAD_REQUEST;
            }
            if(DSDinamic_searchForResource (resource_id,targetP,NULL)==DS_ERROR){
                return COAP_404_NOT_FOUND;
            }
            curr=curr->next;
        }
        curr=root->child; 
        while(curr!=NULL) //CHEQUEO QUE TODOS LOS RECURSOS EXISTAN Y SEAN NUMEROS
        {   
            uint16_t resource_id;
            DS_atoi(curr->string,&resource_id);
            
            if(curr->valuestring!=NULL){
                printf("[DS_Dinamic] Changed resource: %d value to: %s \n",resource_id,curr->valuestring);
                DSDinamic_writeResource (resource_id,targetP,curr->valuestring,strlen(curr->valuestring));
            }
            else
            {
                char buff_[MAX_PACKET_SIZE];
                sprintf(buff_, "%lf", curr->valuedouble);
                printf("[DS_Dinamic] Changed resource: %d value to: %s \n",resource_id,buff_);
                DSDinamic_writeResource (resource_id,targetP,buff_,strlen(buff_));               
            }
            curr=curr->next;
        }
        cJSON_Delete(root);
        return COAP_204_CHANGED;
    }
    else
    {
       for (i = 0 ; i < numData ; i++)
       {
           if( DSDinamic_writeResource (
            dataArray[i].id,
            targetP, 
            dataArray->value.asBuffer.buffer, 
            dataArray->value.asBuffer.length)==DS_ERROR)
           {
            return COAP_404_NOT_FOUND;
        }
    }
    return COAP_204_CHANGED;
}

}

static uint8_t prv_delete(uint16_t id,
  lwm2m_object_t * objectP)
{
 /*   DSDinamic_instance_t * targetP;

    objectP->instanceList = lwm2m_list_remove(objectP->instanceList, id, (lwm2m_list_t **)&targetP);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

*/

    return COAP_202_DELETED;
}

static uint8_t prv_create(uint16_t instanceId,
  int numData,
  lwm2m_data_t * dataArray,
  lwm2m_object_t * objectP)
{
    DSDinamic_instance_t * targetP;
    DSDinamic_ObjResourceList_t * objRes;
    uint8_t result;
    int i;
    DSDinamic_resource_t * instRes;

    targetP = (DSDinamic_instance_t *)lwm2m_malloc(sizeof(DSDinamic_instance_t));
    targetP->instanceResources=NULL;
    if (NULL == targetP) return COAP_500_INTERNAL_SERVER_ERROR;
    memset(targetP, 0, sizeof(DSDinamic_instance_t));

    targetP->shortID = instanceId;
    objectP->instanceList = LWM2M_LIST_ADD(objectP->instanceList, targetP);

    objRes=((DSDinamic_ObjUserData_t *)(objectP->userData))->res;

    while(objRes!=NULL)
    {
        DSDinamic_addResource (
            objRes->resourceId,
            targetP, 
            "0");
        objRes=objRes->next;
    }
    instRes = ((DSDinamic_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId))->instanceResources;
    while (instRes!=NULL)
    {
        //printf("Resource N %d value %s \n",instRes->resourceID,instRes->value);
        instRes=instRes->next;
    }
    result = COAP_204_CHANGED;//prv_write(instanceId, numData, dataArray, objectP);

    if (result != COAP_204_CHANGED)
    {
        (void)prv_delete(instanceId, objectP);
    }
    else
    {
        result = COAP_201_CREATED;
    }

    return result;
}

static uint8_t prv_exec(uint16_t instanceId,
    uint16_t resourceId,
    uint8_t * buffer,
    int length,
    lwm2m_object_t * objectP)
{

}




DSDinamic_ObjResourceList_t * DSDinamic_ObjResourceList_createRes(
    cJSON * Res)
{
    DSDinamic_ObjResourceList_t * created_Res=(DSDinamic_ObjResourceList_t *)my_malloc_(sizeof(DSDinamic_ObjResourceList_t));
    uint16_t L;

    DS_atoi(cJSON_GetObjectItemCaseSensitive(Res, "resourceID")->valuestring,&(created_Res->resourceId));

    L=strlen(cJSON_GetObjectItemCaseSensitive(Res, "resourceName")->valuestring);
    created_Res->resourceName=(char *)my_malloc_(sizeof(char)*(L+1));
    strcpy(created_Res->resourceName,cJSON_GetObjectItemCaseSensitive(Res, "resourceName")->valuestring);
    created_Res->resourceName[L]=0;

    L=strlen(cJSON_GetObjectItemCaseSensitive(Res, "resourceType")->valuestring);
    created_Res->resourceType=(char *)my_malloc_(sizeof(char)*(L+1));
    strcpy(created_Res->resourceType,cJSON_GetObjectItemCaseSensitive(Res, "resourceType")->valuestring);
    created_Res->resourceType[L]=0;

    L=strlen(cJSON_GetObjectItemCaseSensitive(Res, "resourceInstances")->valuestring);
    created_Res->resourceInstances=(char *)my_malloc_(sizeof(char)*(L+1));
    strcpy(created_Res->resourceInstances,cJSON_GetObjectItemCaseSensitive(Res, "resourceInstances")->valuestring);
    created_Res->resourceInstances[L]=0;

    L=strlen(cJSON_GetObjectItemCaseSensitive(Res, "resourceRequired")->valuestring);
    created_Res->resourceRequired=(char *)my_malloc_(sizeof(char)*(L+1));
    strcpy(created_Res->resourceRequired,cJSON_GetObjectItemCaseSensitive(Res, "resourceRequired")->valuestring);
    created_Res->resourceRequired[L]=0;

    L=strlen(cJSON_GetObjectItemCaseSensitive(Res, "resourceOperations")->valuestring);
    created_Res->resourceOperations=(char *)my_malloc_(sizeof(char)*(L+1));
    strcpy(created_Res->resourceOperations,cJSON_GetObjectItemCaseSensitive(Res, "resourceOperations")->valuestring);
    created_Res->resourceOperations[L]=0;

    return created_Res;
}

uint16_t DSDinamic_ObjResourceList_deleteRes(
    DSDinamic_ObjResourceList_t * Res)
{
    my_free_(Res->resourceName);
    my_free_(Res->resourceType);
    my_free_(Res->resourceInstances);
    my_free_(Res->resourceRequired);
    my_free_(Res->resourceOperations);
    my_free_(Res);

    return 1;
}

void DSDinamic_ObjResourceList_listRes(
    DSDinamic_ObjResourceList_t * head)
{
    if(head!=NULL)
    {
        DSDinamic_ObjResourceList_t * cursor=head;
        printf("\tResources:\n");
        while(head!=NULL)
        {
            printf("\t\tId: %d\tName: %s\tType: %s\tInstances: %s\tRequired: %s\tOperations: %s \n",
                head->resourceId,head->resourceName,head->resourceType,head->resourceInstances,head->resourceRequired,head->resourceOperations );
            head=head->next;
        }

    }

}

uint16_t DSDinamic_ObjResourceList_countList(
    DSDinamic_ObjResourceList_t * head)
{
    if(head!=NULL)
    {
        uint16_t i=1;
        DSDinamic_ObjResourceList_t * cursor=head;

        while(head->next!=NULL)
        {
            head=head->next;
            i++;
        }
        return i;
    }
    return 0;

}

static DSDinamic_ObjResourceList_t * DSDinamic_ObjResourceList_readRes(
    DSDinamic_ObjResourceList_t * head,
    uint16_t ResId)
{
    if(head==NULL){
        return NULL;
    }
    DSDinamic_ObjResourceList_t * cursor=head;
    while(cursor->resourceId!=ResId)
    {
        cursor=cursor->next;
        if(cursor==NULL)
        {
            return NULL;
        }
    }
    return cursor;
}


static DSDinamic_ObjResourceList_t * DSDinamic_ObjResourceList_addRes(
    DSDinamic_ObjResourceList_t * head,
    cJSON * Res)
{

    if(head==NULL){
        return DSDinamic_ObjResourceList_createRes(Res);
    }

    DSDinamic_ObjResourceList_t * cursor=head;
    uint16_t N;

    DS_atoi(cJSON_GetObjectItemCaseSensitive(Res, "resourceID")->valuestring,&N);

    if(DSDinamic_ObjResourceList_readRes(head,N)!=NULL)
    {
        perror("Already exists an object with the Id: %d \r\n");
        return head;
    }
    DSDinamic_ObjResourceList_t * resourceToAdd=DSDinamic_ObjResourceList_createRes(Res);

    if(cursor->resourceId>resourceToAdd->resourceId)
    {
        resourceToAdd->next=cursor;
        return resourceToAdd;
    }

    DSDinamic_ObjResourceList_t * prevResource=cursor;
    cursor=cursor->next;
    if(cursor==NULL)
    {
        prevResource->next=resourceToAdd;
        resourceToAdd->next=NULL;
        return head;
    }
    while(cursor->resourceId<resourceToAdd->resourceId&&cursor->next!=NULL)
    {
        prevResource=cursor;
        cursor=cursor->next;
    }
    if(cursor->next==NULL)
    {
        cursor->next=resourceToAdd;
        resourceToAdd->next=NULL;
        return head;
    }
    prevResource->next=resourceToAdd;
    resourceToAdd->next=cursor;
    return head;
}

lwm2m_object_t * get_DSDinamic(cJSON * OBJ)
{
    lwm2m_object_t * DSDinamic;
    DSDinamic = (lwm2m_object_t *)lwm2m_malloc(sizeof(lwm2m_object_t));

    if (DSDinamic != NULL)
    {
        cJSON * cJSONCursor;
        int i=0;
        DS_atoi(cJSON_GetObjectItemCaseSensitive(OBJ, "objectID")->valuestring,&(DSDinamic->objID));
        DSDinamic->userData=(void *)my_malloc_(sizeof(DSDinamic_ObjUserData_t));
        if(DSDinamic->userData==NULL){
            fprintf(stderr, "Could not my_malloc_ userdata of: %s\r\n", cJSON_Print(OBJ));
            return NULL;
        }
        ((DSDinamic_ObjUserData_t *)(DSDinamic->userData))->objectName =
        (char *)my_malloc_(sizeof(char)*(strlen(cJSON_GetObjectItemCaseSensitive(OBJ, "objectName")->valuestring)+1));
        strcpy(((DSDinamic_ObjUserData_t *)(DSDinamic->userData))->objectName ,cJSON_GetObjectItemCaseSensitive(OBJ, "objectName")->valuestring);
        ((DSDinamic_ObjUserData_t *)DSDinamic->userData)->objectName[strlen(cJSON_GetObjectItemCaseSensitive(OBJ, "objectName")->valuestring)]=0;

        ((DSDinamic_ObjUserData_t *)(DSDinamic->userData))->objectMandatory =
        (char *)my_malloc_(sizeof(char)*(strlen(cJSON_GetObjectItemCaseSensitive(OBJ, "objectMandatory")->valuestring)+1));
        strcpy(((DSDinamic_ObjUserData_t *)(DSDinamic->userData))->objectMandatory ,cJSON_GetObjectItemCaseSensitive(OBJ, "objectMandatory")->valuestring);
        ((DSDinamic_ObjUserData_t *)DSDinamic->userData)->objectMandatory[strlen(cJSON_GetObjectItemCaseSensitive(OBJ, "objectMandatory")->valuestring)]=0;

        ((DSDinamic_ObjUserData_t *)(DSDinamic->userData))->objectInstances =
        (char *)my_malloc_(sizeof(char)*(strlen(cJSON_GetObjectItemCaseSensitive(OBJ, "objectInstances")->valuestring)+1));
        strcpy(((DSDinamic_ObjUserData_t *)(DSDinamic->userData))->objectInstances ,cJSON_GetObjectItemCaseSensitive(OBJ, "objectInstances")->valuestring);
        ((DSDinamic_ObjUserData_t *)DSDinamic->userData)->objectInstances[strlen(cJSON_GetObjectItemCaseSensitive(OBJ, "objectInstances")->valuestring)]=0;

        cJSONCursor=cJSON_GetObjectItemCaseSensitive(OBJ, "resources");
        cJSONCursor=cJSON_GetArrayItem(cJSONCursor,0);
        while(cJSONCursor!=NULL)
        {
            ((DSDinamic_ObjUserData_t *)(DSDinamic->userData))->res=DSDinamic_ObjResourceList_addRes(((DSDinamic_ObjUserData_t *)(DSDinamic->userData))->res, cJSONCursor);
            cJSONCursor=cJSONCursor->next;
        }

        /*
         * From a single instance object, two more functions are available.
         * - The first one (createFunc) create a new instance and filled it with the provided informations. If an ID is
         *   provided a check is done for verifying his disponibility, or a new one is generated.
         * - The other one (deleteFunc) delete an instance by removing it from the instance list (and my_free_ing the memory
         *   allocated to it)
         */
        DSDinamic->readFunc = prv_read;
        DSDinamic->discoverFunc = prv_discover;
        DSDinamic->writeFunc = prv_write;
        DSDinamic->executeFunc = prv_exec;
        DSDinamic->createFunc = prv_create;
        DSDinamic->deleteFunc = prv_delete;

        for(i=0;i<20;i++)
        prv_create(i,
          0,
          NULL,
          DSDinamic);

        display_DSDinamic(DSDinamic);
    }
    return DSDinamic;
}


int DS_atoi(char * S, uint16_t * dest){
    uint16_t lim=0;
    uint16_t i;
    while(S[lim]!=0){
        if(S[lim]<'0'||S[lim]>'9'){
            return 1;
        }
        lim++;
    }
    i=0;
    *dest=0;
    while(S[i]!=0){
        uint16_t mul=i+1;
        uint16_t num=0;
        num=S[i]-'0';

        while (S[mul]!=0){
            num*=10;
            mul++;
        }
        (*dest)+=num;
        i++;

    }
    return 0;
}


void free_object_DSDinamic(lwm2m_object_t * obj)
{
/*typedef struct _DSDinamic_ObjResourceList_t
{
    struct _DSDinamic_ObjResourceList_t * next;
    uint16_t resourceId;
    char * resourceName;
    char * resourceType;
    char * resourceInstances;
    char * resourceRequired;
    char * resourceOperations;
} DSDinamic_ObjResourceList_t;

typedef struct 
{
    char * objectName;
    char * objectInstances;
    char * objectMandatory;
    DSDinamic_ObjResourceList_t * res;
} DSDinamic_ObjUserData_t;*/
    while(((DSDinamic_ObjUserData_t *)(obj->userData))->res!=NULL)
    {
        DSDinamic_ObjResourceList_t * del=((DSDinamic_ObjUserData_t *)(obj->userData))->res;

        ((DSDinamic_ObjUserData_t *)(obj->userData))->res=((DSDinamic_ObjUserData_t *)(obj->userData))->res->next;
    }
    ((DSDinamic_ObjUserData_t *)(obj->userData))->res;
}