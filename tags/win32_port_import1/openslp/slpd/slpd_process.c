/***************************************************************************/
/*                                                                         */
/* Project:     OpenSLP - OpenSource implementation of Service Location    */
/*              Protocol Version 2                                         */
/*                                                                         */
/* File:        slpd_process.c                                             */
/*                                                                         */
/* Abstract:    Processes incomming SLP messages                           */
/*                                                                         */
/*-------------------------------------------------------------------------*/
/*                                                                         */
/* Copyright (c) 1995, 1999  Caldera Systems, Inc.                         */
/*                                                                         */
/* This program is free software; you can redistribute it and/or modify it */
/* under the terms of the GNU Lesser General Public License as published   */
/* by the Free Software Foundation; either version 2.1 of the License, or  */
/* (at your option) any later version.                                     */
/*                                                                         */
/*     This program is distributed in the hope that it will be useful,     */
/*     but WITHOUT ANY WARRANTY; without even the implied warranty of      */
/*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the       */
/*     GNU Lesser General Public License for more details.                 */
/*                                                                         */
/*     You should have received a copy of the GNU Lesser General Public    */
/*     License along with this program; see the file COPYING.  If not,     */
/*     please obtain a copy from http://www.gnu.org/copyleft/lesser.html   */
/*                                                                         */
/*-------------------------------------------------------------------------*/
/*                                                                         */
/*     Please submit patches to http://www.openslp.org                     */
/*                                                                         */
/***************************************************************************/

#include "slpd.h"

/*-------------------------------------------------------------------------*/
void ProcessSrvRqstError(SLPMessage message, SLPBuffer result, int errorcode)
/*-------------------------------------------------------------------------*/
{
    int size;

    /*--------------------------------*/
    /* Send back a SrvRply with error */
    /*--------------------------------*/
    if(message->header.flags & SLP_FLAG_MCAST)
    {
        /* drop multicast SrvRqsts we can not answer */
        result->end = result->start;   
    }
    else
    {
        /*-------------------------------------------------------------*/
        /* ensure the buffer is big enough to handle the whole srvrply */
        /*-------------------------------------------------------------*/
        size = message->header.langtaglen + 18; /* 14 bytes for header     */
                                                /*  2 bytes for error code */
                                                /*  2 bytes for url count  */ 
    
        result = SLPBufferRealloc(result,size);
        if(result == 0)
        {
            /* TODO: out of memory, should we do anything else here! */
            result->end = result->start; 
            return;               
        }

        /*----------------*/
        /* Add the header */
        /*----------------*/
        /*version*/
        *(result->start)       = 2;
        /*function id*/
        *(result->start + 1)   = SLP_FUNCT_SRVRPLY;
        /*length*/
        ToUINT24(result->start + 2, size);
        /*flags*/
        ToUINT16(result->start + 5,
        size > SLP_MAX_DATAGRAM_SIZE ? SLP_FLAG_OVERFLOW : 0);
        /*ext offset*/
        ToUINT24(result->start + 7,0);
        /*xid*/
        ToUINT16(result->start + 10,message->header.xid);
        /*lang tag len*/
        ToUINT16(result->start + 12,message->header.langtaglen);
        /*lang tag*/
        memcpy(result->start + 14,
        message->header.langtag,
        message->header.langtaglen);

        /*------------------------------------------*/
        /* Add rest of the SrvRply (with errorcode) */
        /*------------------------------------------*/
        result->curpos = result->start + 14 + message->header.langtaglen;
        /* error code*/
        ToUINT16(result->curpos, errorcode);
        result->curpos = result->curpos + 2;
        /* urlentry count */
        ToUINT16(result->curpos, 0);
        result->curpos = result->curpos + 2;
    }

}

/*-------------------------------------------------------------------------*/
void ProcessSASrvRqst(SLPDPeerInfo* peerinfo,
		      SLPMessage message,
		      SLPBuffer result)
