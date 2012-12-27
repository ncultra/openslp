/***************************************************************************/
/*                                                                         */
/* Project:     OpenSLP - OpenSource implementation of Service Location    */
/*              Protocol                                                   */
/*                                                                         */
/* File:        slp_msg.h                                                  */
/*                                                                         */
/* Abstract:    Header file that defines structures and constants that are */
/*              specific to the SLP wire protocol messages.                */
/*                                                                         */
/* Author(s):   Matthew Peterson                                           */
/*                                                                         */
/***************************************************************************/

#include <slp_message.h>

#include <malloc.h>
#include <netinet/in.h>

/*-------------------------------------------------------------------------*/
#define AsUINT16(charptr)   ( ntohs(*((PUINT16)(charptr))) )
#define AsUINT24(charptr)   ( ntohl(*((PUINT32)(charptr)))>>8 )
#define AsUINT32(charptr)   ( ntohl(*((PUINT32)(charptr))) )
/* Macros used to parse SLPBuffers                                         */
/*-------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------*/
int ParseHeader(SLPBuffer buffer, SLPHeader* header)
/*                                                                         */
/* Returns  - Zero on success, SLP_ERROR_VER_NOT_SUPPORTED, or             */
/*            SLP_ERROR_PARSE_ERROR.                                       */
/*-------------------------------------------------------------------------*/
{
    header->version     = *(buffer->curpos);
    header->functionid  = *(buffer->curpos + 1);
    header->length      = AsUINT24(buffer->curpos + 2);
    header->flags       = AsUINT16(buffer->curpos + 5);
    header->extoffset   = AsUINT24(buffer->curpos + 7);
    header->xid         = AsUINT16(buffer->curpos + 10);
    header->langtaglen  = AsUINT16(buffer->curpos + 12);
    header->langtag     = buffer->curpos + 14;

    if(header->version != 2)
    {
        return SLP_ERROR_VER_NOT_SUPPORTED;
    }

    if(header->functionid > SLP_FUNCT_SAADVERT)
    {
        /* invalid function id */
        return SLP_ERROR_PARSE_ERROR;
    }

    if(header->length != buffer->end - buffer->start ||
       header->length < 18)
    {
        /* invalid length 18 bytes is the smallest v2 message*/
        return SLP_ERROR_PARSE_ERROR;
    }

    if(header->flags & 0x1fff)
    {
        /* invalid flags */
        return SLP_ERROR_PARSE_ERROR;
    }

    buffer->curpos = buffer->curpos + header->langtaglen + 14;

    return 0;
}

/*--------------------------------------------------------------------------*/
int ParseAuthBlock(SLPBuffer buffer, SLPAuthBlock* authblock)
/* Returns  - Zero on success, SLP_ERROR_INTERNAL_ERROR (out of memory) or  */
/*            SLP_ERROR_PARSE_ERROR.                                        */
/*--------------------------------------------------------------------------*/
{
    /* make sure that min size is met */
    #if(defined(PARANOID) || defined(DEBUG))
    if(buffer->end - buffer->curpos < 10)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    
    authblock->bsd          = AsUINT16(buffer->curpos);
    authblock->length       = AsUINT16(buffer->curpos + 2);
    
    #if(defined(PARANOID) || defined(DEBUG))
    if(authblock->length > buffer->end - buffer->curpos)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif                           
    
    authblock->timestamp    = AsUINT32(buffer->curpos + 4);
    authblock->spistrlen    = AsUINT16(buffer->curpos + 8);
    authblock->spistr       = buffer->curpos + 10;
    
    #if(defined(PARANOID) || defined(DEBUG))
    if(authblock->spistrlen > buffer->end - buffer->curpos + 10)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif 
    
    authblock->authstruct   = buffer->curpos + authblock->spistrlen + 10;
    
    buffer->curpos = buffer->curpos + authblock->length;
    
    return 0;
}

