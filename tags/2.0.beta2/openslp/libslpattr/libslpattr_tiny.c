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

/** Tiny attribute parser.
 *
 * A "tiny" implementation of slp_attr. Instead of providing the full 
 * functionality, we give a minimal interface for use in slpd.
 *
 * @file       libslpattr_tiny.c
 * @author     Matthew Peterson, John Calcote (jcalcote@novell.com)
 * @attention  Please submit patches to http://www.openslp.org
 * @ingroup    CommonCodeAttrs
 */

#include "libslpattr.h"
#include "../common/slp_debug.h"

/*****************************************************************************
 *
 * Implemented functions. 
 * 
 ****************************************************************************/

/* The tiny attribute structure. */
struct xx_TinyAttr
{
   char * attributes; /* A null terminated attribute string. */
   int attr_len; /* The length of the attributes member. */
};

SLPError SLPAttrAlloc(const char * lang, const FILE * template_h,
      const SLPBoolean strict, SLPAttributes * slp_attr_h)
{
   struct xx_TinyAttr ** slp_attr;
   slp_attr = (struct xx_TinyAttr * *) slp_attr_h;

   /* Don't bother sanity checking. */
   /* FIXME Should we check? */

   (*slp_attr) = (struct xx_TinyAttr *) malloc(sizeof(struct xx_TinyAttr));
   if (*slp_attr == NULL)
      return SLP_MEMORY_ALLOC_FAILED;

   (*slp_attr)->attributes = NULL;
   (*slp_attr)->attr_len = 0;

   return SLP_OK;
}


void SLPAttrFree(SLPAttributes attr_h)
{
   struct xx_TinyAttr * slp_attr = (struct xx_TinyAttr *) attr_h;

   /***** Free data. *****/
   if (slp_attr->attributes)
      free(slp_attr->attributes);
   slp_attr->attr_len = 0;

   /***** Free struct. *****/
   free(slp_attr);
   slp_attr = NULL;
}

/* TODO/FIXME Does not freshen, instead replaces. */
SLPError SLPAttrFreshen(SLPAttributes attr_h, const char * new_attrs)
{
   struct xx_TinyAttr * slp_attr = (struct xx_TinyAttr *) attr_h;

   /***** Free old data. *****/
   if (slp_attr->attributes)
      free(slp_attr->attributes);
   slp_attr->attr_len = 0;

   /***** Copy new data. *****/
   slp_attr->attributes = strdup(new_attrs);
   if (slp_attr->attributes == NULL)
      return SLP_MEMORY_ALLOC_FAILED;
   slp_attr->attr_len = strlen(new_attrs);

   /***** Done. *****/
   return SLP_OK;
}


SLPError SLPAttrSerialize(SLPAttributes attr_h,
      const char * tags /* NULL terminated */,
      char ** out_buffer /* Where to write. if *out_buffer == NULL, space is alloc'd */,
      size_t bufferlen, /* Size of buffer. */
      size_t * count, /* Bytes needed/written. */
      SLPBoolean find_delta)
{
   struct xx_TinyAttr * slp_attr = (struct xx_TinyAttr *) attr_h;

   /* Write the amount of space we need. */
   if (count != NULL)
   {
      *count = slp_attr->attr_len + 1; /* For the null. */
   }

   /* Check that we have somewhere to write to. */
   if (bufferlen < slp_attr->attr_len + 1)
   {
      /* +1 for null. */
      return SLP_BUFFER_OVERFLOW;
   }
   SLP_ASSERT(out_buffer != NULL && *out_buffer != NULL); /* Verify we have somewhere to write. */


   /* Check for empty string. */
   if (slp_attr->attr_len == 0)
   {
      **out_buffer = 0; /* Empty string. */
      return SLP_OK;
   }

   /* Copy. */
   strcpy(*out_buffer, slp_attr->attributes);

   return SLP_OK;
}



/*****************************************************************************
 *
 * Unimplemented functions.
 * 
 ****************************************************************************/