/*-------------------------------------------------------------------------*/
{
    int size = 0;
    
    if(G_SlpdProperty.isDA)
    {
        /*----------------------------------------------------------------*/
        /* Do not send error codes or empty replies to multicast requests */
        /*----------------------------------------------------------------*/
        if(message->header.flags & SLP_FLAG_MCAST)
        {
            result->end = result->start;
            return;
        }   

	ProcessSrvRqstError(message, result, SLP_ERROR_MESSAGE_NOT_SUPPORTED);
	return;
    }
   
    if(message->body.srvrqst.scopelistlen == 0 ||
       SLPIntersectStringList(message->body.srvrqst.scopelistlen,
                              message->body.srvrqst.scopelist,
                              G_SlpdProperty.useScopesLen,
                              G_SlpdProperty.useScopes) != 0 )
    {
        /*----------------------*/
        /* Send back a SAAdvert */
        /*----------------------*/
        
        /*-------------------------------------------------------------*/
        /* ensure the buffer is big enough to handle the whole srvrply */
        /*-------------------------------------------------------------*/
        size = message->header.langtaglen + 21; /* 14 bytes for header     */
                                                /*  2 bytes for url count  */
                                                /*  2 bytes for scope list len */
                                                /*  2 bytes for attr list len */
                                                /*  1 byte for authblock count */
        size += G_SlpdProperty.myUrlLen;
        size += G_SlpdProperty.useScopesLen;
        /* TODO: size += G_SlpdProperty.SAAttributes */
       
        result = SLPBufferRealloc(result,size);
        if(result == 0)
        {
	    /* TODO: out of memory, what should we do here! */
	    return;
        }

        /*----------------*/
        /* Add the header */
        /*----------------*/
        /*version*/
        *(result->start)       = 2;
        /*function id*/
        *(result->start + 1)   = SLP_FUNCT_SAADVERT;
        /*length*/
        ToUINT24(result->start + 2, size);
        /*flags*/
        ToUINT16(result->start + 5,
        size > SLP_MAX_DATAGRAM_SIZE ? SLP_FLAG_OVERFLOW : 0);
        /*ext offset*/
        ToUINT24(result->start + 7,0);
        /*xid*/
        ToUINT16(result->start + 10,message->header.xid);
        /*lang tag len*/
        ToUINT16(result->start + 12,message->header.langtaglen);
        /*lang tag*/
        memcpy(result->start + 14,
               message->header.langtag,
               message->header.langtaglen);
        
        /*--------------------------*/
        /* Add rest of the SAAdvert */
        /*--------------------------*/
        result->curpos = result->start + 14 + message->header.langtaglen;
        /* url len */
        ToUINT16(result->curpos, G_SlpdProperty.myUrlLen);
        result->curpos = result->curpos + 2;
        /* url */
        memcpy(result->curpos,G_SlpdProperty.myUrl,G_SlpdProperty.myUrlLen);
        result->curpos = result->curpos + G_SlpdProperty.myUrlLen;
        /* scope list len */
        ToUINT16(result->curpos, G_SlpdProperty.useScopesLen);
        result->curpos = result->curpos + 2;
        /* scope list */
        memcpy(result->curpos,G_SlpdProperty.useScopes,G_SlpdProperty.useScopesLen);
        result->curpos = result->curpos + G_SlpdProperty.useScopesLen;
        /* attr list len */
        /* ToUINT16(result->curpos,G_SlpdProperty.SAAttributesLen) */
        ToUINT16(result->curpos, 0);
        result->curpos = result->curpos + 2;
        /* attr list */
        /* memcpy(result->start,G_SlpdProperty.SAAttributes,G_SlpdProperty.SAAttributesLen) */
        /* authblock count */
        *(result->curpos) = 0;
    }
    else
    {
        ProcessSrvRqstError(message, result, SLP_ERROR_SCOPE_NOT_SUPPORTED);
    }
}


/*-------------------------------------------------------------------------*/
void ProcessDASrvRqst(SLPDPeerInfo* peerinfo,
                      SLPMessage message,
                      SLPBuffer result)
