/***************************************************************************/
/*                                                                         */
/* Project:     OpenSLP - OpenSource implementation of Service Location    */
/*              Protocol Version 2                                         */
/*                                                                         */
/* File:        slpd_database.c                                            */
/*                                                                         */
/* Abstract:    Implements database abstraction.  Currently a simple double*/
/*              linked list is used for the underlying storage.            */
/*                                                                         */
/* WARNING:     NOT thread safe!                                           */
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
char* TrimWhitespace(char* str)
/*-------------------------------------------------------------------------*/
{
    int i;

    while(*str && *str <= 0x20)
    {
        str++;
    }

    for(i=strlen(str)-1;i>=0;i--)
    {
        if(str[i] <= 0x20)
        {
            str[i]=0;
        }
    }
    
    return str;
}

/*-------------------------------------------------------------------------*/
char* RegFileReadLine(FILE* fd, char* line, int linesize)
/*-------------------------------------------------------------------------*/
{
    while(1)
    {
        if(fgets(line,linesize,fd) == 0)
        {
            return 0;
        }
        if(*line == 0x0d || *line == 0x0a)
        {
            break;    
        }
        while(*line && *line <= 0x20) line++;

        if(*line != 0 && *line != '#' && *line != ';')
        {
            break;
        }
    }

    return line;
}

