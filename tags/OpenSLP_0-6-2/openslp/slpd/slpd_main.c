/***************************************************************************/
/*                                                                         */
/* Project:     OpenSLP - OpenSource implementation of Service Location    */
/*              Protocol Version 2                                         */
/*                                                                         */
/* File:        slpd_main.c                                                */
/*                                                                         */
/* Abstract:    Main daemon loop                                           */
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
#include <grp.h>

/*==========================================================================*/
int G_SIGALRM   = 0;
int G_SIGTERM   = 0;
int G_SIGHUP    = 0;
/*==========================================================================*/                                                                                                 

/*--------------------------------------------------------------------------*/
void SignalHandler(int signum)
/*--------------------------------------------------------------------------*/
{
    switch(signum)
    {
    case SIGALRM:
        G_SIGALRM = 1;
        break;

    case SIGTERM:
        G_SIGTERM = 1;

    case SIGHUP:
        G_SIGHUP = 1;
        
    case SIGPIPE:
    default:
        return;
    }
}

/*-------------------------------------------------------------------------*/
int SetUpSignalHandlers()
/*-------------------------------------------------------------------------*/
{
    int result;
    struct sigaction sa;

    sa.sa_handler    = SignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags      = 0;//SA_ONESHOT;
    sa.sa_restorer   = 0;
    
    result = sigaction(SIGALRM,&sa,0);
    result |= sigaction(SIGTERM,&sa,0);
    result |= sigaction(SIGPIPE,&sa,0);
    
    signal(SIGHUP,SignalHandler);
    //result |= sigaction(SIGHUP,&sa,0);

    return result;
}


/*-------------------------------------------------------------------------*/
int Daemonize(const char* pidfile)
/* Turn the calling process into a daemon (detach from tty setuid(), etc   */
/*                                                                         */      
/* Returns: zero on success non-zero if slpd could not daemonize (or if    */
/*          slpd is already running                             .          */
/*-------------------------------------------------------------------------*/
{
    pid_t   pid;
    FILE*   fd;
    struct  passwd* pwent;
    char    pidstr[13];
    
    if(G_SlpdCommandLine.detach)
    {
    
        /*-------------------------------------------*/
        /* Release the controlling tty and std files */
        /*-------------------------------------------*/
        switch(fork())
        {
        case -1:
            return -1;
        case 0:
            /* child livs */
            break;
    
        default:
            /* parent dies */
            exit(0);
        }
    
        close(0); 
        close(1); 
        close(2); 
        setsid(); /* will only fail if we are already the process group leader */
    }
     
    /*------------------------------------------*/
    /* make sure that we're not running already */
    /*------------------------------------------*/
    /* read the pid from the file */
    fd = fopen(pidfile,"r");
    if(fd)
    {
        fread(pidstr,13,1,fd);
        fclose(fd);
        pid = atoi(pidstr);
        if(pid)
        {
            if(kill(pid,0) == 0)
            {
                /* we are already running */
                SLPFatal("slpd daemon is already running\n");
                return -1;
            }
        }    
    }
    /* write my pid to the pidfile */
    fd = fopen(pidfile,"w");
    if(fd)
    {
        sprintf(pidstr,"%i",getpid());
        fwrite(pidstr,strlen(pidstr),1,fd);
        fclose(fd);
    }
    
    /*----------------*/
    /* suid to daemon */
    /*----------------*/
    /* TODO: why do the following lines mess up my signal handlers? */
    pwent = getpwnam("daemon"); 
    if(pwent)
    {
        if( setgroups(1, &pwent->pw_gid) < 0 ||
            setgid(pwent->pw_gid) < 0 ||
            setuid(pwent->pw_uid) < 0 )
        {
            /* TODO: should we log here and return fail */
        }
    }
    
    return 0;
}

                  
/*-------------------------------------------------------------------------*/
void HandleTCPListen(SLPDSocketList* list, SLPDSocket* sock)
/*-------------------------------------------------------------------------*/
{
    int                 fd;
    struct sockaddr     peeraddr;
    socklen_t           peeraddrlen;

    /* check to see if we have accepted the maximum number of sockets */
    if(list->count < SLPD_MAX_SOCKETS)
    {
        peeraddrlen = sizeof(peeraddr);
        fd = accept(sock->fd, &peeraddr, &peeraddrlen);
        if(fd >= 0)
        {
            SLPDSocketListAdd(list,fd,TCP_FIRST_READ);
            /* ignore return code */
        }
    }
}


