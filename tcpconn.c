#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdio.h>
#include <stdint.h>

#define MAX_HOST 1024

int dest_port = 80;

int timeout = 1;
int host_num = 0;

char *myname = NULL;
char *filename = NULL;

typedef struct host_entry
{
char *dest_name;
char *dest_quad;
in_addr_t dest_ip;
} HOST_ENTRY;

HOST_ENTRY host_array[MAX_HOST];

void add_host(char *dst_host);

char *myname;

void usage()
{
    FILE *out = stderr;
	fprintf(stderr, "Usage : %s [-h] [-t timeout] [-f filename] remote_host\n", myname);
    fprintf(out, "\n" );
    fprintf(out, "Options:\n" );
    fprintf(out, "   -h         this help\n" );
    fprintf(out, "   -f file    read list of targets from a file\n" );
    fprintf(out, "   -p port    set the target port (default %d)\n", 80);
    fprintf(out, "   -t n       individual target initial timeout (in sec) (default %d)\n", 1);
    fprintf(out, "\n");
    exit(0);
} /* usage() */

int socket_set_noblock(int socket)
{
    int opts = fcntl(socket, F_GETFL, 0); 
    opts = opts | O_NONBLOCK;
    if( fcntl(socket, F_SETFL, opts) < 0 ) { 
        perror("set listen socket non-block error! ");
        return -1; 
    }   

    return 0;
}

void add_host(char *dst_host)
{
	struct in_addr dest_addr;
    char *dest_quad = NULL;
    struct hostent *he = NULL;

    if (host_num == MAX_HOST)
    {
        fprintf(stderr, " %d host. %s", host_num, "too many host (max 80)");
        exit(1);
    }

	he = gethostbyname(dst_host);
	if (!he) {
		herror("gethostbyname");
		exit(1);
	}

	if (!he->h_addr) {
		fprintf(stderr, "No address associated with name: %s\n", dst_host);
		exit(1);
	}

	bcopy(he->h_addr, &host_array[host_num].dest_ip, sizeof(in_addr_t));
	if (host_array[host_num].dest_ip == INADDR_NONE) {
		perror("bad address");
		exit(1);
	}
    //host_array[host_num].dest_name = strdup(he->h_name);
    host_array[host_num].dest_name = strdup(dst_host);

	bzero(&dest_addr, sizeof(struct in_addr));
	dest_addr.s_addr = host_array[host_num].dest_ip;
	dest_quad = inet_ntoa(dest_addr);
	if (dest_quad == NULL) {
		perror("Unable to convert returned address to dotted-quad; try pinging by IP address");
		exit(1);
	}
	host_array[host_num].dest_quad = strdup(dest_quad);
	
    host_num++;
}


void tcpconn()
{
    int i,fd,ret;
    int error=0;
    socklen_t len;
    struct sockaddr_in sin;
    struct timeval tv_start, tv_end, to;
    fd_set readset, writeset;
    int used;

    for (i=0; i<host_num; i++)
    {
        error = 0;
        fd=socket(AF_INET,SOCK_STREAM,0);
        if(fd<0)
        {
            perror("socket");
            exit(0);
        }
        
        sin.sin_family=AF_INET;
        sin.sin_port=htons(dest_port);

        if(inet_pton(AF_INET, host_array[i].dest_quad,&sin.sin_addr.s_addr)<=0)
        {
            perror("inet_pton");
            exit(0);
        }

        ret = gettimeofday(&tv_start, NULL);
        if (ret < 0) {
            perror("gettimeofday");
            exit(1);
        } 

        socket_set_noblock(fd); 

        ret=connect(fd,(struct sockaddr *)&sin,sizeof(sin));
        if(ret!=0)
        {
            if (errno != EINPROGRESS)
            {
                close(fd); 
                fprintf(stdout, "Connected %s (%s:%d) failed.\n", host_array[i].dest_name, host_array[i].dest_quad, dest_port);
                continue;
            }
        }
        else
        {
            goto calculate;
        }

select_again:
        to.tv_sec = timeout;
        to.tv_usec = 0;

        FD_ZERO( &readset );
        FD_ZERO( &writeset );
        FD_SET( fd, &writeset );

        ret = select( fd + 1, &readset, &writeset, NULL, &to);

        if(ret < 0) {
            if(errno == EINTR) {
                /* interrupted system call: redo the select */
                goto select_again;
            }
            else {
                perror("select");
                exit(1);
            }
        }

        if( ret == 0 )
        {
            close(fd); 
            fprintf(stdout, "Connected %s (%s:%d) timeout.\n", host_array[i].dest_name, host_array[i].dest_quad, dest_port);
            continue;
        }

        if (getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&len) < 0)
        {
            close(fd);
            fprintf(stdout, "Connected %s (%s:%d) failed.\n", host_array[i].dest_name, host_array[i].dest_quad, dest_port);
            continue;
        }

calculate:

        if (error == 0)
        {
            ret = gettimeofday(&tv_end, NULL);
            if (ret < 0) {
                perror("gettimeofday");
                exit(1);
            }

            used = (tv_end.tv_sec - tv_start.tv_sec)*1000 + (tv_end.tv_usec - tv_start.tv_usec)/1000; 

            fprintf(stdout, "Connected %s (%s:%d) - %d ms.\n", host_array[i].dest_name, host_array[i].dest_quad, dest_port, used ? used:1);
        }
        else if(error == ECONNRESET)
        {
            fprintf(stdout, "Connected %s (%s:%d) reset by peer.\n", host_array[i].dest_name, host_array[i].dest_quad, dest_port);
        }
        else
        {
            fprintf(stdout, "Connected %s (%s:%d) failed.\n", host_array[i].dest_name, host_array[i].dest_quad, dest_port);
        }

        close(fd); 
    }
}

int main(int argc, char **argv)
{
    int c;

    myname = argv[0];

    memset(host_array, 0, sizeof(host_array));

	while ((c = getopt(argc, argv, "hf:t:p:")) != -1) {
		switch (c) {
			case 'f':
				filename = optarg;
				break;
			case 'p':
				dest_port = atoi(optarg);
				break;
			case 'h':
                usage();
				break;
			case 't':
				timeout = atoi(optarg);
				break;
			default:
				usage();
		}
	}


    argc -= optind;
	argv += optind;

    if ( (*argv && filename) || (!*argv && !filename) )
    {
        usage();
    }
    if( *argv )
    {
        while( *argv )
        {
            add_host( *argv );
            ++argv;
        }
    }else if( filename )
    {
        FILE *ping_file;
        char line[132];
        char host[132];

        ping_file = fopen( filename, "r" );

        if( !ping_file )
        {
            perror("-f");
            exit(1);
        }

        while( fgets( line, sizeof(line), ping_file ) )
        {
            if( sscanf( line, "%s", host ) != 1 )
                continue;

            if( ( !*host ) || ( host[0] == '#' ) )  /* magic to avoid comments */
                continue;

            add_host(host);
        }/* WHILE */

        fclose( ping_file );
    }
    else
    {
        usage();
    }


    if ( !host_num )
    {
        exit(1);
    }

    tcpconn();

    return 0;    
}