/*--------------------------------------------------------------------------*/
int ParseUrlEntry(SLPBuffer buffer, SLPUrlEntry* urlentry)
/*                                                                          */
/* Returns  - Zero on success, SLP_ERROR_INTERNAL_ERROR (out of memory) or  */
/*            SLP_ERROR_PARSE_ERROR.                                        */
/*--------------------------------------------------------------------------*/
{
    int             result;
    int             i;
    
    /* make sure that min size is met */
    #if(defined(PARANOID) || defined(DEBUG))
    if(buffer->end - buffer->curpos < 6)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif

    /* parse out reserved */
    urlentry->reserved = *(buffer->curpos);
    buffer->curpos = buffer->curpos + 1;

    /* parse out lifetime */
    urlentry->lifetime = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;

    /* parse out url */
    urlentry->urllen = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;
    #if(defined(PARANOID) || defined(DEBUG))
    if(urlentry->urllen > buffer->end - buffer->curpos)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    
    urlentry->url = buffer->curpos;
    buffer->curpos = buffer->curpos + urlentry->urllen;

    /* parse out auth block count */
    urlentry->authcount = *(buffer->curpos);
    buffer->curpos = buffer->curpos + 1;

    /* parse out the auth block (if any) */
    if(urlentry->authcount)
    {
        urlentry->autharray = (SLPAuthBlock*)malloc(sizeof(SLPAuthBlock) * urlentry->authcount);
        if(urlentry->autharray == 0)
        {
            return SLP_ERROR_INTERNAL_ERROR;
        }
        memset(urlentry->autharray,0,sizeof(SLPAuthBlock) * urlentry->authcount);

        for(i=0;i<urlentry->authcount;i++)
        {
            result = ParseAuthBlock(buffer,&(urlentry->autharray[i]));
            if(result) return result;
        }
    }
        
    return 0;
}


/*--------------------------------------------------------------------------*/
int ParseSrvRqst(SLPBuffer buffer, SLPSrvRqst* srvrqst)
/*--------------------------------------------------------------------------*/
{
    /* make sure that min size is met */
    #if(defined(PARANOID) || defined(DEBUG))
    if(buffer->end - buffer->curpos < 10)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    
    /* parse the prlist */
    srvrqst->prlistlen = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;
    #if(defined(PARANOID) || defined(DEBUG))
    if(srvrqst->prlistlen > buffer->end - buffer->curpos)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    srvrqst->prlist = buffer->curpos;
    buffer->curpos = buffer->curpos + srvrqst->prlistlen;
    

    /* parse the service type */
    srvrqst->srvtypelen = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;
    #if(defined(PARANOID) || defined(DEBUG))
    if(srvrqst->srvtypelen > buffer->end - buffer->curpos)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    srvrqst->srvtype = buffer->curpos;
    buffer->curpos = buffer->curpos + srvrqst->srvtypelen;    


    /* parse the scope list */
    srvrqst->scopelistlen = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;
    #if(defined(PARANOID) || defined(DEBUG))
    if(srvrqst->scopelistlen > buffer->end - buffer->curpos)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    srvrqst->scopelist = buffer->curpos;
    buffer->curpos = buffer->curpos + srvrqst->scopelistlen;    


    /* parse the predicate string */
    srvrqst->predicatelen = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;
    #if(defined(PARANOID) || defined(DEBUG))
    if(srvrqst->predicatelen > buffer->end - buffer->curpos)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    srvrqst->predicate = buffer->curpos;
    buffer->curpos = buffer->curpos + srvrqst->predicatelen;


    /* parse the slpspi string */
    srvrqst->spistrlen = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;
    #if(defined(PARANOID) || defined(DEBUG))
    if(srvrqst->spistrlen > buffer->end - buffer->curpos)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    srvrqst->spistr = buffer->curpos;
    buffer->curpos = buffer->curpos + srvrqst->spistrlen;

    return 0;
}


/*--------------------------------------------------------------------------*/
int ParseSrvRply(SLPBuffer buffer, SLPSrvRply* srvrply)
/*--------------------------------------------------------------------------*/
{
    int             result;
    int             i;
    
    /* make sure that min size is met */
    #if(defined(PARANOID) || defined(DEBUG))
    if(buffer->end - buffer->curpos < 4)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif

    /* parse out the error code */
    srvrply->errorcode = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;

    /* parse out the url entry count */
    srvrply->urlcount = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;

    /* parse out the url entries (if any) */
    if(srvrply->urlcount)
    {
        srvrply->urlarray = (SLPUrlEntry*)malloc(sizeof(SLPUrlEntry) * srvrply->urlcount);
        if(srvrply->urlarray == 0)
        {
           return SLP_ERROR_INTERNAL_ERROR;
        }
        memset(srvrply->urlarray,0,sizeof(SLPUrlEntry) * srvrply->urlcount);
        
        for(i=0;i<srvrply->urlcount;i++)
        {
            result = ParseUrlEntry(buffer,&(srvrply->urlarray[i]));   
            if (result) return result;
        }
    }       

    return 0;
}


