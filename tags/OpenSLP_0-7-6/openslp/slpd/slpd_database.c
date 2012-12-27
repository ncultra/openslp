/***************************************************************************/
/*                                                                         */
/* Project:     OpenSLP - OpenSource implementation of Service Location    */
/*              Protocol Version 2                                         */
/*                                                                         */
/* File:        slpd_database.c                                            */
/*                                                                         */
/* Abstract:    Implements database abstraction.  Currently a simple       */
/*              double linked list is used for the underlying storage.     */
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
#include <assert.h>

/*=========================================================================*/
SLPList G_DatabaseList = {0,0,0};
/*=========================================================================*/


/*-------------------------------------------------------------------------*/
void FreeEntry(SLPDDatabaseEntry* entry)
/*-------------------------------------------------------------------------*/
{
    if(entry)
    {
        if(entry->scopelist) free(entry->scopelist);
        if(entry->srvtype) free(entry->srvtype);
        if(entry->url) free(entry->url);
        SLPAttrFree(entry->attr);
        free(entry);
    }
}


/*-------------------------------------------------------------------------*/
void FreeAllEntries(SLPList* list)
/*-------------------------------------------------------------------------*/
{
    while(list->count)
    {
        FreeEntry((SLPDDatabaseEntry*)SLPListUnlink(list,list->head));
    }
}


/*=========================================================================*/
void SLPDDatabaseAge(int seconds)
/* Agea the database entries and clears new and deleted entry lists        */
/*                                                                         */
/* seconds  (IN) the number of seconds to age each entry by                */
/*                                                                         */
/* Returns  - None                                                         */
/*=========================================================================*/
{
    SLPDDatabaseEntry* entry;
    SLPDDatabaseEntry* del   = 0;

    /* Age the database */
    entry = (SLPDDatabaseEntry*)G_DatabaseList.head;
    while(entry)
    {
        /* don't age services with lifetime > SLP_LIFETIME_MAXIMUM */
        if(entry->lifetime < SLP_LIFETIME_MAXIMUM)
    	{
            entry->lifetime = entry->lifetime - seconds;
            if(entry->lifetime <= 0)
            {
                del = entry;
            }
    	}
        
        entry = (SLPDDatabaseEntry*)entry->listitem.next;

        if(del)
        {
            FreeEntry((SLPDDatabaseEntry*)SLPListUnlink(&G_DatabaseList,(SLPListItem*)del));
            del = 0;
        }                                                               
    }
}


/*=========================================================================*/
int SLPDDatabaseReg(SLPSrvReg* srvreg,
                    int fresh,
                    pid_t pid,
                    uid_t uid)