/*-------------------------------------------------------------------------*/
{
    int size = 0;
   
    if(G_SlpdProperty.isDA == 0)
    {
        /*----------------------------------------------------------------*/
        /* Do not send error codes or empty replies to multicast requests */
        /*----------------------------------------------------------------*/
        if(message->header.flags & SLP_FLAG_MCAST)
        {
            result->end = result->start;
            return;
        }   

	ProcessSrvRqstError(message, result, SLP_ERROR_MESSAGE_NOT_SUPPORTED);
	return;
    }

    if(message->body.srvrqst.scopelistlen == 0 ||
       SLPIntersectStringList(message->body.srvrqst.scopelistlen,
                              message->body.srvrqst.scopelist,
                              G_SlpdProperty.useScopesLen,
                              G_SlpdProperty.useScopes) != 0 )
    {
        /*----------------------*/
        /* Send back a DAAdvert */
        /*----------------------*/

        /* increment the value for DATimestamp */
        G_SlpdProperty.DATimestamp += 1;
        if(G_SlpdProperty.DATimestamp == 0) 
        {
           G_SlpdProperty.DATimestamp = 1;
        }  

        /*-------------------------------------------------------------*/
        /* ensure the buffer is big enough to handle the whole srvrply */
        /*-------------------------------------------------------------*/
        size = message->header.langtaglen + 29; /* 14 bytes for header     */
                                                /*  2 errorcode  */
                                                /*  4 bytes for timestamp */
                                                /*  2 bytes for url len */
                                                /*  2 bytes for scope list len */
                                                /*  2 bytes for attr list len */
                                                /*  2 bytes for spi str len */
                                                /*  1 byte for authblock count */

        size += G_SlpdProperty.myUrlLen;
        size += G_SlpdProperty.useScopesLen;
        /* TODO:  - we don't use attributes yet */
        /* size += G_SlpdProperty.SAAttributes; */
       
        result = SLPBufferRealloc(result,size);
        if(result == 0)
        {
	        /* TODO: out of memory, what should we do here! */
	        return;
        }

        /*----------------*/
        /* Add the header */
        /*----------------*/
        /*version*/
        *(result->start)       = 2;
        /*function id*/
        *(result->start + 1)   = SLP_FUNCT_DAADVERT;
        /*length*/
        ToUINT24(result->start + 2, size);
        /*flags*/
        ToUINT16(result->start + 5,
        size > SLP_MAX_DATAGRAM_SIZE ? SLP_FLAG_OVERFLOW : 0);
        /*ext offset*/
        ToUINT24(result->start + 7,0);
        /*xid*/
        ToUINT16(result->start + 10,message->header.xid);
        /*lang tag len*/
        ToUINT16(result->start + 12,message->header.langtaglen);
        /*lang tag*/
        memcpy(result->start + 14,
               message->header.langtag,
               message->header.langtaglen);
        
        /*--------------------------*/
        /* Add rest of the DAAdvert */
        /*--------------------------*/
        result->curpos = result->start + 14 + message->header.langtaglen;
        /* error code */
        ToUINT16(result->curpos,0);
        result->curpos = result->curpos + 2;
        /* timestamp */
        ToUINT32(result->curpos,G_SlpdProperty.DATimestamp);
        result->curpos = result->curpos + 4;
        /* url len */                       
        ToUINT16(result->curpos, G_SlpdProperty.myUrlLen);
        result->curpos = result->curpos + 2;
        /* url */
        memcpy(result->curpos,G_SlpdProperty.myUrl,G_SlpdProperty.myUrlLen);
        result->curpos = result->curpos + G_SlpdProperty.myUrlLen;
        /* scope list len */
        ToUINT16(result->curpos, G_SlpdProperty.useScopesLen);
        result->curpos = result->curpos + 2;
        /* scope list */
        memcpy(result->curpos,G_SlpdProperty.useScopes,G_SlpdProperty.useScopesLen);
        result->curpos = result->curpos + G_SlpdProperty.useScopesLen;
        /* attr list len */
        ToUINT16(result->curpos, 0);
        result->curpos = result->curpos + 2;
        /* attr list */
        /* memcpy(result->start,G_SlpdProperty.DAAttributes,G_SlpdProperty.DAAttributesLen) */
        /* result->curpos = result->curpos + G_SlpdProperty.DAAttributesLen */
        /* SPI List */
        ToUINT16(result->curpos,0);
        result->curpos = result->curpos + 2;
        /* authblock count */
        *(result->curpos) = 0;
        result->curpos = result->curpos + 1;
    }
    else
    {
        ProcessSrvRqstError(message, result, SLP_ERROR_SCOPE_NOT_SUPPORTED);
    }
}


