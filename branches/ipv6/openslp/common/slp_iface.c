/***************************************************************************/
/*                                                                         */
/* Project:     OpenSLP - OpenSource implementation of Service Location    */
/*              Protocol                                                   */
/*                                                                         */
/* File:        slp_iface.c                                                */
/*                                                                         */
/* Abstract:    Common code to obtain network interface information        */
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

#include "slp_iface.h"
#include "slp_xmalloc.h"
#include "slp_compare.h"
#include "slp_net.h"
#include "slp_property.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef SOLARIS
#include <sys/sockio.h>
#endif

#ifndef _WIN32
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#else
#ifndef UINT32_T_DEFINED
#define UINT32_T_DEFINED
typedef unsigned int uint32_t; 
#endif
#endif

/* the max index for v6 address to test for valid scope ids */
#define MAX_INTERFACE_TEST_INDEX 255

/*=========================================================================*/
int SLPIfaceGetInfo(const char* useifaces,
                    SLPIfaceInfo* ifaceinfo, int family)
/* Description:
 *    Get the network interface addresses for this host.  Exclude the
 *    loopback interface
 *
 * Parameters:
 *     useifaces (IN) Pointer to comma delimited string of interface IPv4
 *                    addresses to get interface information for.  Pass
 *                    NULL or empty string to get all interfaces (except 
 *                    loopback)
 *     ifaceinfo (OUT) Information about requested interfaces.
 *     family    (IN) Hint for family to get info for - can be AF_INET, AF_INET6, 
 *                    or AF_UNSPEC for both
 *     scope     (IN) For IPV6 this specifies which scope to get an address for,
 *                this can be SCOPE_GLO
 *
 * Returns:
 *     zero on success, non-zero (with errno set) on error.
 *=========================================================================*/
  {
	char *interfaceString;
	char *bcastString;
	int sts = 0;
	int useifaceslen;
	struct sockaddr_in v4addr;
    long hostAddr;
	struct sockaddr_in bcastAddr;
	struct sockaddr_in6 v6addr;
	struct sockaddr_in6 indexHack;
	unsigned int fd;
	int i;

    /* first try ipv6 addrs */
    ifaceinfo->iface_count = 0;
    ifaceinfo->bcast_count = 0;

	if (useifaces) {
		useifaceslen = strlen(useifaces);
	}
	else {
		useifaceslen = 0;
	}

	/* attempt to retrieve the interfaces from the configuration file */
    interfaceString = (char *) SLPPropertyGet("net.slp.interfaces");
	if (*interfaceString == '\0') {
		/* they don't have any setting in their conf file, so put in the default info */
		if (SLPNetIsIPV6() && ((family == AF_INET6) || (family == AF_UNSPEC)) ){
			struct sockaddr_storage storageaddr_any;

		    fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
			if(fd != -1) {
				for (i = 0; i < MAX_INTERFACE_TEST_INDEX; i++) {
					SLPNetSetAddr((struct sockaddr_storage *) &indexHack, AF_INET6, 0, (char *) &slp_in6addr_any, sizeof(slp_in6addr_any));
					indexHack.sin6_scope_id = i;
					sts = bind(fd, (struct sockaddr *) &indexHack, sizeof(indexHack));
					if (sts == 0) {
						SLPNetSetAddr(&storageaddr_any, AF_INET6, 0, (char *) &slp_in6addr_any, sizeof(slp_in6addr_any));
						SLPNetCopyAddr(&ifaceinfo->iface_addr[ifaceinfo->iface_count], &storageaddr_any);
						ifaceinfo->iface_count++;
						break;
					}
				}
				#ifdef _WIN32 
				closesocket(fd);
				#else
				close(fd);
				#endif
			}
		}
		if (SLPNetIsIPV4() && ((family == AF_INET) || (family == AF_UNSPEC)) ){
			struct sockaddr_storage storageaddr_any;
			unsigned int addrAny = INADDR_ANY;

		    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if(fd != -1) {
				v4addr.sin_family = AF_INET;
				v4addr.sin_port = 0;
				v4addr.sin_addr.s_addr = addrAny;
				sts = bind(fd, (struct sockaddr *) &v4addr, sizeof(v4addr));
				if (sts == 0) {
					SLPNetSetAddr(&storageaddr_any, AF_INET, 0, (char *) &addrAny, sizeof(addrAny));
					SLPNetCopyAddr(&ifaceinfo->iface_addr[ifaceinfo->iface_count], &storageaddr_any);
					ifaceinfo->iface_count++;
				}
				#ifdef _WIN32 
				closesocket(fd);
				#else
				close(fd);
				#endif
			}
		}
	}
	else {
		char *slider1, *slider2, *temp, *tempend;
		/* attemp to use the settings from the file */
	    slider1 = slider2 = temp = xstrdup(SLPPropertyGet("net.slp.interfaces"));
		if(temp) {
	        tempend = temp + strlen(temp);
			while(slider1 != tempend) {
	            while(*slider2 && *slider2 != ',') slider2++;
		        *slider2 = 0;
				if (*slider1 == '\0') {
					slider1++;
				}
				/* should have slider1 pointing to a NULL terminated string for the ip address */
				if (SLPContainsStringList(useifaceslen,
											 useifaces,
											 strlen(slider1),
											 slider1)) {
					/* check if an ipv4 address was given */
					if (inet_pton(AF_INET, slider1, &v4addr.sin_addr) == 1) {
						if (SLPNetIsIPV4() && ((family == AF_INET) || (family == AF_UNSPEC)) ) {
						    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
							if(fd != -1) {
								v4addr.sin_family = AF_INET;
								v4addr.sin_port = 0;
								sts = bind(fd, (struct sockaddr *) &v4addr, sizeof(v4addr));
								if (sts == 0) {
                                    hostAddr = ntohl(v4addr.sin_addr.s_addr);
									SLPNetSetAddr(&ifaceinfo->iface_addr[ifaceinfo->iface_count], AF_INET, 0, (char *) &hostAddr, sizeof(v4addr.sin_addr));
									ifaceinfo->iface_count++;
								}
								#ifdef _WIN32 
								closesocket(fd);
								#else
								close(fd);
								#endif
							}
						}
					}
					else if (inet_pton(AF_INET6, slider1, &v6addr.sin6_addr) == 1) {
						if (SLPNetIsIPV6() && ((family == AF_INET6) || (family == AF_UNSPEC)) ) {
							/* try and bind to verify the address is okay */
						    fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
							if(fd != -1) {
								for (i = 0; i < MAX_INTERFACE_TEST_INDEX; i++) {
									SLPNetSetAddr((struct sockaddr_storage *) &indexHack, AF_INET6, 0, (char *) &v6addr.sin6_addr, sizeof(v6addr.sin6_addr));
									indexHack.sin6_scope_id = i;
									sts = bind(fd, (struct sockaddr *) &indexHack, sizeof(indexHack));
									if (sts == 0) {
										memcpy(&ifaceinfo->iface_addr[ifaceinfo->iface_count], &indexHack, sizeof(indexHack));
										ifaceinfo->iface_count++;
										break;
									}
								}
								#ifdef _WIN32 
								closesocket(fd);
								#else
								close(fd);
								#endif
							}
						}
					}
					else {
						/* not v4, not v6 */
						errno = EINVAL;
						sts = 1;
					}
				}
	            slider1 = slider2;
		        slider2++;
			}
        }
        xfree(temp);
    }

	/* now stuff in a broadcast address */
	if (SLPNetIsIPV4() && ((family == AF_INET) || (family == AF_UNSPEC)) ) { 
	    bcastString = (char *) SLPPropertyGet("net.slp.broadcastAddr");
		if (*bcastString == '\0') {
			struct sockaddr_storage storageaddr_bcast;
			unsigned int broadAddr = INADDR_BROADCAST;

			SLPNetSetAddr(&storageaddr_bcast, AF_INET, 0, (char *) &broadAddr, sizeof(broadAddr));
	        SLPNetCopyAddr(&ifaceinfo->bcast_addr[ifaceinfo->bcast_count], &storageaddr_bcast);
			ifaceinfo->bcast_count++;
		}
		else {
			struct sockaddr_storage storageaddr_bcast;

			if (inet_pton(AF_INET, bcastString, &bcastAddr.sin_addr) == 1) {
				SLPNetSetAddr(&storageaddr_bcast, AF_INET, 0, (char *) &bcastAddr.sin_addr, sizeof(bcastAddr.sin_addr));
		        SLPNetCopyAddr(&ifaceinfo->bcast_addr[ifaceinfo->bcast_count], &storageaddr_bcast);
				ifaceinfo->bcast_count++;
			}
		}
	}

	return(sts);
}
/*=========================================================================*/
int SLPIfaceSockaddrsToString(const struct sockaddr_storage* addrs,
                              int addrcount,
                              char** addrstr)