/* Add a service registration to the database                              */
/*                                                                         */
/* srvreg   -   (IN) pointer to the SLPSrvReg to be added to the database  */
/*                                                                         */
/* fresh    -   (IN) pass in nonzero if the registration is fresh.         */
/*                                                                         */
/* pid      -   (IN) process id of the process that registered the service */
/*                                                                         */
/* uid      -   (IN) user id of the user that registered the service       */
/*                                                                         */
/* Returns  -   Zero on success.  non-zero on error                        */
/*                                                                         */
/* NOTE:        All registrations are treated as fresh regardless of the   */
/*              setting of the fresh parameter                             */
/*=========================================================================*/
{
    int                result = 0;
    SLPDDatabaseEntry* entry  = (SLPDDatabaseEntry*)G_DatabaseList.head;

    /* Check to see if there is already an identical entry */
    while(entry)
    {
        if(SLPCompareString(entry->urllen,
                            entry->url,
                            srvreg->urlentry.urllen,
                            srvreg->urlentry.url) == 0)
        {
            if(SLPIntersectStringList(entry->scopelistlen,
                                      entry->scopelist,
                                      srvreg->scopelistlen,
                                      srvreg->scopelist) > 0)
            {
                SLPListUnlink(&G_DatabaseList,(SLPListItem*)entry);
                break;
            } 
        }             
        
        entry = (SLPDDatabaseEntry*) entry->listitem.next;
    }
    

    /* if no identical entry are found, create a new one */
    if(entry == 0)
    {
        entry = SLPDDatabaseEntryAlloc();
        if(entry == 0)
        {
            return -1;
        }
    }
    
    /* copy info from the message from the wire to the database entry */
    entry->pid          = pid;
    entry->uid          = uid;
    entry->scopelistlen = srvreg->scopelistlen;
    entry->scopelist    = (char*)memdup(srvreg->scopelist,srvreg->scopelistlen);
    entry->lifetime     = srvreg->urlentry.lifetime;
    entry->urllen       = srvreg->urlentry.urllen;
    entry->url          = (char*)memdup(srvreg->urlentry.url, srvreg->urlentry.urllen);
    entry->srvtypelen   = srvreg->srvtypelen;
    entry->srvtype      = (char*)memdup(srvreg->srvtype,srvreg->srvtypelen);
    if (srvreg->attrlistlen)
    {
		char *str;
		SLPError err;
		
		/* FIXME The following is a hack to give SLPAttrAllocStr() a 
		 * null-terminated string. */
		str = (char *)malloc(srvreg->attrlistlen+1);
		memcpy(str, srvreg->attrlist, srvreg->attrlistlen);
		str[srvreg->attrlistlen] = '\0';
		/* end of hack. */
		
		err = SLPAttrFreshen(entry->attr, str);
		if (err != SLP_OK) {
			FreeEntry(entry);
			return -1;
		}
	}
    
	/* check for malloc() failures */
    if(entry->scopelist == 0 ||
       entry->url == 0 ||
       entry->srvtype == 0)
    {
        FreeEntry(entry);
        return -1;
    }
    
    /* link the new (or modified) entry into the list */
    SLPListLinkHead(&G_DatabaseList,(SLPListItem*)entry);

    /* traceReg if necessary */
    if(G_SlpdProperty.traceReg)
    {
        SLPDLogTraceReg("SrvReg", entry);
    }

    return result;
}


/*=========================================================================*/
int SLPDDatabaseDeReg(SLPSrvDeReg* srvdereg)
/* Remove a service registration from the database                         */
/*                                                                         */
/* regfile  -   (IN) filename of the registration file to read into the    */
/*              database. Pass in NULL for no file.                        */
/*                                                                         */
/* Returns  -   Zero on success.  Non-zero if syntax error in registration */
/*              file.                                                      */
/*=========================================================================*/

{
    SLPDDatabaseEntry* del = 0;
    SLPDDatabaseEntry* entry = (SLPDDatabaseEntry*)G_DatabaseList.head;


    while(entry)
    {
        if(SLPCompareString(entry->urllen,
                            entry->url,
                            srvdereg->urlentry.urllen,
                            srvdereg->urlentry.url) == 0)
        {
            if(SLPIntersectStringList(entry->scopelistlen,
                                      entry->scopelist,
                                      srvdereg->scopelistlen,
                                      srvdereg->scopelist) > 0)
            {
                if(G_SlpdProperty.traceReg)
                {
                    SLPDLogTraceReg("SrvDeReg",entry);
                }
                
                del = entry;
                
                break;
            } 
        }             
        
        entry = (SLPDDatabaseEntry*) entry->listitem.next;

        if(del)
        {
            FreeEntry((SLPDDatabaseEntry*)SLPListUnlink(&G_DatabaseList,(SLPListItem*)del));
            del = 0;
        }
    }

    return 0;
}


/*=========================================================================*/
int SLPDDatabaseFindSrv(SLPSrvRqst* srvrqst, 
                        SLPDDatabaseSrvUrl* result,
                        int count)