SLPError SLPAttrAllocStr(const char * lang, const FILE * template_h,
      const SLPBoolean strict, SLPAttributes * slp_attr, const char * str)
{
   return SLP_NOT_IMPLEMENTED;
}


/* Attribute manipulation. */
SLPError SLPAttrSet_bool(SLPAttributes attr_h, const char * attribute_tag,
      SLPBoolean val)
{
   return SLP_NOT_IMPLEMENTED;
}


SLPError SLPAttrSet_str(SLPAttributes attr_h, const char * tag,
      const char * val, SLPInsertionPolicy pol)
{
   return SLP_NOT_IMPLEMENTED;
}


SLPError SLPAttrSet_keyw(SLPAttributes attr_h, const char * attribute_tag)
{
   return SLP_NOT_IMPLEMENTED;
}


SLPError SLPAttrSet_int(SLPAttributes attr_h, const char * tag, int val,
      SLPInsertionPolicy policy)
{
   return SLP_NOT_IMPLEMENTED;
}


SLPError SLPAttrSet_opaque(SLPAttributes attr_h, const char * tag,
      const char * val, const unsigned int len, SLPInsertionPolicy policy)
{
   return SLP_NOT_IMPLEMENTED;
}


SLPError SLPAttrSet_guess(SLPAttributes attr_h, const char * tag,
      const char * val, SLPInsertionPolicy policy)
{
   return SLP_NOT_IMPLEMENTED;
}



/* Attribute Querying. */
SLPError SLPAttrGet_bool(SLPAttributes attr_h, const char * tag,
      SLPBoolean * val)
{
   return SLP_NOT_IMPLEMENTED;
}


SLPError SLPAttrGet_keyw(SLPAttributes attr_h, const char * tag)
{
   return SLP_NOT_IMPLEMENTED;
}


SLPError SLPAttrGet_int(SLPAttributes attr_h, const char * tag, int * val[],
      size_t * size)
{
   return SLP_NOT_IMPLEMENTED;
}


SLPError SLPAttrGet_str(SLPAttributes attr_h, const char * tag, char *** val,
      size_t * size)
{
   return SLP_NOT_IMPLEMENTED;
}



SLPError SLPAttrGet_opaque(SLPAttributes attr_h, const char * tag,
      SLPOpaque *** val, size_t * size)
{
   return SLP_NOT_IMPLEMENTED;
}



/* Misc. */
SLPError SLPAttrGetType(SLPAttributes attr_h, const char * tag, SLPType * type)
{
   return SLP_NOT_IMPLEMENTED;
}

/* Functions. */
SLPError SLPRegAttr(SLPHandle slp_h, const char * srvurl,
      unsigned short lifetime, const char * srvtype, SLPAttributes attr_h,
      SLPBoolean fresh, SLPRegReport callback, void * cookie)
{
   return SLP_NOT_IMPLEMENTED;
}


SLPError SLPFindAttrObj(SLPHandle hslp, const char * srvurlorsrvtype,
      const char * scopelist, const char * attrids,
      SLPAttrObjCallback * callback, void * cookie)
{
   return SLP_NOT_IMPLEMENTED;
}




/*****************************************************************************
 *
 * Functions for the iterator struct
 * 
 ****************************************************************************/

SLPError SLPAttrIteratorAlloc(SLPAttributes attr, SLPAttrIterator * iter)
{
   return SLP_NOT_IMPLEMENTED;
}

void SLPAttrIteratorFree(SLPAttrIterator iter)
{
   return ;
}


SLPBoolean SLPAttrIterNext(SLPAttrIterator iter_h, char const * *tag,
      SLPType * type)
{
   return SLP_NOT_IMPLEMENTED;
}

SLPBoolean SLPAttrIterValueNext(SLPAttrIterator iter_h, SLPValue *value)
{
   return SLP_NOT_IMPLEMENTED;
}

/*=========================================================================*/
