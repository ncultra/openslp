/***************************************************************************/
/*                                                                         */
/* Project:     OpenSLP - OpenSource implementation of Service Location    */
/*              Protocol                                                   */
/*                                                                         */
/* File:        slp_buffer.c                                               */
/*                                                                         */
/* Abstract:    Header file that defines structures and constants that are */
/*              are used to control memory allocation in OpenSLP           */
/*                                                                         */
/* Author(s):   Matthew Peterson                                           */
/*                                                                         */
/***************************************************************************/

#include <slp_buffer.h> 

/*=========================================================================*/
void* memdup(const void* src, int srclen)
/* Generic memdup analogous to strdup()                                    */
/*=========================================================================*/
{
    char* result;
    result = (char*)malloc(srclen);
    if(result)
    {
        memcpy(result,src,srclen);
    }

    return result;
}

/*=========================================================================*/
SLPBuffer SLPBufferAlloc(size_t size)                                         
/* Must be called to initially allocate a SLPBuffer                        */
/*                                                                         */
/* size     - (IN) number of bytes to reallocate                           */ 
/*                                                                         */
/* returns  - newly allocated SLPBuffer or NULL on ENOMEM                  */
/*=========================================================================*/
{
   SLPBuffer result;

   result = (SLPBuffer)malloc(sizeof(struct _SLPBuffer) + size);
   if(result)
   {
       result->allocated = size;
       result->start = (char*)(result + 1);
       result->curpos = result->start;
       result->end = result->start + size; 
       
       #if(defined DEBUG)
       memset(result->start,0xff,size);
       #endif
   }
   
   return result;
}

/*=========================================================================*/
SLPBuffer SLPBufferRealloc(SLPBuffer buf, size_t size)
/* Must be called to initially allocate a SLPBuffer                        */
/*                                                                         */
/* size     - (IN) number of bytes to allocate                             */
/*                                                                         */
/* returns  - a newly allocated SLPBuffer or NULL on ENOMEM                */
/*=========================================================================*/
{
   SLPBuffer result;
   if(buf)
   {
       if(buf->allocated >= size)
       {
           result = buf;
       }
       else
       {
           result = (SLPBuffer)realloc(buf,sizeof(struct _SLPBuffer) + size);
           result->allocated = size;
       }
       
       if(result)
       {
    	   result->start = (char*)(result + 1);
    	   result->curpos = result->start;
    	   result->end = result->start + size;
    	   
    	   #if(defined DEBUG)
    	   memset(result->start,0xff,size);
    	   #endif
       }
   }
   else
   {
       result = SLPBufferAlloc(size);
   }

   return result;
}


/*=========================================================================*/
SLPBuffer SLPBufferDup(SLPBuffer buf)
/* Returns a duplicate buffer.  Duplicate buffer must be freed by a call   */
/* to SLPBufferFree()                                                      */
/*                                                                         */
/* size     - (IN) number of bytes to allocate                             */
/*                                                                         */
/* returns  - a newly allocated SLPBuffer or NULL on ENOMEM                */
/*=========================================================================*/
{
    SLPBuffer dup;

    dup = SLPBufferAlloc(buf->end - buf->start);
    if(dup)
    {
	    memcpy(dup->start,buf->start,buf->end - buf->start);       
    }
    
    return dup;
}


/*=========================================================================*/
void SLPBufferFree(SLPBuffer buf)
/* Free a previously allocated SLPBuffer                                   */
/*                                                                         */
/* msg      - (IN) the SLPBuffer to free                                   */
/*                                                                         */
/* returns  - none                                                         */
/*=========================================================================*/
{
    if(buf)
    {
        free(buf);
    }
}
