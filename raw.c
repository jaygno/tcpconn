/* =====================================================================================  
 *  
 *       Filename:  raw.c  
 *  
 *    Description:  使用原始套接字发送TCP协议，并外带自己的数据。  
 *  
 *        Version:  1.0  
 *       Compiler:  GCC   
 *  
 *         Author:  jaygno  
 *  
 * =====================================================================================  
 */  
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <netinet/ip.h>  
#include <netinet/tcp.h>  
#include <arpa/inet.h>  


struct option_mss {
    uint8_t   kind;
    uint8_t  length;
    uint16_t  value;
};

struct option_sack {
    uint8_t   kind;
    uint8_t   length;
    uint8_t   nop1;
    uint8_t   nop2;
};

#define PACKET_SIZE sizeof(struct iphdr) + sizeof(struct tcphdr) + sizeof(struct option_mss) + sizeof(struct option_sack)

/*--------------------------------------------------------- 
 Function Name : check_sum() 
   Descrypthon : 校验和计算，摘自UNP源码 
------------------------------------------------------------*/  
unsigned short check_sum(unsigned short *addr, int len)  
{  
    int nleft = len;  
    int sum = 0;  
    unsigned short *w = addr;  
    short answer = 0;  
    while (nleft > 1) {  
        sum += *w++;  
        nleft -=2;  
    }  
    if (nleft == 1) {  
        *(unsigned char *)(&answer) = *(unsigned char *)w;  
        sum += answer;  
    }  
    sum = (sum >> 16) + (sum & 0xffff);  
    sum += (sum >> 16);  
    answer = ~sum;  
    return answer;  
}  
/*--------------------------------------------------------- 
 Function Name : init_socket() 
   Descrypthon : 初始化socket，使用原始套接字 
------------------------------------------------------------*/  
int init_socket(int sockfd, struct sockaddr_in *target,\  
                const char *dst_addr, const char *dst_port)  
{  
    const int flag = 1;  
    target->sin_family = AF_INET;  
    target->sin_port = htons(atoi(dst_port));  
    if (inet_aton(dst_addr, &target->sin_addr) == 0) {  
        perror("inet_aton fail\n");  
        exit(-1);  
    }  
    if((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP)) < 0) {  
        perror("error");  
        exit(-1);  
    }  
    if (setsockopt(sockfd,IPPROTO_IP, IP_HDRINCL, &flag, sizeof(flag)) < 0) {  
        perror("setsockopt fail \n");  
        exit(-1);  
    }  
    return sockfd;  
}  
/*--------------------------------------------------------------- 
 Function Name : build_iphdr() 
   Descrypthon : 构建IP头部数据， 源地址使用伪随机地址 
-----------------------------------------------------------------*/  
void build_iphdr(struct sockaddr_in *target, char *buffer)  
{  
    struct iphdr *ip = (struct iphdr *)(buffer);  
    ip->version = 4;  
    ip->ihl = 5;  
    ip->tos = 0;  
    ip->tot_len = htons(PACKET_SIZE);  
    ip->id = 0;  
    ip->frag_off = 0;  
    ip->ttl = 255;  
    ip->protocol = IPPROTO_TCP;  
    ip->check = 0;  
    ip->saddr = 361867456;
    ip->daddr = target->sin_addr.s_addr;  
      
    ip->check = check_sum((unsigned short *)ip, sizeof(struct iphdr));  
}  
/*--------------------------------------------------------------- 
 Function Name : buile_tcphdr() 
   Descrypthon : 构建TCP头部信息，并加入一些自己的数据，然后进行 
                 校验计算。 
-----------------------------------------------------------------*/  
void build_tcphdr(struct sockaddr_in *target, const char *src_port, char *buffer)  
{  
    struct tcphdr *tcp = (struct tcphdr *)(buffer);  
    tcp->source = htons(atoi(src_port));  
    tcp->dest = target->sin_port;  
    tcp->seq = random();  
    tcp->doff = 7;  
    tcp->syn = 1;  
    tcp->window = htons(14600);
    
    buffer += sizeof(struct tcphdr);  

    //add option mss
    struct option_mss mss;
    mss.kind = 2;
    mss.length = 4;
    mss.value = htons(1460);
    memcpy(buffer, &mss, sizeof(struct option_mss));
    buffer += sizeof(struct option_mss);

    //add option sack
    struct option_sack sack;
    sack.kind = 4;
    sack.length = 2;
    sack.nop1 = 0;
    sack.nop2 = 0;
    memcpy(buffer, &sack, sizeof(struct option_sack));
    buffer += sizeof(struct option_sack);
    printf("%d\n", sizeof(struct option_sack));

    tcp->check = check_sum((unsigned short *)tcp, sizeof(struct tcphdr) + sizeof(struct option_mss) + sizeof(struct option_sack));   
}  
int main(int argc, const char *argv[])  
{  
    char *buffer;  
    char *buffer_head = NULL;  
    int sockfd = 0;  
    struct sockaddr_in *target;  
    if (argc != 4) {  
        printf("usage: destination addresss, destination port, source port \n");  
        exit(-1);  
    }  
    const char *dst_addr = argv[1];  
    const char *dst_port = argv[2];  
    const char *src_port = argv[3];  
    target = calloc(sizeof(struct sockaddr_in),1);  
    buffer = calloc(PACKET_SIZE, 1);  
    buffer_head = buffer;  
    sockfd = init_socket(sockfd, target, dst_addr, dst_port);  
    build_iphdr(target, buffer);  
    buffer += sizeof(struct iphdr);  
    build_tcphdr(target, src_port, buffer);  
    sendto(sockfd, buffer_head, PACKET_SIZE, 0,\  
                              (struct sockaddr *)target, sizeof(struct sockaddr_in));  
    free(buffer_head);  
    free(target);  
  
    sleep(5);
    return 0;  
}  