/*--------------------------------------------------------------------------*/
int ParseSrvReg(SLPBuffer buffer, SLPSrvReg* srvreg)
/*--------------------------------------------------------------------------*/
{
    int             result;
    int             i;

    /* Parse out the url entry */
    result = ParseUrlEntry(buffer,&(srvreg->urlentry));
    if(result)
    {
        return result;
    }                       
    

    /* parse the service type */
    srvreg->srvtypelen = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;
    #if(defined(PARANOID) || defined(DEBUG))
    if(srvreg->srvtypelen > buffer->end - buffer->curpos)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    srvreg->srvtype = buffer->curpos;
    buffer->curpos = buffer->curpos + srvreg->srvtypelen;    


    /* parse the scope list */
    srvreg->scopelistlen = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;
    #if(defined(PARANOID) || defined(DEBUG))
    if(srvreg->scopelistlen > buffer->end - buffer->curpos)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    srvreg->scopelist = buffer->curpos;
    buffer->curpos = buffer->curpos + srvreg->scopelistlen;    


    /* parse the attribute list*/
    srvreg->attrlistlen = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;
    #if(defined(PARANOID) || defined(DEBUG))
    if(srvreg->attrlistlen > buffer->end - buffer->curpos)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    srvreg->attrlist = buffer->curpos;
    buffer->curpos = buffer->curpos + srvreg->attrlistlen;


    /* parse out attribute auth block count */
    srvreg->authcount = *(buffer->curpos);
    buffer->curpos = buffer->curpos + 1;

    /* parse out the auth block (if any) */
    if(srvreg->authcount)
    {
        srvreg->autharray = (SLPAuthBlock*)malloc(sizeof(SLPAuthBlock) * srvreg->authcount);
        if(srvreg->autharray == 0)
        {
            return SLP_ERROR_INTERNAL_ERROR;
        }
        memset(srvreg->autharray,0,sizeof(SLPAuthBlock) * srvreg->authcount);

        for(i=0;i<srvreg->authcount;i++)
        {
            result = ParseAuthBlock(buffer,&(srvreg->autharray[i]));
            if(result) return result;
        }
    }
    
    return 0;
}


/*--------------------------------------------------------------------------*/
int ParseSrvDeReg(SLPBuffer buffer, SLPSrvDeReg* srvdereg)
/*--------------------------------------------------------------------------*/
{
    int            result;

    /* make sure that min size is met */
    #if(defined(PARANOID) || defined(DEBUG))
    if(buffer->end - buffer->curpos < 4)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    

    /* parse the scope list */
    srvdereg->scopelistlen = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;
    #if(defined(PARANOID) || defined(DEBUG))
    if(srvdereg->scopelistlen > buffer->end - buffer->curpos)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    srvdereg->scopelist = buffer->curpos;
    buffer->curpos = buffer->curpos + srvdereg->scopelistlen;

    /* parse the url entry */
    result = ParseUrlEntry(buffer,&(srvdereg->urlentry));
    if(result)
    {
        return result;
    }
    
    /* parse the tag list */
    srvdereg->taglistlen = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;
    #if(defined(PARANOID) || defined(DEBUG))
    if(srvdereg->taglistlen > buffer->end - buffer->curpos)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    srvdereg->taglist = buffer->curpos;
    buffer->curpos = buffer->curpos + srvdereg->taglistlen;

    return 0;
}


/*--------------------------------------------------------------------------*/
int ParseSrvAck(SLPBuffer buffer, SLPSrvAck* srvack)
/*--------------------------------------------------------------------------*/
{
    srvack->errorcode = AsUINT16(buffer->curpos);
    return 0;
}