/* Description:
 *    Get the comma delimited string of addresses from an array of sockaddr_storages
 *
 * Parameters:
 *     addrs (IN) Pointer to array of sockaddr_storages to convert
 *     addrcount (IN) Number of sockaddr_storages in addrs.
 *     addrstr (OUT) pointer to receive malloc() allocated address string.
 *                   Caller must free() addrstr when no longer needed.
 *
 * Returns:
 *     zero on success, non-zero (with errno set) on error.
 *=========================================================================*/
{
    int i;
    
    #ifdef DEBUG
    if(addrs == NULL ||
       addrcount == 0 ||
       addrstr == NULL)
    {
        /* invalid paramaters */
        errno = EINVAL;
        return 1;
    }
    #endif

    /* 40 is the maximum size of a string representation of
     * an IPv6 address (including the comman for the list)
     */
    *addrstr = (char *)xmalloc(addrcount * 40);
    *addrstr[0] = 0;
    
    for (i=0;i<addrcount;i++)
    {
        char buf[1024];

        buf[0]= '\0';

        SLPNetSockAddrStorageToString((struct sockaddr_storage *) (&addrs[i]), buf, sizeof(buf));
        strcat(*addrstr, buf);
        if (i != addrcount-1)
        {
            strcat(*addrstr,",");
        }
    }

    return 0;
}  


