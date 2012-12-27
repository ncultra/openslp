/***************************************************************************/
/*                                                                         */
/* Project:     OpenSLP - OpenSource implementation of Service Location    */
/*              Protocol Version 2                                         */
/*                                                                         */
/* File:        slplib_findsrvs.c                                          */
/*                                                                         */
/* Abstract:    Implementation for SLPFindSrvs() call.                     */
/*                                                                         */
/*-------------------------------------------------------------------------*/
/*                                                                         */
/*     Please submit patches to http://www.openslp.org                     */
/*                                                                         */
/*-------------------------------------------------------------------------*/
/*                                                                         */
/* Copyright (C) 2000 Caldera Systems, Inc                                 */
/* All rights reserved.                                                    */
/*                                                                         */
/* Redistribution and use in source and binary forms, with or without      */
/* modification, are permitted provided that the following conditions are  */
/* met:                                                                    */ 
/*                                                                         */
/*      Redistributions of source code must retain the above copyright     */
/*      notice, this list of conditions and the following disclaimer.      */
/*                                                                         */
/*      Redistributions in binary form must reproduce the above copyright  */
/*      notice, this list of conditions and the following disclaimer in    */
/*      the documentation and/or other materials provided with the         */
/*      distribution.                                                      */
/*                                                                         */
/*      Neither the name of Caldera Systems nor the names of its           */
/*      contributors may be used to endorse or promote products derived    */
/*      from this software without specific prior written permission.      */
/*                                                                         */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS     */
/* `AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT      */
/* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR   */
/* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE CALDERA      */
/* SYSTEMS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, */
/* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT        */
/* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,  */
/* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON       */
/* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT */
/* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE   */
/* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.    */
/*                                                                         */
/***************************************************************************/

#include "slp.h"
#include "libslp.h"

/*-------------------------------------------------------------------------*/
SLPBoolean CallbackSrvRqst(SLPError errorcode, SLPMessage msg, void* cookie)
/*-------------------------------------------------------------------------*/
{
    int             i;
    PSLPHandleInfo  handle      = (PSLPHandleInfo) cookie;

    if(errorcode)
    {
        handle->params.findsrvs.callback((SLPHandle)handle,
                                         0,
                                         0,
                                         errorcode,
                                         handle->params.findsrvs.cookie);
        return 0; 
    }

    if(msg->header.functionid != SLP_FUNCT_SRVRPLY)
    {
        return 1;
    }

    if(msg->body.srvrply.errorcode == 0)
    {
        for(i=0;i<msg->body.srvrply.urlcount;i++)
        {
            /* TODO: Check the authblock */

            /* TRICKY: null terminate the url by setting the authcount to 0 */
            *((char*)(msg->body.srvrply.urlarray[i].url)+msg->body.srvrply.urlarray[i].urllen) = 0;

            if(handle->params.findsrvs.callback((SLPHandle)handle,
                                                msg->body.srvrply.urlarray[i].url,
                                                msg->body.srvrply.urlarray[i].lifetime,
                                                SLP_OK,
                                                handle->params.findsrvs.cookie) == 0)
            {
                return 0;
            }
        }   

        return 1;
    }

    return handle->params.findsrvs.callback((SLPHandle)handle,
                                            0,
                                            0,
                                            msg->body.srvrply.errorcode * -1,
                                            handle->params.findsrvs.cookie);  
}