/*--------------------------------------------------------------------------*/
int ParseAttrRqst(SLPBuffer buffer, SLPAttrRqst* attrrqst)
/*--------------------------------------------------------------------------*/
{
    /* make sure that min size is met */
    #if(defined(PARANOID) || defined(DEBUG))
    if(buffer->end - buffer->curpos < 10)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    
    /* parse the prlist */
    attrrqst->prlistlen = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;
    #if(defined(PARANOID) || defined(DEBUG))
    if(attrrqst->prlistlen > buffer->end - buffer->curpos)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    attrrqst->prlist = buffer->curpos;
    buffer->curpos = buffer->curpos + attrrqst->prlistlen;
    
    /* parse the url */
    attrrqst->urllen = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;
    #if(defined(PARANOID) || defined(DEBUG))
    if(attrrqst->urllen > buffer->end - buffer->curpos)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    attrrqst->url = buffer->curpos;
    buffer->curpos = buffer->curpos + attrrqst->urllen;    


    /* parse the scope list */
    attrrqst->scopelistlen = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;
    #if(defined(PARANOID) || defined(DEBUG))
    if(attrrqst->scopelistlen > buffer->end - buffer->curpos)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    attrrqst->scopelist = buffer->curpos;
    buffer->curpos = buffer->curpos + attrrqst->scopelistlen;    


    /* parse the taglist string */
    attrrqst->taglistlen = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;
    #if(defined(PARANOID) || defined(DEBUG))
    if(attrrqst->taglistlen > buffer->end - buffer->curpos)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    attrrqst->taglist = buffer->curpos;
    buffer->curpos = buffer->curpos + attrrqst->taglistlen;


    /* parse the slpspi string */
    attrrqst->spistrlen = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;
    #if(defined(PARANOID) || defined(DEBUG))
    if(attrrqst->spistrlen > buffer->end - buffer->curpos)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    attrrqst->spistr = buffer->curpos;
    buffer->curpos = buffer->curpos + attrrqst->spistrlen;

    return 0;
}


/*--------------------------------------------------------------------------*/
int ParseAttrRply(SLPBuffer buffer, SLPAttrRply* attrrply)
/*--------------------------------------------------------------------------*/
{
    int             result;
    int             i;
    
    /* make sure that min size is met */
    #if(defined(PARANOID) || defined(DEBUG))
    if(buffer->end - buffer->curpos < 4)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif

    /* parse out the error code */
    attrrply->errorcode = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;

    /* parse out the attrlist */
    attrrply->attrlistlen = AsUINT16(buffer->curpos);
    buffer->curpos = buffer->curpos + 2;
    #if(defined(PARANOID) || defined(DEBUG))
    if(attrrply->attrlistlen > buffer->end - buffer->curpos)
    {
        return SLP_ERROR_PARSE_ERROR;
    }
    #endif
    attrrply->attrlist = buffer->curpos;
    buffer->curpos = buffer->curpos + attrrply->attrlistlen;

    /* parse out auth block count */
    attrrply->authcount = *(buffer->curpos);
    buffer->curpos = buffer->curpos + 1;

    /* parse out the auth block (if any) */
    if(attrrply->authcount)
    {
        attrrply->autharray = (SLPAuthBlock*)malloc(sizeof(SLPAuthBlock) * attrrply->authcount);
        if(attrrply->autharray == 0)
        {
            return SLP_ERROR_INTERNAL_ERROR;
        }
        memset(attrrply->autharray,0,sizeof(SLPAuthBlock) * attrrply->authcount);

        for(i=0;i<attrrply->authcount;i++)
        {
            result = ParseAuthBlock(buffer,&(attrrply->autharray[i]));
            if(result) return result;
        }
    }

    return 0;
}


/*-------------------------------------------------------------------------*/
void SLPMessageFreeInternals(SLPMessage message)
/*-------------------------------------------------------------------------*/
{
    int i;

    switch(message->header.functionid)
    {   
    case SLP_FUNCT_SRVRPLY:
        if(message->body.srvrply.urlarray)
        {
        
            for(i=0;i<message->body.srvrply.urlcount;i++)
            {
                if(message->body.srvrply.urlarray[i].autharray)
                {
                    free(message->body.srvrply.urlarray[i].autharray);
                }
            }

            free(message->body.srvrply.urlarray);
        }
        break;
    
    case SLP_FUNCT_SRVREG:
        if(message->body.srvreg.urlentry.autharray)
        {
            free(message->body.srvreg.urlentry.autharray);
        }                                        
        if(message->body.srvreg.autharray)
        {
            free(message->body.srvreg.autharray);
        }                                        
        break;
    
      
    case SLP_FUNCT_SRVDEREG:
        if(message->body.srvdereg.urlentry.autharray)
        {
            free(message->body.srvdereg.urlentry.autharray);
        }
        break;
          
    case SLP_FUNCT_ATTRRQST:
        break;
        
    case SLP_FUNCT_ATTRRPLY:
        break;
        
    /*
    case SLP_FUNCT_DAADVERT:
        result = ParseDAAdvert(start,end,&xlator->body.daadvert);
        break;
        
    case SLP_FUNCT_SRVTYPERQST:
        result = ParseSrvTypeRqst(start,end,&xlator->body.srvtyperqst);
        break;
        
    case SLP_FUNCT_SRVTYPERPLY:
        result = ParseSrvTypeRply(start,end,&xlator->body.srvtyperply);
        break;
        
    case SLP_FUNCT_SAADVERT:
        result = ParseSAAdvert(start,end,&xlator->body.saadvert);
        break;
        */
    
    case SLP_FUNCT_SRVACK:
    case SLP_FUNCT_SRVRQST:
    default:
        /* don't do anything */
        break;
    }

}