/*-------------------------------------------------------------------------*/
void ProcessSrvRqst(SLPDPeerInfo* peerinfo,
                    SLPMessage message,
                    SLPBuffer result)
/*-------------------------------------------------------------------------*/
{
    int                     i;
    int                     size        = 0;
    int                     count       = 0;
    int                     found       = 0;
    SLPDDatabaseSrvUrl*     srvarray    = 0;
    int                     errorcode   = 0;
    

    /*-------------------------------------------------*/
    /* Check for one of our IP addresses in the prlist */
    /*-------------------------------------------------*/
    if(SLPIntersectStringList(message->body.srvrqst.prlistlen,
                              message->body.srvrqst.prlist,
                              G_SlpdProperty.interfacesLen,
                              G_SlpdProperty.interfaces))
    {
        result->end = result->start;
        goto FINISHED;
    }

    /*------------------------------------------------*/
    /* Check to to see if a this is a special SrvRqst */
    /*------------------------------------------------*/
    if(SLPCompareString(message->body.srvrqst.srvtypelen,
			message->body.srvrqst.srvtype,
			23,
			"service:directory-agent") == 0)
    {
        ProcessDASrvRqst(peerinfo,
                         message,
                         result);
        goto FINISHED;
    }
    if(SLPCompareString(message->body.srvrqst.srvtypelen,
			message->body.srvrqst.srvtype,
			21,
			"service:service-agent") == 0)
    {
        ProcessSASrvRqst(peerinfo,
			 message,
			 result);
        goto FINISHED;
    }
   
    /*------------------------------------*/
    /* Make sure that we handle the scope */
    /*------ -----------------------------*/
    if(SLPIntersectStringList(message->body.srvrqst.scopelistlen,
                              message->body.srvrqst.scopelist,
                              G_SlpdProperty.useScopesLen,
                              G_SlpdProperty.useScopes) != 0)
    {
        /*-------------------------------*/
        /* Find services in the database */
        /*-------------------------------*/
        while(found == count)
        {
            count += SLPDPROCESS_RESULT_COUNT;
        
            if(srvarray) free(srvarray);
            srvarray = (SLPDDatabaseSrvUrl*)malloc(sizeof(SLPDDatabaseSrvUrl) * count);
            if(srvarray == 0)
            {
                found       = 0;
                errorcode   = SLP_ERROR_INTERNAL_ERROR;
                break;
            }
        
            found = SLPDDatabaseFindSrv(&(message->body.srvrqst), srvarray, count);
            if(found < 0)
            {
                found = 0;
                errorcode   = SLP_ERROR_INTERNAL_ERROR;
                break;
            }
        }
    }
    else
    { 
        errorcode = SLP_ERROR_SCOPE_NOT_SUPPORTED;
    }
   
    /*----------------------------------------------------------------*/
    /* Do not send error codes or empty replies to multicast requests */
    /*----------------------------------------------------------------*/
    if(found <= 0 && (message->header.flags & SLP_FLAG_MCAST))
    {
        result->end = result->start;
        goto FINISHED;
    }   


    /*-------------------------------------------------------------*/
    /* ensure the buffer is big enough to handle the whole srvrply */
    /*-------------------------------------------------------------*/
    size = message->header.langtaglen + 18; /* 14 bytes for header     */
                                            /*  2 bytes for error code */
                                            /*  2 bytes for url count  */ 
    for(i=0;i<found;i++)
    {
        size += srvarray[i].urllen + 6; /*  1 byte for reserved  */
                                                /*  2 bytes for lifetime */
                                                /*  2 bytes for urllen   */
                                                /*  1 byte for authcount */
        
        /* TODO: Fix this for authentication */
    } 
    result = SLPBufferRealloc(result,size);
    if(result == 0)
    {
        found = 0;
        errorcode = SLP_ERROR_INTERNAL_ERROR;
    }

    
    /*----------------*/
    /* Add the header */
    /*----------------*/
    /*version*/
    *(result->start)       = 2;
    /*function id*/
    *(result->start + 1)   = SLP_FUNCT_SRVRPLY;
    /*length*/
    ToUINT24(result->start + 2, size);
    /*flags*/
    ToUINT16(result->start + 5,
             size > SLP_MAX_DATAGRAM_SIZE ? SLP_FLAG_OVERFLOW : 0);
    /*ext offset*/
    ToUINT24(result->start + 7,0);
    /*xid*/
    ToUINT16(result->start + 10,message->header.xid);
    /*lang tag len*/
    ToUINT16(result->start + 12,message->header.langtaglen);
    /*lang tag*/
    memcpy(result->start + 14,
           message->header.langtag,
           message->header.langtaglen);
    
    
    /*-------------------------*/
    /* Add rest of the SrvRply */
    /*-------------------------*/
    result->curpos = result->start + 14 + message->header.langtaglen;
    /* error code*/
    ToUINT16(result->curpos, errorcode);
    result->curpos = result->curpos + 2;
    /* urlentry count */
    ToUINT16(result->curpos, found);
    result->curpos = result->curpos + 2;
    for(i=0;i<found;i++)
    {
        /* url-entry reserved */
        *result->curpos = 0;        
        result->curpos = result->curpos + 1;
        /* url-entry lifetime */
        ToUINT16(result->curpos,srvarray[i].lifetime);
        result->curpos = result->curpos + 2;
        /* url-entry urllen */
        ToUINT16(result->curpos,srvarray[i].urllen);
        result->curpos = result->curpos + 2;
        /* url-entry url */
        memcpy(result->curpos,srvarray[i].url,srvarray[i].urllen);
        result->curpos = result->curpos + srvarray[i].urllen;
        /* url-entry authcount */
        *result->curpos = 0;        
        result->curpos = result->curpos + 1;

        /* TODO: put in authentication stuff too */
    }
    
    FINISHED:   
    if(srvarray) free(srvarray);
}