/*=========================================================================*/
int SLPIfaceStringToSockaddrs(const char* addrstr,
                              struct sockaddr_storage* addrs,
                              int* addrcount)
/* Description:
 *    Fill an array of struct sockaddr_storages from the comma delimited string of
 *    addresses.
 *
 * Parameters:
 *     addrstr (IN) Address string to convert.
 *     addrcount (OUT) sockaddr_storage array to fill.
 *     addrcount (INOUT) The number of sockaddr_storage stuctures in the addr array
 *                       on successful return will contain the number of
 *                       sockaddr_storages that were filled in the addr array
 *
 * Returns:
 *     zero on success, non-zero (with errno set) on error.
 *=========================================================================*/
{
    int i;
    char* str;
    char* slider1;
    char* slider2;

    #ifdef DEBUG
    if(addrstr == NULL ||
       addrs == NULL ||
       addrcount == 0)
    {
        /* invalid parameters */
        errno = EINVAL;
        return 1;
    }
    #endif

    str = xstrdup(addrstr);
    if(str == NULL)
    {
        /* out of memory */
        return 1;
    }
    
    i=0;
    slider1 = str;
    while(1)
    {
        slider2 = strchr(slider1,',');
        
        /* check for empty string */
        if(slider2 == slider1)
        {
            break;
        }

        /* stomp the comma and null terminate address */
        if(slider2)
        {
            *slider2 = 0;
        }
        // if it has a dot - try v4
        if (strchr(slider1, '.')) {
            struct sockaddr_in *d4 = (struct sockaddr_in *) &addrs[i];
            inet_pton(AF_INET, slider1, &d4->sin_addr);
            d4->sin_family = AF_INET;
        }
        else if (strchr(slider1, ':')) {
            struct sockaddr_in6 *d6 = (struct sockaddr_in6 *) &addrs[i];
            inet_pton(AF_INET6, slider1, &d6->sin6_addr);
            d6->sin6_family = AF_INET6;
        }
        else {
            return(-1);
        }
        i++;
        if(i == *addrcount)
        {
            break;
        }

        /* are we done? */
        if(slider2 == 0)
        {
            break;
        }

        slider1 = slider2 + 1;
    }

    *addrcount = i;

    xfree(str);

    return 0;
}