/*-------------------------------------------------------------------------*/
SLPError ProcessSrvRqst(PSLPHandleInfo handle)
/*-------------------------------------------------------------------------*/
{
    int                 sock;
    struct sockaddr_in  peeraddr;
    int                 bufsize     = 0;
    char*               buf         = 0;
    char*               curpos      = 0;
    SLPError            result      = 0;

    /*-------------------------------------------------------------------*/
    /* determine the size of the fixed portion of the SRVRQST            */
    /*-------------------------------------------------------------------*/
    bufsize  = handle->params.findsrvs.srvtypelen + 2;   /*  2 bytes for len field */
    bufsize += handle->params.findsrvs.scopelistlen + 2; /*  2 bytes for len field */
    bufsize += handle->params.findsrvs.predicatelen + 2; /*  2 bytes for len field */
    bufsize += 2;    /*  2 bytes for spistr len*/

    /* TODO: make sure that we don't exceed the MTU */
    buf = curpos = (char*)malloc(bufsize);
    if(buf == 0)
    {
        result = SLP_MEMORY_ALLOC_FAILED;
        goto FINISHED;
    }

    /*------------------------------------------------------------*/
    /* Build a buffer containing the fixed portion of the SRVRQST */
    /*------------------------------------------------------------*/
    /* service type */
    ToUINT16(curpos,handle->params.findsrvs.srvtypelen);
    curpos = curpos + 2;
    memcpy(curpos,
           handle->params.findsrvs.srvtype,
           handle->params.findsrvs.srvtypelen);
    curpos = curpos + handle->params.findsrvs.srvtypelen;
    /* scope list */
    ToUINT16(curpos,handle->params.findsrvs.scopelistlen);
    curpos = curpos + 2;
    memcpy(curpos,
           handle->params.findsrvs.scopelist,
           handle->params.findsrvs.scopelistlen);
    curpos = curpos + handle->params.findsrvs.scopelistlen;
    /* predicate */
    ToUINT16(curpos,handle->params.findsrvs.predicatelen);
    curpos = curpos + 2;
    memcpy(curpos,
           handle->params.findsrvs.predicate,
           handle->params.findsrvs.predicatelen);
    curpos = curpos + handle->params.findsrvs.predicatelen;
    /* TODO: add spi list stuff here later*/
    ToUINT16(curpos,0);

    /*--------------------------*/
    /* Call the RqstRply engine */
    /*--------------------------*/
    do
    {
        sock = NetworkConnectToDA(handle,
                                  handle->params.findsrvs.scopelist,
                                  handle->params.findsrvs.scopelistlen,
                                  &peeraddr);
        if(sock == -1)
        {
            /* use multicast as a last resort */
            sock = NetworkConnectToMulticast(&peeraddr);
            result = NetworkRqstRply(sock,
                                     &peeraddr,
                                     handle->langtag,
                                     buf,
                                     SLP_FUNCT_SRVRQST,
                                     bufsize,
                                     CallbackSrvRqst,
                                     handle);
            close(sock);
            break;
        }

        result = NetworkRqstRply(sock,
                                 &peeraddr,
                                 handle->langtag,
                                 buf,
                                 SLP_FUNCT_SRVRQST,
                                 bufsize,
                                 CallbackSrvRqst,
                                 handle);
        if(result)
        {
            NetworkDisconnectDA(handle);
        }

    }while(result == SLP_NETWORK_ERROR);

    FINISHED:
    if(buf) free(buf);

    return result;
}   


#ifdef ENABLE_ASYNC_API
/*-------------------------------------------------------------------------*/ 
SLPError AsyncProcessSrvRqst(PSLPHandleInfo handle)
/*-------------------------------------------------------------------------*/
{
    SLPError result = ProcessSrvRqst(handle);
    free((void*)handle->params.findsrvs.srvtype);
    free((void*)handle->params.findsrvs.scopelist);
    free((void*)handle->params.findsrvs.predicate);
    handle->inUse = SLP_FALSE;
    return result;
}
#endif


/*=========================================================================*/
SLPError SLPFindSrvs(SLPHandle  hSLP,
                     const char *pcServiceType,
                     const char *pcScopeList,
                     const char *pcSearchFilter,
                     SLPSrvURLCallback callback,
                     void *pvCookie)