/*-------------------------------------------------------------------------*/
void ProcessSrvReg(SLPDPeerInfo* peerinfo,
                   SLPMessage message,
                   SLPBuffer result)
/*-------------------------------------------------------------------------*/
{
    int errorcode;

    if(message->header.flags & SLP_FLAG_MCAST)
    {
        /* don't do anything multicast SrvReg (set result empty) */
        result->end = result->start;
        return;
    }

    /*------------------------------------*/
    /* Make sure that we handle the scope */
    /*------ -----------------------------*/
    if(SLPIntersectStringList(message->body.srvreg.scopelistlen,
                              message->body.srvreg.scopelist,
                              G_SlpdProperty.useScopesLen,
                              G_SlpdProperty.useScopes))
    {
        /*-------------------------------*/
        /* TODO: Validate the authblocks */
        /*-------------------------------*/
    
    
        /*---------------------------------*/
        /* put the service in the database */
        /*---------------------------------*/
#ifdef WIN32
      if(SLPDDatabaseReg(&(message->body.srvreg),
                           message->header.flags | SLP_FLAG_FRESH,
                           GetCurrentProcessId(),
                           0) == 0) /* TODO: Find an equivalent to 
                                       user id on Win32*/
#else
      if(SLPDDatabaseReg(&(message->body.srvreg),
                           message->header.flags | SLP_FLAG_FRESH,
                           getpid(),
                           getuid()) == 0)
#endif
        {
            errorcode = 0;
        }
        else
        {
            errorcode = SLP_ERROR_INTERNAL_ERROR;
        }    
    }
    else
    {
        errorcode = SLP_ERROR_SCOPE_NOT_SUPPORTED;
    }

    /*------------------------------------------------------------*/
    /* ensure the buffer is big enough to handle the whole srvack */
    /*------------------------------------------------------------*/
    result = SLPBufferRealloc(result,message->header.langtaglen + 16);


    /*----------------*/
    /* Add the header */
    /*----------------*/
    /*version*/
    *(result->start)       = 2;
    /*function id*/
    *(result->start + 1)   = SLP_FUNCT_SRVACK;
    /*length*/
    ToUINT24(result->start + 2,message->header.langtaglen + 16);
    /*flags*/
    ToUINT16(result->start + 5,0);
    /*ext offset*/
    ToUINT24(result->start + 7,0);
    /*xid*/
    ToUINT16(result->start + 10,message->header.xid);
    /*lang tag len*/
    ToUINT16(result->start + 12,message->header.langtaglen);
    /*lang tag*/
    memcpy(result->start + 14,
           message->header.langtag,
           message->header.langtaglen);
    
    /*-------------------*/
    /* Add the errorcode */
    /*-------------------*/
    ToUINT16(result->start + 14 + message->header.langtaglen, errorcode);
}