/*-------------------------------------------------------------------------*/
void HandleUDPRead(SLPDSocketList* list, SLPDSocket* sock)
/*-------------------------------------------------------------------------*/
{
    int                 bytesread;
    int                 bytestowrite;
    struct sockaddr_in  peeraddr;
    int                 peeraddrlen    = sizeof(peeraddr);

    memset(&peeraddr,0,sizeof(peeraddr));
    if(sock->recvbuf && sock->sendbuf)
    {
        bytesread = recvfrom(sock->fd,
                              sock->recvbuf->start,
                              SLP_MAX_DATAGRAM_SIZE,
                              0,
                              &peeraddr,
                              &peeraddrlen);
        if(bytesread > 0)
        {
            sock->recvbuf->end = sock->recvbuf->start + bytesread;

            if(SLPDProcessMessage(sock->recvbuf,sock->sendbuf) == 0)
            {
                /* check to see if we should send anything */
                bytestowrite = sock->sendbuf->end - sock->sendbuf->start;
                if(bytestowrite > 0)
                {
                    sendto(sock->fd,
                           sock->sendbuf->start,
                           sock->sendbuf->end - sock->sendbuf->start,
                           0,
                           &peeraddr,
                           peeraddrlen);
                }
            }
            else
            {
                SLPLog("An error occured while processing message from %s\n",
                       inet_ntoa(peeraddr.sin_addr));
            } 
        }
    }
    else
    {
        /* we're out of memory, drop the UDP datagram */
        recv(sock->fd,&bytesread,4,0);  
        /* &bytesread serves as a (small) temporary bucket to drop data into */
        SLPLog("Slpd is out of memory!!\n");
    }
}


/*-------------------------------------------------------------------------*/
void HandleTCPRead(SLPDSocketList* list, SLPDSocket* sock)
/*-------------------------------------------------------------------------*/
{
    int     fdflags;
    int     bytesread;
    char    peek[16];
    
    if(sock->type == TCP_FIRST_READ)
    {
        fdflags = fcntl(sock->fd, F_GETFL, 0);
        fcntl(sock->fd,F_SETFL, fdflags | O_NONBLOCK);
        
        /*---------------------------------------------------------------*/
        /* take a peek at the packet to get version and size information */
        /*---------------------------------------------------------------*/
        bytesread = recvfrom(sock->fd,
                             peek,
                             16,
                             MSG_PEEK,
                             &(sock->peeraddr),
                             &(sock->peeraddrlen));
        if(bytesread > 0)
        {
            /* check the version */
            if(*peek == 2)
            {
                /* allocate the recvbuf big enough for the whole message */
                sock->recvbuf = SLPBufferRealloc(sock->recvbuf,AsUINT24(peek+2));
                if(sock->recvbuf)
                {
                    sock->type = TCP_READ;
                }
                else
                {
                    SLPLog("Slpd is out of memory!\n");
                    sock->type = SOCKET_CLOSE;
                }
            }
            else
            {
                SLPLog("Unsupported version %i received from %s\n",
                       *peek,
                       inet_ntoa(sock->peeraddr.sin_addr));

                sock->type = SOCKET_CLOSE;
            }
        }
        else
        {
            if(errno != EWOULDBLOCK)
            {
                sock->type = SOCKET_CLOSE;
                return;
            }
        }        

        
        /*------------------------------*/
        /* recv the rest of the message */
        /*------------------------------*/
        bytesread = recv(sock->fd,
                         sock->recvbuf->curpos,
                         sock->recvbuf->end - sock->recvbuf->curpos,
                         0);              

        if(bytesread > 0)
        {
            /*------------------------------*/
            /* Reset the timestamp          */
            /*------------------------------*/
            time(&(sock->timestamp));
            
            sock->recvbuf->curpos += bytesread;
            if(sock->recvbuf->curpos == sock->recvbuf->end)
            {
                if(SLPDProcessMessage(sock->recvbuf,sock->sendbuf) == 0)
                {
                    sock->type = TCP_FIRST_WRITE;
                }
                else
                {
                    /* An error has occured in SLPDProcessMessage() */
                    SLPLog("An error while processing message from %s\n",
                           inet_ntoa(sock->peeraddr.sin_addr));
                    sock->type = SOCKET_CLOSE;
                }                                                          
            }
        }
        else
        {
            if(errno != EWOULDBLOCK)
            {
                /* error in recv() */
                sock->type = SOCKET_CLOSE;
            }
        }
    }
}