/*                                                                         */
/* Issue the query for services on the language specific SLPHandle and     */
/* return the results through the callback.  The parameters determine      */
/* the results                                                             */
/*                                                                         */
/* hSLP             The language specific SLPHandle on which to search for */
/*                  services.                                              */
/*                                                                         */
/* pcServiceType    The Service Type String, including authority string if */
/*                  any, for the request, such as can be discovered using  */
/*                  SLPSrvTypes(). This could be, for example              */
/*                  "service:printer:lpr" or "service:nfs".  May not be    */
/*                  the empty string or NULL.                              */
/*                                                                         */
/*                                                                         */
/* pcScopeList      A pointer to a char containing comma separated list of */
/*                  scope names.  Pass in the NULL or the empty string ""  */
/*                  to find services in all the scopes the local host is   */
/*                  configured query.                                      */
/*                                                                         */
/* pcSearchFilter   A query formulated of attribute pattern matching       */
/*                  expressions in the form of a LDAPv3 Search Filter.     */
/*                  If this filter is NULL or empty, i.e.  "", all         */
/*                  services of the requested type in the specified scopes */
/*                  are returned.                                          */
/*                                                                         */
/* callback         A callback function through which the results of the   */
/*                  operation are reported. May not be NULL                */
/*                                                                         */
/* pvCookie         Memory passed to the callback code from the client.    */
/*                  May be NULL.                                           */
/*                                                                         */
/* Returns:         If an error occurs in starting the operation, one of   */
/*                  the SLPError codes is returned.                        */
/*                                                                         */
/*=========================================================================*/
{
    PSLPHandleInfo      handle;
    SLPError            result;

    /*------------------------------*/
    /* check for invalid parameters */
    /*------------------------------*/
    if(hSLP            == 0 ||
       *(unsigned int*)hSLP != SLP_HANDLE_SIG ||
       pcServiceType   == 0 ||
       *pcServiceType  == 0 ||  /* srvtype can't be empty string */
       callback        == 0)
    {
        return SLP_PARAMETER_BAD;
    }


    /*-----------------------------------------*/
    /* cast the SLPHandle into a SLPHandleInfo */
    /*-----------------------------------------*/
    handle = (PSLPHandleInfo)hSLP; 

    /*-----------------------------------------*/
    /* Check to see if the handle is in use    */
    /*-----------------------------------------*/
    if(handle->inUse == SLP_TRUE)
    {
        return SLP_HANDLE_IN_USE;
    }
    handle->inUse = SLP_TRUE;


    /*-------------------------------------------*/
    /* Set the handle up to reference parameters */
    /*-------------------------------------------*/
    handle->params.findsrvs.srvtypelen   = strlen(pcServiceType);
    handle->params.findsrvs.srvtype      = pcServiceType;
    if(pcScopeList && *pcScopeList)
    {
        handle->params.findsrvs.scopelistlen = strlen(pcScopeList);
        handle->params.findsrvs.scopelist    = pcScopeList;
    }
    else
    {
        handle->params.findsrvs.scopelist    = SLPGetProperty("net.slp.useScopes");
        handle->params.findsrvs.scopelistlen = strlen(handle->params.findsrvs.scopelist);
    }

    if(pcSearchFilter)
    {
        handle->params.findsrvs.predicatelen = strlen(pcSearchFilter);
        handle->params.findsrvs.predicate    = pcSearchFilter;
    }
    else
    {
        handle->params.findsrvs.predicatelen = 0;
        handle->params.findsrvs.predicate  = (char*)&handle->params.findsrvs.predicatelen;
    }
    handle->params.findsrvs.callback     = callback;
    handle->params.findsrvs.cookie       = pvCookie; 


    /*----------------------------------------------*/
    /* Check to see if we should be async or sync   */
    /*----------------------------------------------*/
#ifdef ENABLE_ASYNC_API
    if(handle->isAsync)
    {
        /* COPY all the referenced parameters */
        handle->params.findsrvs.srvtype = strdup(handle->params.findsrvs.srvtype);
        handle->params.findsrvs.scopelist = strdup(handle->params.findsrvs.scopelist);
        handle->params.findsrvs.predicate = strdup(handle->params.findsrvs.predicate);

        /* make sure strdups did not fail */
        if(handle->params.findsrvs.srvtype &&
           handle->params.findsrvs.scopelist &&
           handle->params.findsrvs.predicate)
        {
            result = ThreadCreate((ThreadStartProc)AsyncProcessSrvRqst,handle);
        }
        else
        {
            result = SLP_MEMORY_ALLOC_FAILED;    
        }

        if(result)
        {
            if(handle->params.findsrvs.srvtype) free((void*)handle->params.findsrvs.srvtype);
            if(handle->params.findsrvs.scopelist) free((void*)handle->params.findsrvs.scopelist);
            if(handle->params.findsrvs.predicate) free((void*)handle->params.findsrvs.predicate);
            handle->inUse = SLP_FALSE;
        }
    }
    else
#endif /* ifdef ENABLE_ASYNC_API */
    {
        /* Leave all parameters REFERENCED */

        result = ProcessSrvRqst(handle);

        handle->inUse = SLP_FALSE;
    }

    return result;
}

