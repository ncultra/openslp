/*-------------------------------------------------------------------------
 * Copyright (C) 2000 Caldera Systems, Inc
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 *    Neither the name of Caldera Systems nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * `AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE CALDERA
 * SYSTEMS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *-------------------------------------------------------------------------*/

#ifndef _SLPTOOL_H
#define _SLPTOOL_H

#include <slp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32
#include <malloc.h>
#define strcasecmp(str1,str2) stricmp(str1,str2)
#endif

/*=========================================================================*/
typedef enum _SLPToolCommand
/*=========================================================================*/
{
    FINDSRVS = 1,
    FINDATTRS,
    FINDSRVTYPES,
    FINDSCOPES,
    GETPROPERTY,
    REGISTER,
    DEREGISTER
}SLPToolCommand;



/*=========================================================================*/
typedef struct _SLPToolCommandLine
/*=========================================================================*/
{
    SLPToolCommand  cmd;
    const char*     lang;
    const char*     scopes;
    const char*     cmdparam1;
    const char*     cmdparam2;
}SLPToolCommandLine;


/*=========================================================================*/
void FindSrvs(SLPToolCommandLine* cmdline);
/*=========================================================================*/


/*=========================================================================*/
void FindAttrs(SLPToolCommandLine* cmdline);
/*=========================================================================*/


/*=========================================================================*/            
void FindSrvTypes(SLPToolCommandLine* cmdline);
/*=========================================================================*/
            

/*=========================================================================*/
void GetProperty(SLPToolCommandLine* cmdline);
/*=========================================================================*/

/*=========================================================================*/
void Register(SLPToolCommandLine* cmdline);
/*=========================================================================*/


/*=========================================================================*/
void Deregister(SLPToolCommandLine* cmdline);
/*=========================================================================*/

#endif
