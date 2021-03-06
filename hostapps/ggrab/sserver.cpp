/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

program: sserver version 0.03 by Axel Buehning <mail at diemade.de>
*/

#include <stdlib.h>
#include <netdb.h>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#ifdef __CYGWIN__
#include <cygwin/in.h>
#endif
#include "sserver.h"

#define BUFLEN 10240

int AnalyzeXMLRequest(char *szXML, RecordingData   *rdata);

int main(int argc, char * argv[])
{
	RecordingData recdata;
	struct sockaddr_in  servaddr;
	int ListenSocket, ConnectionSocket;
	u_short Port = 4000;
	char buf[BUFLEN];
	char * p_act;
	int rc;
	int pid = 0;
	time_t t;
	char * a_arg[50];
	char a_grabname[256];
	char a_vpid[20];
	char a_apid[20];
	char a_filename[256];
	char a_path[256]="";
	char a_host[256];
	int	i,n;
	struct sockaddr_in cliaddr;
	socklen_t clilen = sizeof(cliaddr);

	a_arg[0] = a_grabname;
	a_arg[1] = (char *)"-p";
	a_arg[2] = a_vpid;
	a_arg[3] = a_apid;
	a_arg[4] = (char *)"-o";
	a_arg[5] = a_filename;
	a_arg[6] = (char *)"-host";
	a_arg[7] = a_host;
	a_arg[8] = (char *)"-nos";

	strcpy (a_grabname,argv[0]);
	if (strrchr(a_grabname,'/')){
		strcpy (strrchr(a_grabname,'/') + 1, "ggrab");
	}
	else {
		strcpy(a_grabname,"ggrab");
	}
#ifdef __CYGWIN__
	strcat(a_grabname,".exe");
#endif
	n = 9;
	
	for (i = 1; i < argc; i++) {
		if (!strcmp("-o",argv[i])) {
			i++; if (i >= argc) { fprintf(stderr, "need path for -o\n"); return -1; }
			strcpy (a_path, argv[i]);
		}
		else if (!strcmp("-sport",argv[i])) {
			i++; if (i >= argc) { fprintf(stderr, "need port for -sport\n"); return -1; }
			Port = atoi(argv[i]);
		} else if (!strcmp("-h", argv[i])) {
			fprintf(stderr, "sserver version 0.03, Copyright (C) 2002 Axel Buehning\n"
			                "sserver comes with ABSOLUTELY NO WARRANTY; This is free software,\n"
			                "and you are welcome to redistribute it under the conditions of the\n"
			                "GNU Public License, see www.gnu.org\n"
					"------- mandatory options: ---------\n"
					"-p <pid1> <pid2> <pidn> video and audio pids to receive [none]\n"
					"\n"
					"------- basic options: -------------\n"
					"-sport <port>            port for streaming server [4000]\n"
					"\n" 
					"for other options see ggrab -h\n");
					exit(0);
		}
		else {
			a_arg[n++]=argv[i];
		}
	}
	a_arg[n] = 0;

	//network-setup
	ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(Port);

	i = 0;
	while ((rc = bind(ListenSocket, (sockaddr *)&servaddr, sizeof(struct sockaddr_in))))
	{
		fprintf(stderr, "bind to port %d failed, RC=%d...\n",Port, rc);
		if (i == 10) {
			fprintf(stderr, "Giving up\n");
			exit(1);
		}
		fprintf(stderr, "%d. try, wait for 2 s\n",i++);
		sleep(2);
	}

	if (listen(ListenSocket, 5))
	{
		fprintf(stderr,"listen failed\n");
		exit(1);
	}
	printf("server startet\n");

	do
	{
		memset(&cliaddr, 0, sizeof(cliaddr));
		if((ConnectionSocket = accept(ListenSocket, (struct sockaddr *) &cliaddr, (socklen_t *) &clilen)) == -1){
			fprintf(stderr,"accept failed\n");
			exit(1);
		}
		strcpy(a_host,inet_ntoa(cliaddr.sin_addr));
		fprintf(stderr,"request from dbox ip :%s\n",a_host);

		do
		{
			rc = recv(ConnectionSocket, buf, BUFLEN, 0);
			if ((rc > 0))
			{
				memset((void *)&recdata, 0, sizeof(recdata));
				AnalyzeXMLRequest(buf, &recdata);

				switch (recdata.cmd) 
				{
					case CMD_VCR_UNKNOWN:
						fprintf(stderr, "VCR_UNKNOWN NOT HANDLED\n");
						break;
					case CMD_VCR_RECORD:
						fprintf(stderr, "********************** START RECORDING **********************\n");
						fprintf(stderr, "ONIDSID     : %x\n", recdata.onidsid);
						fprintf(stderr, "APID        : %x\n", recdata.apid);
						fprintf(stderr, "VPID        : %x\n", recdata.vpid);
						fprintf(stderr, "CHANNELNAME : %s\n", recdata.channelname);
						fprintf(stderr, "EPG TITLE   : %s\n", recdata.epgtitle);
						fprintf(stderr, "***********************************************************\n");
						sprintf(a_vpid,"0x%03x",recdata.vpid);	
						sprintf(a_apid,"0x%03x",recdata.apid);

						strcpy (a_filename,a_path);
							
						if (strlen(a_filename)) {
							strcat(a_filename,"/");
						}

						if (strlen(recdata.channelname) > 0)
						{
							p_act = recdata.channelname;
							do {
								p_act +=  strcspn(p_act, "/ \"%&-\t`'�!,:;");
								if (*p_act) {
									*p_act++ = '_';
								}
							} while (*p_act);
								
							strcat(a_filename, recdata.channelname);
							strcat(a_filename, "_");
						}

						if (strlen(recdata.epgtitle) > 0)
						{
							p_act = recdata.epgtitle;
							do {
								p_act +=  strcspn(p_act, "/ \"%&-\t`'~<>!,:;?^�$\\=*#@�|��������");
								if (*p_act) {
									*p_act++ = '_';
								}
							} while (*p_act);

							p_act = recdata.epgtitle;
							do {
								if ((unsigned char) (*p_act) > 128) {
									*p_act = '_';
								}
							} while (*p_act++);
							
							strcat(a_filename, recdata.epgtitle);
							strcat(a_filename, "_");
						}

						t = time (&t);
						strftime (buf, sizeof(a_filename)-1, "%Y%m%d_%H%M%S", localtime(&t));
						strcat(a_filename, buf);

						pid = fork();
						if (pid == -1) {
							fprintf(stderr, "fork process failed\n");
							break;
						}
						if (pid == 0) {
							execvp(a_arg[0], a_arg);
							fprintf(stderr,"execv failed");
							perror("");
						}
						break;
					case CMD_VCR_STOP:
						if (pid > 0) {
							if(kill(pid,SIGINT)) {
								printf ("ggrab process not killed\n");
							}
							waitpid(pid,0,0);
							fprintf(stderr,"\nStop recording\n");
						}
						break;
					case CMD_VCR_PAUSE:
						fprintf(stderr, "VCR_PAUSE NOT HANDLED\n");
						break;
					case CMD_VCR_RESUME:
						fprintf(stderr, "VCR_RESUME NOT HANDLED\n");
						break;
					case CMD_VCR_AVAILABLE:
						fprintf(stderr, "VCR_AVAIABLE NOT HANDLED\n");
						break;
					default:
						fprintf(stderr, "unknown VCR command\n");
						break;
				}
			}
		} while((rc > 0));
	} while (true);

	return 0;
}