/*-------------------------------------------------------------------------*/
void ProcessSrvDeReg(SLPDPeerInfo* peerinfo,
                     SLPMessage message,
                     SLPBuffer result)
/*-------------------------------------------------------------------------*/
{
    int errorcode;

    if(message->header.flags & SLP_FLAG_MCAST)
    {
        /* don't do anything multicast SrvDeReg (set result empty) */
        result->end = result->start;
        return;
    }

    /*------------------------------------------*/
    /* TODO: make sure that we handle the scope */
    /*------------------------------------------*/
    if(SLPIntersectStringList(message->body.srvdereg.scopelistlen,
                              message->body.srvdereg.scopelist,
                              G_SlpdProperty.useScopesLen,
                              G_SlpdProperty.useScopes))
    {
        /*-------------------------------*/
        /* TODO: Validate the authblocks */
        /*-------------------------------*/
        
        /*--------------------------------------*/
        /* remove the service from the database */
        /*--------------------------------------*/
        if(SLPDDatabaseDeReg(&(message->body.srvdereg)) == 0)
        {
             errorcode = 0;
        }
        else
        {
            errorcode = SLP_ERROR_INTERNAL_ERROR;
        }
    }
    else
    {
        errorcode = SLP_ERROR_SCOPE_NOT_SUPPORTED;
    }

    /*------------------------------------------------------------*/
    /* ensure the buffer is big enough to handle the whole srvack */
    /*------------------------------------------------------------*/
    result = SLPBufferRealloc(result,message->header.langtaglen + 16);
    
    /*----------------*/
    /* Add the header */
    /*----------------*/
    /*version*/
    *(result->start)       = 2;
    /*function id*/
    *(result->start + 1)   = SLP_FUNCT_SRVACK;
    /*length*/
    ToUINT24(result->start + 2,message->header.langtaglen + 16);
    /*flags*/
    ToUINT16(result->start + 5,0);
    /*ext offset*/
    ToUINT24(result->start + 7,0);
    /*xid*/
    ToUINT16(result->start + 10,message->header.xid);
    /*lang tag len*/
    ToUINT16(result->start + 12,message->header.langtaglen);
    /*lang tag*/
    memcpy(result->start + 14,
           message->header.langtag,
           message->header.langtaglen);
    
    /*-------------------*/
    /* Add the errorcode */
    /*-------------------*/
    ToUINT16(result->start + 14 + message->header.langtaglen, errorcode);
}



/*-------------------------------------------------------------------------*/
void ProcessSrvAck(SLPDPeerInfo* peerinfo,
                   SLPMessage message,
                   SLPBuffer result)
/*-------------------------------------------------------------------------*/
{
}


/*-------------------------------------------------------------------------*/
void ProcessAttrRqst(SLPDPeerInfo* peerinfo,
                     SLPMessage message,
                     SLPBuffer result)