/*=========================================================================*/
SLPMessage SLPMessageAlloc()
/* Allocates memory for a SLP message descriptor                           */
/*                                                                         */
/* Returns   - A newly allocated SLPMessage pointer of NULL on ENOMEM      */
/*=========================================================================*/
{
    SLPMessage result = (SLPMessage)malloc(sizeof(struct _SLPMessage));
    if(result)
    {
        memset(result,0,sizeof(struct _SLPMessage));
    }
    
    return result;
}


/*=========================================================================*/
void SLPMessageFree(SLPMessage message)
/* Frees memory that might have been allocated by the SLPMessage for       */
/* UrlEntryLists or AuthBlockLists.                                        */
/*                                                                         */
/* message  - (IN) the SLPMessage to free                                  */
/*=========================================================================*/
{
    SLPMessageFreeInternals(message);
    free(message);
}


/*=========================================================================*/
int SLPMessageParseBuffer(SLPBuffer buffer, SLPMessage message)
/* Initializes a message descriptor by parsing the specified buffer.       */
/*                                                                         */
/* buffer   - (IN) pointer the SLPBuffer to parse                          */
/*                                                                         */
/* message  - (OUT) set to describe the message from the buffer            */
/*                                                                         */
/* Returns  - Zero on success, SLP_ERROR_PARSE_ERROR, or                   */
/*            SLP_ERROR_INTERNAL_ERROR if out of memory.  SLPMessage is    */
/*            invalid return is not successful.                            */
/*                                                                         */
/* WARNING  - If successful, pointers in the SLPMessage reference memory in*/ 
/*            the parsed SLPBuffer.  If SLPBufferFree() is called then the */
/*            pointers in SLPMessage will be invalidated.                  */
/*=========================================================================*/
{
    int result;

    /* Get ready to parse */
    SLPMessageFreeInternals(message);
    buffer->curpos = buffer->start;

    /* parse the header first */
    result = ParseHeader(buffer,&(message->header));
    if(result == 0)
    {
        /* switch on the function id to parse the body */
        switch(message->header.functionid)
        {
        case SLP_FUNCT_SRVRQST:
            result = ParseSrvRqst(buffer,&(message->body.srvrqst));
            break;

        case SLP_FUNCT_SRVRPLY:
            result = ParseSrvRply(buffer,&(message->body.srvrply));
            break;
    
        case SLP_FUNCT_SRVREG:
            result = ParseSrvReg(buffer,&(message->body.srvreg));
            break;
            
        case SLP_FUNCT_SRVDEREG:
            result = ParseSrvDeReg(buffer,&(message->body.srvdereg));
            break;
    
        case SLP_FUNCT_SRVACK:
            result = ParseSrvAck(buffer,&(message->body.srvack));
            break;
    
        case SLP_FUNCT_ATTRRQST:
            result = ParseAttrRqst(buffer,&(message->body.attrrqst));
            break;
    
        case SLP_FUNCT_ATTRRPLY:
            result = ParseAttrRply(buffer,&(message->body.attrrply));
            break;

/*    
        case SLP_FUNCT_DAADVERT:
            result = ParseDAAdvert(start,end,&xlator->body.daadvert);
            break;
    
        case SLP_FUNCT_SRVTYPERQST:
            result = ParseSrvTypeRqst(start,end,&xlator->body.srvtyperqst);
            break;
    
        case SLP_FUNCT_SRVTYPERPLY:
            result = ParseSrvTypeRply(start,end,&xlator->body.srvtyperply);
            break;
    
        case SLP_FUNCT_SAADVERT:
            result = ParseSAAdvert(start,end,&xlator->body.saadvert);
            break;
*/
        default:
            result = SLP_ERROR_PARSE_ERROR;
        }
    }   

    return result;
}






 