/*===========================================================================
 * TESTING CODE enabled by removing #define comment and compiling with the 
 * following command line:
 *
 * $ gcc -g -DDEBUG slp_iface.c slp_xmalloc.c slp_linkedlist.c slp_compare.c
 *==========================================================================*/
/* #define SLP_IFACE_TEST */
#ifdef SLP_IFACE_TEST 
int main(int argc, char* argv[])
{
    int i;
    int addrscount =  10;
    struct sockaddr_storage addrs[10];
    SLPIfaceInfo ifaceinfo;
    char* addrstr;

#ifdef _WIN32
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2,2), &wsadata);
#endif

    if(SLPIfaceGetInfo(NULL,&ifaceinfo, AF_INET) == 0)
    {
        for(i=0;i<ifaceinfo.iface_count;i++)
        {
            char myname[MAX_HOST_NAME];


            SLPNetSockAddrStorageToString(&ifaceinfo.iface_addr[i], myname, sizeof(myname));
            printf("v4 found iface = %s\n", myname);
            SLPNetSockAddrStorageToString(&ifaceinfo.bcast_addr[i], myname, sizeof(myname));
            printf("v4 bcast addr = %s\n", myname);
        }
    }

    if(SLPIfaceGetInfo(NULL,&ifaceinfo, AF_INET6) == 0)
    {
        for(i=0;i<ifaceinfo.iface_count;i++)
        {
            char myname[MAX_HOST_NAME];


            SLPNetSockAddrStorageToString(&ifaceinfo.iface_addr[i], myname, sizeof(myname));
            printf("v6 found iface = %s\n", myname);
        }
        for(i=0;i<ifaceinfo.bcast_count;i++)
        {
            char myname[MAX_HOST_NAME];
            SLPNetSockAddrStorageToString(&ifaceinfo.bcast_addr[i], myname, sizeof(myname));
            printf("v6 bcast addr = %s\n", myname);
        }
    }


    SLPIfaceGetInfo("fec0:0:0:0001:0:0:0:3,5:6::7,10.0.25.82", &ifaceinfo, AF_INET6);
    SLPIfaceGetInfo("fec0:0:0:0001:0:0:0:3,5:6::7,10.0.25.82", &ifaceinfo, AF_INET);
    if(SLPIfaceStringToSockaddrs("192.168.100.1,192.168.101.1",
                                 addrs,
                                 &addrscount) == 0)
    {
        if(SLPIfaceSockaddrsToString(addrs, 
                                         addrscount,
                                         &addrstr) == 0)
        {
            printf("sock addr string v4 = %s\n",addrstr);
            xfree(addrstr);
        }
    }

    if(SLPIfaceStringToSockaddrs("1:2:0:0:0::4,10:0:0:0:0:0:0:11",
                                 addrs,
                                 &addrscount) == 0)
    {
        if(SLPIfaceSockaddrsToString(addrs, 
                                         addrscount,
                                         &addrstr) == 0)
        {
            printf("sock addr string v6 = %s\n",addrstr);
            xfree(addrstr);
        }
    }

#ifdef _WIN32
    WSACleanup();
#endif
}
#endif