/*-------------------------------------------------------------------------*/
{
    int                     i;
    int                     attrlistlen = 0;
    int                     size        = 0;
    int                     count       = 0;
    int                     found       = 0;
    SLPDDatabaseAttr*       attrarray   = 0;
    int                     errorcode   = 0;
    

    /*-------------------------------------------------*/
    /* Check for one of our IP addresses in the prlist */
    /*-------------------------------------------------*/
    if(SLPIntersectStringList(message->body.attrrqst.prlistlen,
                              message->body.attrrqst.prlist,
                              G_SlpdProperty.interfacesLen,
                              G_SlpdProperty.interfaces))
    {
        result->end = result->start;
        return;
    }

    /*------------------------------------*/
    /* Make sure that we handle the scope */
    /*------ -----------------------------*/
    if(SLPIntersectStringList(message->body.attrrqst.scopelistlen,
                              message->body.attrrqst.scopelist,
                              G_SlpdProperty.useScopesLen,
                              G_SlpdProperty.useScopes) == 0)
    {
        result->end = result->start;
        return;
    }
    
    /*-------------------------------*/
    /* Find attributes in the database */
    /*-------------------------------*/
    while(found == count)
    {
        count += SLPDPROCESS_RESULT_COUNT;
        
        if(attrarray) free(attrarray);
        attrarray = (SLPDDatabaseAttr*)malloc(sizeof(SLPDDatabaseAttr) * count);
        if(attrarray == 0)
        {
            found       = 0;
            errorcode   = SLP_ERROR_INTERNAL_ERROR;
            break;
        }
        
        found = SLPDDatabaseFindAttr(&(message->body.attrrqst), attrarray, count);
        if(found < 0)
        {
            found = 0;
            errorcode   = SLP_ERROR_INTERNAL_ERROR;
            break;
        }
    }

    /*----------------------------------------------------------------*/
    /* Do not send error codes or empty replies to multicast requests */
    /*----------------------------------------------------------------*/
    if(found <= 0 && (message->header.flags & SLP_FLAG_MCAST))
    {
        if(attrarray) free(attrarray);
        result->end = result->start;
        return;
    }

    /*--------------------------------------------------------------*/
    /* ensure the buffer is big enough to handle the whole attrrply */
    /*--------------------------------------------------------------*/
    size = message->header.langtaglen + 20; /* 14 bytes for header     */
                                            /*  2 bytes for error code */
                                            /*  2 bytes for attr-list len */
                                            /*  2 bytes for the authblockcount */
    for (i=0;i<found;i++)
    {
        attrlistlen += attrarray[i].attrlen;
    }
    size += attrlistlen;
    

    /*-------------------*/
    /* Alloc the  buffer */
    /*-------------------*/
    result = SLPBufferRealloc(result,size);
    if(result == 0)
    {
        found = 0;
        errorcode = SLP_ERROR_INTERNAL_ERROR;
    }

    /*----------------*/
    /* Add the header */
    /*----------------*/
    /*version*/
    *(result->start)       = 2;
    /*function id*/
    *(result->start + 1)   = SLP_FUNCT_ATTRRPLY;
    /*length*/
    ToUINT24(result->start + 2,size);
    /*flags*/
    ToUINT16(result->start + 5,
             size > SLP_MAX_DATAGRAM_SIZE ? SLP_FLAG_OVERFLOW : 0);
    /*ext offset*/
    ToUINT24(result->start + 7,0);
    /*xid*/
    ToUINT16(result->start + 10,message->header.xid);
    /*lang tag len*/
    ToUINT16(result->start + 12,message->header.langtaglen);
    /*lang tag*/
    memcpy(result->start + 14,
           message->header.langtag,
           message->header.langtaglen);
    
    /*--------------------------*/
    /* Add rest of the AttrRqst */
    /*--------------------------*/
    result->curpos = result->start + 14 + message->header.langtaglen;
    /* error code*/
    ToUINT16(result->curpos, errorcode);
    result->curpos = result->curpos + 2;
    /* attr-list len */
    ToUINT16(result->curpos, attrlistlen);
    result->curpos = result->curpos + 2;
    for(i=0;i<found;i++)
    {
        memcpy(result->curpos,attrarray[i].attr,attrarray[i].attrlen);
        result->curpos = result->curpos + attrarray[i].attrlen;
    }
    
    /* TODO: no auth block */
    ToUINT16(result->curpos, 0);

    if(attrarray) free(attrarray);
}        


/*-------------------------------------------------------------------------*/
void ProcessDAAdvert(SLPDPeerInfo* peerinfo,
                     SLPMessage message,
                     SLPBuffer result)
