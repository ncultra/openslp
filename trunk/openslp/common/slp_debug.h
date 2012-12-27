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

/** Defines basic SLP types.
 *
 * C99 standard sized types are cool, but not very standard yet. Some
 * platforms/compilers support stdint.h, others support inttypes.h to
 * do the same thing (even though inttypes.h is clearly supposed to do
 * something different). Some platforms (like Windows) doesn't even 
 * support the C99 standard sized types yet. In this case, we define
 * them here on an as-needed basis.
 *
 * @file       common/slp_debug.h
 * @author     John Calcote (jcalcote@novell.com)
 * @attention  Please submit patches to http://www.openslp.org
 * @ingroup    CommonCodeDebugAssert
 */

#ifndef SLP_DEBUG_H_INCLUDED
#define SLP_DEBUG_H_INCLUDED

/*!@defgroup CommonCodeDebug Debug
 * @ingroup CommonCode
 */

/*!@defgroup CommonCodeDebugAssert Assert
 * @ingroup CommonCodeDebug
 * @{
 */

#include "slp_types.h"

#ifdef DEBUG
int SLPAssertionFailed(const char * assertion, const char * file, int line);
# define SLP_ASSERT(x) (void)(x? 0: SLPAssertionFailed(#x, __FILE__, __LINE__))
#else
# define SLP_ASSERT(x) (void)0
#endif

/*! @} */

#endif   /* SLP_DEBUG_H_INCLUDED */

/*=========================================================================*/