/* Find services in the database                                           */
/*                                                                         */
/* srvrqst  (IN) the request to find.                                      */
/*                                                                         */
/* result   (OUT) pointer to an array of result structures that receives   */
/*                results                                                  */
/*                                                                         */
/* count    (IN)  number of elements in the result array                   */
/*                                                                         */
/* Returns  - The number of services found or <0 on error.  If the number  */
/*            of services found is exactly equal to the number of elements */
/*            in the array, the call may be repeated with a larger array.  */
/*=========================================================================*/
{
    SLPDDatabaseEntry*  entry;
    int                 found;
	
	SLPDPredicate pred; /* The predicate object. */
	SLPError err; /* Error code for checking return vals. */

	/***** Create and verify predicate. *****/
	err = SLPDPredicateAlloc(srvrqst->predicate, srvrqst->predicatelen, &pred);
	
	assert(err == SLP_OK || err == SLP_PARSE_ERROR || err == SLP_MEMORY_ALLOC_FAILED); /* Make sure a legal value was returned. */
	
	if (err == SLP_INTERNAL_SYSTEM_ERROR || err == SLP_PARSE_ERROR) {
		return -1;
	}
	assert(err == SLP_OK);


	/***** Test services. *****/
    found = 0;
    entry = (SLPDDatabaseEntry*)G_DatabaseList.head;
    while(entry)
    {
        if(SLPCompareSrvType(srvrqst->srvtypelen,
                             srvrqst->srvtype,
                             entry->srvtypelen,
                             entry->srvtype) == 0)
        {
			if(SLPTestPredicate(pred, entry->attr) == 0)
            {
                if(SLPIntersectStringList(srvrqst->scopelistlen,
                                          srvrqst->scopelist,
                                          entry->scopelistlen,
                                          entry->scopelist))
                {
                    result[found].lifetime = entry->lifetime;
                    result[found].urllen = entry->urllen;
                    result[found].url = entry->url;
                    found ++;
                    if(found >= count)
                    {
                        break;
                    }
                }
            } 
        }

        entry = (SLPDDatabaseEntry*)entry->listitem.next;
    }

	SLPDPredicateFree(pred);
    return found;
}


/*=========================================================================*/
int SLPDDatabaseFindType(SLPSrvTypeRqst* srvtyperqst, 
                         SLPDDatabaseSrvType* result,
                         int count)
/* Find service types                                                      */
/*                                                                         */
/* srvtyperqst  (IN) the request to find.                                  */
/*                                                                         */
/* result   (OUT) pointer to an array of result structures that receives   */
/*                results                                                  */
/*                                                                         */
/* count    (IN)  number of elements in the result array                   */
/*                                                                         */
/* Returns  - The number of srvtypes found or <0 on error.  If the number  */
/*            of srvtypes found is exactly equal to the number of elements */
/*            in the array, the call may be repeated with a larger array.  */
/*=========================================================================*/
{
    SLPDDatabaseEntry*  entry;
    int                 found;
    int                 i;


    found = 0;
    entry = (SLPDDatabaseEntry*)G_DatabaseList.head;
    while(entry)
    {
	if(SLPCompareNamingAuth(entry->srvtypelen, entry->srvtype,
				srvtyperqst->namingauthlen,
				srvtyperqst->namingauth) == 0)
	{
	    if(SLPIntersectStringList(srvtyperqst->scopelistlen,
				      srvtyperqst->scopelist,
				      entry->scopelistlen,
				      entry->scopelist))
	    {
		for (i = 0; i < found; i++)
		{
		    if (strncasecmp(result[i].type, entry->srvtype,
				    entry->srvtypelen) == 0) 
		    {
			break;
		    }
		}
		if(i == found)
		{
		    result[found].type = entry->srvtype;
		    result[found].typelen = entry->srvtypelen;
		    found++;
		    if(found >= count)
		    {
			break;
		    }
		}
	    }
	}

        entry = (SLPDDatabaseEntry*)entry->listitem.next;
    }

    return found;
}