/* Shameless stolen from TuxVision */
char* ParseForString(char *szStr, const char *szSearch, int ptrToEnd)
{
    char *p=NULL;
    p=strstr(szStr, szSearch);
    if (p==NULL)
        return(NULL);
    if (!ptrToEnd)
        return(p);
    else
        return(p+strlen(szSearch));
    
}

char* ExtractQuotedString(char *szStr, char *szResult, int ptrToEnd)
{
    int i=0;
    int len=strlen(szStr);
    strcpy(szResult,"");
    for(i=0;i<len;i++)
        {
        if (szStr[i]=='\"')
            {
            char *pe=NULL;
            char *ps=szStr+i+1;      
            pe=ParseForString(ps, "\"", 1);
            if (pe==NULL)
                return(NULL);

            memcpy(szResult,ps, pe-ps-1);
            szResult[pe-ps-1]=0;                       
            if (ptrToEnd)
                return(pe);
            else
                return(ps);
            }
        }
    return(NULL);
}

int AnalyzeXMLRequest(char *szXML, RecordingData   *rdata)
{
    char *p1=NULL;
    char *p2=NULL;
    char *p3=NULL;
    char szcommand[264]="";
    char szonidsid[264]="";
    char szapid[264]="";
    char szvpid[264]="";
    char szmode[3]="";
    char szchannelname[264]="";
    char szepgtitle[264]="";
    int hr=false;

    if ( (szXML==NULL) || (rdata==NULL) )
        return(false);

    rdata->apid=0;
    rdata->vpid=0;
    rdata->cmd=CMD_VCR_UNKNOWN;
    rdata->onidsid=0;
    strcpy(rdata->channelname,"");

    p1=ParseForString(szXML,"<record ", 1);
    if (p1!=NULL)
        {
        p2=ParseForString(p1,"command=", 1);
        p1=NULL;
        if (p2!=NULL)
            p3=ExtractQuotedString(p2, szcommand, 1);
        if (p3!=NULL)
            p1=ParseForString(p3,">", 1);
        }

    if (p1!=NULL)
        {
        p2=ParseForString(p1,"<channelname>", 1);
        p3=p2;
        p1=NULL;
        if (p2!=NULL)
            {
            p1=ParseForString(p2,"</channelname>", 0);
            if (p1!=NULL)
                {
                memcpy(szchannelname,p3,p1-p3);
                szchannelname[p1-p3]=0;
                hr=true;
                }
            }
        }
    if (p1!=NULL)
        {
        p2=ParseForString(p1,"<epgtitle>", 1);
        p3=p2;
        p1=NULL;
        if (p2!=NULL)
            {
            p1=ParseForString(p2,"</epgtitle>", 0);
            if (p1!=NULL)
                {
                memcpy(szepgtitle,p3,p1-p3);
                szepgtitle[p1-p3]=0;
                hr=true;
                }
            }
        }
    if (p1!=NULL)
        {
        p2=ParseForString(p1,"<onidsid>", 1);
        p3=p2;
        p1=NULL;
        if (p2!=NULL)
            {
            p1=ParseForString(p2,"</onidsid>", 0);
            if (p1!=NULL)
                {
                memcpy(szonidsid,p3,p1-p3);
                szonidsid[p1-p3]=0;
                hr=true;
                }
            }
        }
    
    if (p1!=NULL)
        {
        p2=ParseForString(p1,"<mode>", 1);
        p3=p2;
        p1=NULL;
        if (p2!=NULL)
            {
            p1=ParseForString(p2,"</mode>", 0);
            if (p1!=NULL)
                {
                memcpy(szmode,p3,p1-p3);
                szmode[p1-p3]=0;
                hr=true;
                }
            }
        }
    
    p2=ParseForString(szXML,"<videopid>", 1);
    if (p2!=NULL)
        {
        p3=p2;
        p1=NULL;
        p1=ParseForString(p2,"</videopid>", 0);
        if (p1!=NULL)
            {
            memcpy(szvpid,p3,p1-p3);
            szvpid[p1-p3]=0;
            hr=true;
            }
        }

    p2=ParseForString(szXML,"<audiopids selected=", 1);
    if (p2!=NULL)
        {
        p3=ExtractQuotedString(p2, szapid, 1);
        if (p3!=NULL)
            p1=ParseForString(p3,">", 1);
        }

    if (!hr)
        return(hr);

    strcpy(rdata->channelname, szchannelname);
    strcpy(rdata->epgtitle, szepgtitle);

    if (strlen(szvpid)>0)
        rdata->vpid=atoi(szvpid);

    if (strlen(szapid)>0)
        rdata->apid=atoi(szapid);

    if (!strcmp(szcommand,"record"))
        rdata->cmd=CMD_VCR_RECORD;
    if (!strcmp(szcommand,"stop"))
        rdata->cmd=CMD_VCR_STOP;
    if (!strcmp(szcommand,"pause"))
        rdata->cmd=CMD_VCR_PAUSE;
    if (!strcmp(szcommand,"resume"))
        rdata->cmd=CMD_VCR_RESUME;
    if (!strcmp(szcommand,"available"))
        rdata->cmd=CMD_VCR_AVAILABLE;

    rdata->onidsid=atol(szonidsid);

    return(hr);
}