/*-------------------------------------------------------------------------*/
{

    /* TODO: enable the following when we link to libslp and        */
    /* have SLPParseSrvURL()                                        */
    #if(0)
    SLPSrvURL*      srvurl;
    struct hostent* he;
    #endif 
    
    /* DAAdverts should never be replied to.  Set result buffer to empty*/
    result->end = result->start;
    
    /* Do not look at DAAdverts if we are a DA */
    if(G_SlpdProperty.isDA == 0)
    {
    
        /* Only process if errorcode is not set */
        if(message->body.daadvert.errorcode == SLP_ERROR_OK)
        {
            
            /* TODO: enable the following when we link to libslp and        */
            /* have SLPParseSrvURL()                                        */
            #if(0)  
            /* yes, we could just get the host addr from peer info. Looking */
            /* it up is safer                                               */
            if(SLPParseSrvURL(message->body.daadvert.url, &srvurl) == 0)
            {
                he = gethostbyname(srvurl->s_pcHost);
                if(he)
                {
                    /* Add the DA to a list of known DAs (ignore return)*/
                    SLPDKnownDAAdd((struct in_addr*)(he->h_addr_list[0]),
                                   message->body.daadvert.bootstamp,
                                   message->body.daadvert.scopelist,
                                   message->body.daadvert.scopelistlen);
                }
                
                SLPFree(srvurl);
            }
            #endif
            
            /* TODO: the following is allows for easy DA masquarading (unsafe) */
            /*       remove it when the above is fixed                         */
            SLPDKnownDAAdd(&(peerinfo->peeraddr.sin_addr), 
                           message->body.daadvert.bootstamp,
                           message->body.daadvert.scopelist,
                           message->body.daadvert.scopelistlen);
        }
                                   
        /* If necessary log that we received a DAAdvert */
        if(G_SlpdProperty.traceDATraffic)
        {
            SLPDLogDATrafficMsg("IN", peerinfo, message);
        }
    }
}


/*-------------------------------------------------------------------------*/
void ProcessSrvTypeRqst(SLPDPeerInfo* peerinfo,
                        SLPMessage message,
                        SLPBuffer result)
/*-------------------------------------------------------------------------*/
{
}


/*-------------------------------------------------------------------------*/
void ProcessSAAdvert(SLPDPeerInfo* peerinfo,
                     SLPMessage message,
                     SLPBuffer result)
/*-------------------------------------------------------------------------*/
{
}


/*=========================================================================*/
int SLPDProcessMessage(SLPDPeerInfo* peerinfo,
                       SLPBuffer recvbuf,
                       SLPBuffer sendbuf)
/* Processes the recvbuf and places the results in sendbuf                 */
/*                                                                         */
/* recvfd   - the socket the message was received on                       */
/*                                                                         */
/* recvbuf  - message to process                                           */
/*                                                                         */
/* sendbuf  - results of the processed message                             */
/*                                                                         */
/* Returns  - zero on success SLP_ERROR_PARSE_ERROR or                     */
/*            SLP_ERROR_INTERNAL_ERROR on ENOMEM.                          */
/*=========================================================================*/
{
    SLPMessage  message   = 0;
    int         result  = 0;

    message = SLPMessageAlloc();
    if(message == 0)
    {
        return SLP_ERROR_INTERNAL_ERROR;
    }

    result = SLPMessageParseBuffer(recvbuf, message);
    if(result == 0)
    {
       if(G_SlpdProperty.traceMsg)
       {
         SLPDLogTraceMsg("IN",peerinfo,recvbuf);
       }
       
       switch(message->header.functionid)
        {
        case SLP_FUNCT_SRVRQST:
            ProcessSrvRqst(peerinfo,message,sendbuf);
            break;
    
        case SLP_FUNCT_SRVREG:
            ProcessSrvReg(peerinfo,message,sendbuf);
            break;
    
        case SLP_FUNCT_SRVDEREG:
            ProcessSrvDeReg(peerinfo,message,sendbuf);
            break;
    
        case SLP_FUNCT_SRVACK:
            ProcessSrvAck(peerinfo,message,sendbuf);        
            break;
    
        case SLP_FUNCT_ATTRRQST:
            ProcessAttrRqst(peerinfo,message,sendbuf);
            break;
    
        case SLP_FUNCT_DAADVERT:
            ProcessDAAdvert(peerinfo,message,sendbuf);
            break;
    
        case SLP_FUNCT_SRVTYPERQST:
            ProcessSrvTypeRqst(peerinfo,message,sendbuf);
            break;
    
        case SLP_FUNCT_SAADVERT:
            ProcessSAAdvert(peerinfo,message,sendbuf);
            break;
    
        default:
            /* this will NEVER happen */
            break;
        }

        /* Log traceMsg of message was received and the one that will be sent */
        if(G_SlpdProperty.traceMsg)
        {
            SLPDLogTraceMsg("OUT",peerinfo,sendbuf);
        }
    }
    else
    {
        /* TODO: Log drop here? */
    }
    
    SLPMessageFree(message);

    return result;
}                



