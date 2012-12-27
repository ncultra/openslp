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

/** Service provider index code.
 *
 * @file       slpd_spi.c
 * @author     Matthew Peterson, John Calcote (jcalcote@novell.com)
 * @attention  Please submit patches to http://www.openslp.org
 * @ingroup    SlpdCode
 */
#ifdef _WIN32
// nonstandard extension used : translation unit is empty
# pragma warning (disable : 4206) 
#else
# if HAVE_CONFIG_H
#  include "config.h"
# endif
#endif

#ifdef ENABLE_SLPv2_SECURITY

#include "slpd_spi.h"

SLPSpiHandle G_SlpdSpiHandle = 0;

/** Initialize the Security Parameter Index system from a file.
 *
 * @param[in] spifile - The SPI file to read from.
 *
 * @return Zero on success or a non-zero value on failure.
 */
int SLPDSpiInit(const char * spifile)
{
   if (G_SlpdSpiHandle)
   {
      SLPSpiClose(G_SlpdSpiHandle);
      G_SlpdSpiHandle = 0;
   }
   G_SlpdSpiHandle = SLPSpiOpen(spifile,1);
   return G_SlpdSpiHandle == 0;
}

#ifdef DEBUG
/** Deinitialize the Security Parameter Index system.
 */
void SLPDSpiDeinit(void)
{
   SLPSpiClose(G_SlpdSpiHandle);
}
#endif

#endif   /* ENABLE_SLPv2_SECURITY */

/*=========================================================================*/