/*=========================================================================*/
int SLPDDatabaseFindAttr(SLPAttrRqst* attrrqst, 
                         SLPDDatabaseAttr* result,
                         int count)
/* Find attributes                                                         */
/*                                                                         */
/* srvtyperqst  (IN) the request to find.                                  */
/*                                                                         */
/* result   (OUT) pointer to a result structure that receives              */
/*                results                                                  */
/*                                                                         */
/* count    (IN)  number of elements in the result array                   */
/*                                                                         */
/* Returns  -   >0 on success. 0 if the url of the attrrqst could not be   */
/*              cound and <0 on error.                                     */
/*=========================================================================*/
{
    SLPDDatabaseEntry*  entry   = 0;
    int                 found   = 0;

    found = 0;
    entry = (SLPDDatabaseEntry*)G_DatabaseList.head;
    while(entry)
    {
        if(SLPCompareString(attrrqst->urllen,
                            attrrqst->url,
                            entry->urllen,
                            entry->url) == 0 ||
           SLPCompareSrvType(attrrqst->urllen,
                             attrrqst->url,
                             entry->srvtypelen,
                             entry->srvtype) == 0 )
        {
            if(SLPIntersectStringList(attrrqst->scopelistlen,
                                      attrrqst->scopelist,
                                      entry->scopelistlen,
                                      entry->scopelist))
            {
				SLPError err;

                err = SLPAttrSerialize(entry->attr, &result[found].attrlen, &result[found].attr, SLP_FALSE);
				if (err == SLP_OK) 
				{
					/* FIXME TODO Should the entire function fail, or should 
					 * we just ignore this one? */
	                found++;
				}
            }       
        }

        entry = (SLPDDatabaseEntry*)entry->listitem.next;

    }

    return found;
}


/*=========================================================================*/
int SLPDDatabaseInit(const char* regfile)
/* Optionaly initialize the database with registrations from a regfile.    */
/*                                                                         */
/* regfile  (IN)    the regfile to register.                               */
/*                                                                         */
/* Returns  - zero on success or non-zero on error.                        */
/*=========================================================================*/
{
    FILE*               fd;
#ifdef WIN32
    int                 mypid = GetCurrentProcessId();
    int                 myuid = 0; /* TODO: find an equivalent to 
                                      uid on Win32 */
#else
    int                 mypid = getpid();
    int                 myuid = getuid();
#endif
    SLPDDatabaseEntry*  entry;

    /* Remove all entries in the database if any */
    FreeAllEntries(&G_DatabaseList);

    /*--------------------------------------*/
    /* Read static registration file if any */
    /*--------------------------------------*/
    if(regfile)
    {
        fd = fopen(regfile,"rb");
        if(fd)
        {
            
            SLPLog("Reading registration file %s\n",regfile);

            while(SLPDRegFileReadEntry(fd,&entry) != 0)
            {
                entry->pid              = mypid;
                entry->uid              = myuid;

                if(G_SlpdProperty.traceReg)
                {
                    SLPDLogTraceReg("SrvReg (static)",entry);
                }

                SLPListLinkHead(&G_DatabaseList,(SLPListItem*)entry);
            }

            fclose(fd);
        }
    }
    
    return 0;
}


/*=========================================================================*/
SLPDDatabaseEntry *SLPDDatabaseEntryAlloc()
/* Allocates and initializes a database entry.                             */
/*                                                                         */
/* Returns  - zero on success or non-zero on error.                        */
/*=========================================================================*/
{
	SLPDDatabaseEntry *entry;
	SLPError err;

	/* Allocate the entry. */
	entry = (SLPDDatabaseEntry *)calloc(1, sizeof(SLPDDatabaseEntry));
	if (entry == NULL) 
	{
		return NULL;
	}

	/* Initialize the entry. */
	err = SLPAttrAlloc("en", NULL, SLP_FALSE, &entry->attr);
	if (err != SLP_OK) {
		FreeEntry(entry);
	}

	return entry;
}