/*=========================================================================*/
SLPDDatabaseEntry* SLPDRegFileReadEntry(FILE* fd, SLPDDatabaseEntry** entry)
/* A really big and nasty function that reads an entry SLPDDatabase entry  */
/* from a file. Don't look at this too hard or you'll be sick              */
/*                                                                         */
/* fd       (IN) file to read from                                         */
/*                                                                         */
/* entry    (OUT) Address of a pointer that will be set to the location of */
/*                a dynamically allocated SLPDDatabase entry.  The entry   */
/*                must be freed                                            */
/*                                                                         */
/* Returns  *entry or null on error.                                       */
/*=========================================================================*/
{
    char*   slider1;
    char*   slider2;
    char    line[4096];

    /* give the out param a value */
    *entry = 0;

    /*----------------------------------------------------------*/
    /* read the next non-white non-comment line from the stream */
    /*----------------------------------------------------------*/
    do
    {
        slider1 = RegFileReadLine(fd,line,4096);
        if(slider1 == 0)
        {
            /* read through the whole file and found no entries */
            return 0;
        }
    }while(*slider1 == 0x0d ||  *slider1 == 0x0a);

    /*---------------------------*/
    /* Allocate a database entry */
    /*---------------------------*/
    *entry =  (SLPDDatabaseEntry*)malloc(sizeof(SLPDDatabaseEntry));
    if(entry == 0)
    {
        SLPError("Out of memory allocating DatabaseEntry\n");
        return 0;
    }
    memset(*entry,0,sizeof(SLPDDatabaseEntry));

    /*---------------------*/
    /* Parse the url-props */
    /*---------------------*/
    slider2 = strchr(slider1,',');
    if(slider2) 
    {
        /* srvurl */
        *slider2 = 0; /* squash comma to null terminate srvurl */
        (*entry)->url = strdup(TrimWhitespace(slider1));
        if((*entry)->url == 0)
        {
            SLPError("Out of memory reading srvurl from regfile line ->%s",line);
            goto ERROR;
        }
        (*entry)->urllen = strlen((*entry)->url);
        
        /* derive srvtype from srvurl if srvurl is "service:" scheme URL */
        if(strncasecmp(slider1,"service:",8)==0) 
        {
            (*entry)->srvtype = strstr(slider1,"://");
            if((*entry)->srvtype == 0)
            {
                SLPError("Looks like a bad url on regfile line ->%s",line);
                goto ERROR;   
            }
            *(*entry)->srvtype = 0;
            (*entry)->srvtype=strdup(TrimWhitespace(slider1 + 8));
            (*entry)->srvtypelen = strlen((*entry)->srvtype);
        }
        slider1 = slider2 + 1;

        /*lang*/
        slider2 = strchr(slider1,',');
        if(slider2)
        {
            *slider2 = 0; /* squash comma to null terminate lang */
            (*entry)->langtag = strdup(TrimWhitespace(slider1)); 
            if((*entry)->langtag == 0)
            {
                SLPError("Out of memory reading langtag from regfile line ->%s",line);
                goto ERROR;
            }            (*entry)->langtaglen = strlen((*entry)->langtag);     
            slider1 = slider2 + 1;                                  
        }
        else
        {
            SLPError("Expected language tag near regfile line ->%s\n",line);
            goto ERROR;
        }
             
        /* ltime */
        slider2 = strchr(slider1,',');
        if(slider2)                      
        {
            *slider2 = 0; /* squash comma to null terminate ltime */
            (*entry)->lifetime = atoi(slider1);
            slider1 = slider2 + 1;
        }                                  
        else
        {
            (*entry)->lifetime = atoi(slider1);
            slider1 = slider2;
        }
        if((*entry)->lifetime < 1 || (*entry)->lifetime > 0xffff)
        {
            SLPError("Invalid lifetime near regfile line ->%s\n",line);
            goto ERROR;
        }
        
        /* get the srvtype if one was not derived by the srvurl*/
        if((*entry)->srvtype == 0)
        {
            (*entry)->srvtype = strdup(TrimWhitespace(slider1));
            if((*entry)->srvtype == 0)
            {
                SLPError("Out of memory reading srvtype from regfile line ->%s",line);
                goto ERROR;
            }
            (*entry)->srvtypelen = strlen((*entry)->srvtype);
            if((*entry)->srvtypelen == 0)
            {
                SLPError("Expected to derive service-type near regfile line -> %s\n",line);
                goto ERROR;
            }
        }   

    }
    else
    {
        SLPError("Expected to find srv-url near regfile line -> %s\n",line);
        goto ERROR;
    }
    
    /*-------------------------------------------------*/
    /* Read all the attributes including the scopelist */
    /*-------------------------------------------------*/
    *line=0;
    while(1)
    {
        if(RegFileReadLine(fd,line,4096) == 0)
        {
            break;
        }         
        if(*line == 0x0d || *line == 0x0a)
        {
            break;
        }

        /* Check to see if it is the scopes line */
        if(strncasecmp(line,"scopes",6) == 0)
        {
            /* found scopes line */
            slider1 = line;
            slider2 = strchr(slider1,'=');
            if(slider2)
            {
                slider2++;
                if(*slider2)
                {
                    /* just in case some idiot puts multiple scopes lines */
                    if((*entry)->scopelist)
                    {
                        SLPError("scopes already defined previous to regfile line ->%s",line);
                        goto ERROR;
                    }

                    (*entry)->scopelist=strdup(TrimWhitespace(slider2));
                    if((*entry)->scopelist == 0)
                    {
                        SLPError("Out of memory adding scopes from regfile line ->%s",line);
                        goto ERROR;
                    }
                    (*entry)->scopelistlen = strlen((*entry)->scopelist);
                }
            }
        }
        else
        {
            /* line contains an attribute (slow but it works)*/
            /* TODO Fix this so it's faster! */
            TrimWhitespace(line); 
            (*entry)->attrlistlen += strlen(line) + 2;
            (*entry)->attrlist = realloc((*entry)->attrlist,(*entry)->attrlistlen + 1);
            strcat((*entry)->attrlist,"(");
            strcat((*entry)->attrlist,line);
            strcat((*entry)->attrlist,")");
        }
    }

    /* Set the scope to default if not is set */
    if((*entry)->scopelist == 0)
    {
        (*entry)->scopelist=strdup("DEFAULT");
        if((*entry)->scopelist == 0)
        {
            SLPError("Out of memory adding DEFAULT scope\n");
            goto ERROR;
        }
        (*entry)->scopelistlen = 7;
    }

    return *entry;

    ERROR:
    if(*entry)
    {
        if((*entry)->srvtype) free((*entry)->srvtype);
        if((*entry)->url) free((*entry)->url);
        if((*entry)->langtag) free((*entry)->langtag);
        if((*entry)->scopelist) free((*entry)->scopelist);
        if((*entry)->attrlist) free((*entry)->attrlist);
        free(*entry);
        *entry = 0;
    }
    
    return 0;
}