/*-------------------------------------------------------------------------*/
void HandleTCPWrite(SLPDSocketList* list, SLPDSocket* sock)
/*-------------------------------------------------------------------------*/
{
    int byteswritten;
    
    if(sock->type == TCP_FIRST_WRITE)
    {
        /* make sure that the start and curpos pointers are the same */
        sock->sendbuf->curpos = sock->sendbuf->start;
        sock->type = TCP_WRITE;
    }

    if(sock->sendbuf->end - sock->sendbuf->start != 0)
    {
        byteswritten = send(sock->fd,
                            sock->sendbuf->curpos,
                            sock->sendbuf->end - sock->sendbuf->start,
                            MSG_DONTWAIT);
        if(byteswritten > 0)
        {
            /*------------------------------*/
            /* Reset the timestamp          */
            /*------------------------------*/
            time(&(sock->timestamp));
    
            sock->sendbuf->curpos += byteswritten;
            if(sock->sendbuf->curpos == sock->sendbuf->end)
            {
                /* message is completely sent */
                sock->type = TCP_FIRST_READ;
             }
        }
        else
        {
            if(errno != EWOULDBLOCK)
            {
                /* Error occured or connection was closed */
                sock->type = SOCKET_CLOSE;
            }   
        }    
    }
}


/*=========================================================================*/
int main(int argc, char* argv[])
/*=========================================================================*/
{
    
    fd_set          readfds;
    fd_set          writefds;
    int             highfd      = 0;
    int             fdcount     = 0;
    SLPDSocket*     sock        = 0;
    SLPDSocket*     del         = 0;
    SLPDSocketList  socketlist  = {0,0};
    
    /*------------------------------*/
    /* Make sure we are root        */
    /*------------------------------*/
    if(getuid() != 0)
    {
        SLPFatal("slpd must be started by root\n");
    }
    
    /*------------------------*/
    /* Parse the command line */
    /*------------------------*/
    if(SLPDParseCommandLine(argc,argv))
    {
        SLPFatal("Invalid command line\n");
    }

    /*------------------------------*/
    /* Initialize the log file      */
    /*------------------------------*/
    SLPLogFileOpen(G_SlpdCommandLine.logfile, 0);

    SLPLog("****************************************\n");
    SLPLog("*** SLPD daemon started              ***\n");
    SLPLog("****************************************\n");
    SLPLog("command line = %s\n",argv[0]);
    
    /*--------------------------------------------------*/
    /* Initialize for the first time                    */
    /*--------------------------------------------------*/
    SLPDPropertyInit(G_SlpdCommandLine.cfgfile);
    SLPDDatabaseInit(G_SlpdCommandLine.regfile);
    SLPDSocketInit(&socketlist);
    
    /*---------------------------*/
    /* make slpd run as a daemon */
    /*---------------------------*/
    if(Daemonize(G_SlpdCommandLine.pidfile))
    {
        SLPFatal("Could not run as daemon\n");
    }
    
    /*-----------------------*/
    /* Setup signal handlers */ 
    /*-----------------------*/
    if(SetUpSignalHandlers())
    {
        SLPFatal("Could not set up signal handlers.\n");
    }

    /*------------------------------*/
    /* Set up alarm to age database */
    /*------------------------------*/
    alarm(SLPD_AGE_TIMEOUT);

    
    /*-----------*/
    /* Main loop */
    /*-----------*/
    while(G_SIGTERM == 0)
    {
        if(G_SIGHUP)
        {
            /* Reinitialize */
            SLPLog("****************************************\n");
            SLPLog("*** SLPD daemon restarted            ***\n");
            SLPLog("****************************************\n");
            SLPLog("Got SIGHUP reinitializing... \n");
            SLPDPropertyInit(G_SlpdCommandLine.cfgfile);
            SLPDDatabaseInit(G_SlpdCommandLine.regfile);
            SLPDSocketInit(&socketlist);                
            G_SIGHUP = 0;
        }

        /*--------------------------------------------------------*/
        /* Load the fdsets up with all of the sockets in the list */
        /*--------------------------------------------------------*/
        highfd = 0;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        sock = socketlist.head;
        while(sock)
        {
            if(sock->fd > highfd)
            {
                highfd = sock->fd;
            }

            switch(sock->type)
            {
            case UDP:
            case UDP_MCAST:
                FD_SET(sock->fd,&readfds);
                break;
                
            case TCP_LISTEN:
                if(socketlist.count < SLPD_MAX_SOCKETS)
                {
                    FD_SET(sock->fd,&readfds);
                }
                break;
    
            case TCP_READ:
            case TCP_FIRST_READ:
                FD_SET(sock->fd,&readfds);
                break;
  
            case TCP_WRITE:
            case TCP_FIRST_WRITE:
                FD_SET(sock->fd,&writefds);
                break;

            default:
                break;
            }
    
            sock = (SLPDSocket*)sock->listitem.next;
        }
        
        /*-----------------------------------------------*/
        /* Check to see if we we should age the database */
        /*-----------------------------------------------*/
        /* there is a reason this is here instead of somewhere else, but I */
        /* can't remember what it was.                                     */
        if(G_SIGALRM)
        {
            SLPDDatabaseAge(SLPD_AGE_TIMEOUT);
            G_SIGALRM = 0;
            alarm(SLPD_AGE_TIMEOUT);
        }
        
        /*-------------*/
        /* Main select */
        /*-------------*/
        fdcount = select(highfd+1,&readfds,&writefds,0,0);
        if(fdcount > 0)
        {
            sock = socketlist.head;
            while(sock && fdcount)
            {
                if(FD_ISSET(sock->fd,&readfds))
                {
                    switch(sock->type)
                    {
                    
                    case TCP_LISTEN:
                        HandleTCPListen(&socketlist,sock);
                        break;

                    case UDP:
                    case UDP_MCAST:
                        HandleUDPRead(&socketlist,sock);
                        break;                      
                
                    case TCP_READ:
                    case TCP_FIRST_READ:
                        HandleTCPRead(&socketlist,sock);
                        break;

                    default:
                        break;
                    }

                    fdcount --;
                } 

                if(FD_ISSET(sock->fd,&writefds))
                {
                    HandleTCPWrite(&socketlist,sock);
                    fdcount --;
                }   

                /* TODO: Close aged sockets */

                
                if(sock->type == SOCKET_CLOSE)
                {
                    del = sock;
                    sock = (SLPDSocket*)sock->listitem.next;
                    SLPDSocketListRemove(&socketlist,del);
                }
                else
                {
                    sock = (SLPDSocket*)sock->listitem.next;
                }
            }
        }
    }

    SLPLog("Got SIGTERM.  Going down\n");

    return 0;
}




