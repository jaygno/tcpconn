#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <stdarg.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>


char *myname;

void usage()
{
    fprintf(stderr, "%s: <ip> <port>\n", myname);
    exit(0);
}

int main(int argc, char **argv)
{
    int fd,ret;
    struct sockaddr_in sin;
    struct timeval tv_start, tv_end;

    myname = argv[0];

    if(argc != 3)
    {
        usage();
    }

    fd=socket(AF_INET,SOCK_STREAM,0);
    if(fd<0)
    {
        perror("socket");
        exit(0);
    }
    
    sin.sin_family=AF_INET;
    sin.sin_port=htons(atoi(argv[2]));

    if(inet_pton(AF_INET,argv[1],&sin.sin_addr.s_addr)<=0)
    {
        perror("inet_pton");
        exit(0);
    }

    ret = gettimeofday(&tv_start, NULL);
    if (ret < 0) {
        perror("gettimeofday");
        exit(1);
    }  

    ret=connect(fd,(struct sockaddr *)&sin,sizeof(sin));
    if(ret<0)
    {
        if(errno == ECONNRESET)
        {
            fprintf(stderr, "%s\n", "Connect reset by peer.");
        }
        else
        {
            fprintf(stderr, "%s\n", "Connect failed");
        }
        exit(0);
    }
    
    ret = gettimeofday(&tv_end, NULL);
    if (ret < 0) {
        perror("gettimeofday");
        exit(1);
    }

    printf("Connect time: %ld ms.\n", (tv_end.tv_sec - tv_start.tv_sec)*1000 + (tv_end.tv_usec - tv_start.tv_usec)/1000 ); 

    close(fd); 

    return 0;
}
