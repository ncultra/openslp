/***************************************************************************/
/*                                                                         */
/* Project:     OpenSLP command line UA wrapper                            */
/*                                                                         */
/* File:        slptool_main.c                                             */
/*                                                                         */
/* Abstract:    Main implementation for slpua                              */
/*                                                                         */
/* Requires:    OpenSLP installation                                       */
/*                                                                         */
/***************************************************************************/

#include "slptool.h"

/*=========================================================================*/
SLPBoolean mySrvTypeCallback( SLPHandle hslp, 
                              const char* srvtypes, 
                              SLPError errcode, 
                              void* cookie ) 
/*=========================================================================*/
{
    if(errcode == SLP_OK)
    {
        printf("%s\n",srvtypes);
    }
}


/*=========================================================================*/
void FindSrvTypes(SLPToolCommandLine* cmdline)
/*=========================================================================*/
{
    SLPError    result;
    SLPHandle   hslp;

    if(SLPOpen(cmdline->lang,SLP_FALSE,&hslp) == SLP_OK)
    {
        result = SLPFindSrvTypes(hslp,
                                 cmdline->cmdparam1,
                                 cmdline->scopes,
                                 mySrvTypeCallback,
                                 0);
        if(result != SLP_OK)
        {
            printf("errorcode: %i",result);
        }
        SLPClose(hslp);
    }
}

/*=========================================================================*/
SLPBoolean myAttrCallback(SLPHandle hslp, 
                        const char* attrlist, 
                        SLPError errcode, 
                        void* cookie )
/*=========================================================================*/
{
    if(errcode == SLP_OK)
    {
        printf("%s\n",attrlist);
    }
}


/*=========================================================================*/
void FindAttrs(SLPToolCommandLine* cmdline)
/*=========================================================================*/
{
    SLPError    result;
    SLPHandle   hslp;

    if(SLPOpen(cmdline->lang,SLP_FALSE,&hslp) == SLP_OK)
    {
        printf("Calling FindAttrs(hslp,%s,%s,%s,callback\n",cmdline->cmdparam1,cmdline->scopes,cmdline->cmdparam2);

        result = SLPFindAttrs(hslp,
                             cmdline->cmdparam1,
                             cmdline->scopes,
                             cmdline->cmdparam2,
                             myAttrCallback,
                             0);
        if(result != SLP_OK)
        {
            printf("errorcode: %i",result);
        }
        SLPClose(hslp);
    }               
}

 
/*=========================================================================*/
SLPBoolean mySrvUrlCallback( SLPHandle hslp, 
                             const char* srvurl, 
                             unsigned short lifetime, 
                             SLPError errcode, 
                             void* cookie ) 
/*=========================================================================*/
{
    if(errcode == SLP_OK)
    {
        printf("%s,%i\n",srvurl,lifetime);
    }
}


/*=========================================================================*/
void FindSrvs(SLPToolCommandLine* cmdline)
/*=========================================================================*/
{
    SLPError    result;
    SLPHandle   hslp;

    if(SLPOpen(cmdline->lang,SLP_FALSE,&hslp) == SLP_OK)
    {
        result = SLPFindSrvs(hslp,
                             cmdline->cmdparam1,
                             cmdline->scopes,
                             cmdline->cmdparam2,
                             mySrvUrlCallback,
                             0);
        if(result != SLP_OK)
        {
            printf("errorcode: %i",result);
        }
        SLPClose(hslp);
    }               
}

/*=========================================================================*/
int ParseCommandLine(int argc,char* argv[], SLPToolCommandLine* cmdline)
/* Returns  Zero on success.  Non-zero on error.
/*=========================================================================*/
{
    int i;

    if(argc < 2)
    {
        /* not enough arguments */
        return 1;
    }

    for (i=1;i<argc;i++)
    {
        if( strcasecmp(argv[i],"-s") == 0 ||
            strcasecmp(argv[i],"--scopes") == 0 )
        {
            i++;
            if(i < argc)
            {
                cmdline->scopes = argv[i];
            }
            else
            {
                return 1;
            }
        }
        else if( strcasecmp(argv[i],"-l") == 0 ||
                 strcasecmp(argv[i],"--lang") == 0 )
        {
            i++;
            if(i < argc)
            {
                cmdline->lang = argv[i];
            }
            else
            {
                return 1;
            }
        }
        else if(strcasecmp(argv[i],"findsrvs") == 0)
        {
            cmdline->cmd = FINDSRVS;
            
            /* service type */
            i++;
            if(i < argc)
            {
                cmdline->cmdparam1 = argv[i];
            }
            else
            {
                return 1;
            }
            
            /* (optional) filter */
            i++;
            if(i < argc)
            {
                cmdline->cmdparam2 = argv[i];
            }
            
            break;
        }
        else if(strcasecmp(argv[i],"findattrs") == 0)
        {
            cmdline->cmd = FINDATTRS;     
                  
            /* url or service type */
            i++;
            if(i < argc)
            {
                cmdline->cmdparam1 = argv[i];
            }
            else
            {
                return 1;
            }
            
            /* (optional) attrids */
            i++;
            if(i < argc)
            {
                cmdline->cmdparam2 = argv[i];
            }
        }
        else if(strcasecmp(argv[i],"findsrvtypes") == 0)
        {
            cmdline->cmd = FINDSRVTYPES;

            /* (optional) naming authority */
            i++;
            if(i < argc)
            {
                cmdline->cmdparam1 = argv[i];
            }                             
        }
        else
        {
            return 1;
        }    
    }

    return 0;
}


/*=========================================================================*/
void DisplayUsage()
/*=========================================================================*/
{
    printf("Usage: slptool [options] command-and-arguments \n");
    printf("   options may be:\n");
    printf("      -s (or --scope) followed by a comma separated list of scopes\n");
    printf("      -l (or --language) followed by a language tag\n");
    printf("   command-and-arguments may be:\n");
    printf("      findsrvs service-type [filter]\n");
    printf("      findattrs url [attrids]\n");
    printf("      findsrvtypes [authority]\n");
    
}


/*=========================================================================*/
int main(int argc, char* argv[])
/*=========================================================================*/
{
    int                 result;
    SLPToolCommandLine  cmdline;

    /* zero out the cmdline */
    memset(&cmdline,0,sizeof(cmdline));
    
    /* Parse the command line */
    if(ParseCommandLine(argc,argv,&cmdline) == 0)
    {
        switch(cmdline.cmd)
        {
        case FINDSRVS:
            FindSrvs(&cmdline);
            break;
        
        case FINDATTRS:
            FindAttrs(&cmdline);
            break;
        
        case FINDSRVTYPES:
            FindSrvTypes(&cmdline);
            break;
        
        case GETPROPERTY:
//            GetProperty(&cmdline);
            break;
        }
    }
    else
    {
        DisplayUsage();
        result = 1;
    }

    return 0;
}
