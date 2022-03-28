// gcc ./src/c/01-icmp/main.c && sudo ./a.out
#include <stdio.h>            // for perror(3), printf(3)
#include <stdlib.h>           // for exit(3), EXIT_FAILURE
#include <string.h>           // for strcmp(3)
#include <unistd.h>           // for close(2)
#include <sys/types.h>        // for u_int16_t
#include <sys/socket.h>       // for socket(2)
#include <arpa/inet.h>        // for inet_addr(3), inet_ntoa(3)
#include <netinet/ip_icmp.h>  // for icmphdr

// 按照 16 位为单位进行反码求和，进位需加回最低位。
u_int16_t checksum(unsigned short *buf, int size)
{
    unsigned long sum = 0;
    while (size > 1)
    {
        sum += *buf;
        buf++;
        size -= 2;
    }
    if (size == 1)
        sum += *(unsigned char *)buf;
    sum = (sum & 0xffff) + (sum >> 16);
    sum = (sum & 0xffff) + (sum >> 16);
    return ~sum;
}

//  创建 protocol 的 raw socket
int make_raw_socket(int protocol)
{
    int s = socket(AF_INET, SOCK_RAW, protocol);
    if (s < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    return s;
}

//  构造 ICMP echo 消息的 Header
void setup_icmp_echo_hdr(u_int16_t id, u_int16_t seq, struct icmphdr *icmphdr)
{
    memset(icmphdr, 0, sizeof(struct icmphdr));
    icmphdr->type = ICMP_ECHO;
    icmphdr->code = 0;
    icmphdr->checksum = 0;
    icmphdr->un.echo.id = id;
    icmphdr->un.echo.sequence = seq;
    icmphdr->checksum = checksum((unsigned short *)icmphdr, sizeof(struct icmphdr));
}

int main(int argc, char **argv)
{
    int n, s;
    char buf[1500];
    struct sockaddr_in target_addr;
    struct in_addr recv_source_addr;
    struct icmphdr icmphdr;
    struct iphdr *recv_iphdr;
    struct icmphdr *recv_icmphdr;
    const char *target_addr_str = "127.0.0.1";

    target_addr.sin_family = AF_INET;
    target_addr.sin_addr.s_addr = inet_addr(target_addr_str);
    // 创建一个 ICMP 协议的 Raw Socket
    // 可以直接向该 socket 发送消息，发送的消息体只需要给 IP Data 部分的内容
    // 从该 socket 接收消息所有发给该主机的 IP 消息的一份拷贝，接收消息内容是整个 IP packet （包括 IP Header）的内容
    s = make_raw_socket(IPPROTO_ICMP);
    setup_icmp_echo_hdr(0, 0, &icmphdr);

    // 发送 ICMP echo 消息到 target_addr
    n = sendto(s, (char *)&icmphdr, sizeof(icmphdr), 0, (struct sockaddr *)&target_addr, sizeof(target_addr));
    if (n < 1)
    {
        perror("sendto");
        return 1;
    }

    // 接收 ICMP 消息，因为上面代码发送到了 127.0.0.1 所以：
    // 第 1 个消息是上面代码发送的 echo 消息
    // 第 2 个消息是内核回复的 echo reply 消息
    // 如果 target 是其他主机，则只会收到第 2 个消息
    for (int i = 0; i < 2; i++) {
        // 整个 IP packet 将填充到 buf 里
        n = recv(s, buf, sizeof(buf), 0);
        if (n < 1)
        {
            perror("recv");
            return 1;
        }
        // 转换为 IP Header 类型
        recv_iphdr = (struct iphdr *)buf;
        // 根据 ihl 协议头长度获取 IP Data，即 ICMP Header
        recv_icmphdr = (struct icmphdr *)(buf + (recv_iphdr->ihl << 2));
        recv_source_addr.s_addr = recv_iphdr->saddr;
        // 检查回复的消息的 Source IP 和 发送消息的 Target IP 是否一样 且 消息类型需要是 ICMP Echo Reply
        if (!strcmp(target_addr_str, inet_ntoa(recv_source_addr)) && recv_icmphdr->type == ICMP_ECHOREPLY)
            printf("icmp echo reply from %s\n", target_addr_str);
    }
    close(s);
    return 0;
}