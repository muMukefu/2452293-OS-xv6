#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

#define MAX_PACKETS_PER_PORT 16

// UDP 包队列项
struct udp_packet {
  char* buf;              // 完整的以太网帧
  int len;                // 总长度
  uint32 src_ip;          // 源 IP 地址（主机字节序）
  uint16 src_port;        // 源端口（主机字节序）
  uint16 dst_port;        // 目标端口（主机字节序）
  int payload_len;        // UDP 负载长度
  char* payload;          // UDP 负载的起始位置
  struct udp_packet* next;
};

// 绑定的端口
struct bound_port {
  uint16 port;                    // 端口号
  int in_use;                     // 是否在使用
  struct udp_packet* head;        // 队列头
  struct udp_packet* tail;        // 队列尾
  int packet_count;               // 当前队列中的包数
  struct spinlock lock;           // 每个端口的锁
};

#define MAX_BOUND_PORTS 16
static struct bound_port bound_ports[MAX_BOUND_PORTS];

// xv6's ethernet and IP addresses
static uint8 local_mac[ETHADDR_LEN] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint32 local_ip = MAKE_IP_ADDR(10, 0, 2, 15);

// qemu host's ethernet address.
static uint8 host_mac[ETHADDR_LEN] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };

static struct spinlock netlock;

// 查找已绑定的端口，返回索引，如果不存在返回 -1
static int
find_bound_port(uint16 port)
{
  for (int i = 0; i < MAX_BOUND_PORTS; i++) {
    if (bound_ports[i].in_use && bound_ports[i].port == port) {
      return i;
    }
  }
  return -1;
}

// 分配一个新的绑定端口
static int
alloc_bound_port(uint16 port)
{
  for (int i = 0; i < MAX_BOUND_PORTS; i++) {
    if (!bound_ports[i].in_use) {
      bound_ports[i].port = port;
      bound_ports[i].in_use = 1;
      bound_ports[i].head = 0;
      bound_ports[i].tail = 0;
      bound_ports[i].packet_count = 0;
      initlock(&bound_ports[i].lock, "portlock");
      return i;
    }
  }
  return -1;
}

void
netinit(void)
{
  initlock(&netlock, "netlock");
}


//
// bind(int port)
// prepare to receive UDP packets address to the port,
// i.e. allocate any queues &c needed.
//
uint64
sys_bind(void)
{
  //
  // Your code here.
  //
  int port;
  argint(0, &port);

  if (port < 0 || port > 65535) {
    return -1;
  }

  acquire(&netlock);

  // 检查端口是否已经被绑定
  if (find_bound_port((uint16)port) >= 0) {
    release(&netlock);
    return -1;
  }

  // 分配新端口
  if (alloc_bound_port((uint16)port) < 0) {
    release(&netlock);
    return -1;
  }

  release(&netlock);
  return 0;
}

//
// unbind(int port)
// release any resources previously created by bind(port);
// from now on UDP packets addressed to port should be dropped.
//
uint64
sys_unbind(void)
{
  //
  // Optional: Your code here.
  //

  return 0;
}

//
// recv(int dport, int *src, short *sport, char *buf, int maxlen)
// if there's a received UDP packet already queued that was
// addressed to dport, then return it.
// otherwise wait for such a packet.
//
// sets *src to the IP source address.
// sets *sport to the UDP source port.
// copies up to maxlen bytes of UDP payload to buf.
// returns the number of bytes copied,
// and -1 if there was an error.
//
// dport, *src, and *sport are host byte order.
// bind(dport) must previously have been called.
//
uint64
sys_recv(void)
{
  int dport;
  uint64 src_addr, sport_addr, buf_addr;
  int maxlen;

  argint(0, &dport);
  argaddr(1, &src_addr);
  argaddr(2, &sport_addr);
  argaddr(3, &buf_addr);
  argint(4, &maxlen);

  if (dport < 0 || dport > 65535 || maxlen < 0) {
    return -1;
  }

  struct proc* p = myproc();

  acquire(&netlock);

  int idx = find_bound_port((uint16)dport);
  if (idx < 0) {
    release(&netlock);
    return -1;
  }

  struct bound_port* bp = &bound_ports[idx];

  // 等待直到有数据包
  while (bp->packet_count == 0) {
    // 使用 &bp->lock 作为 channel（和之前一样）
    sleep(&bp->lock, &netlock);
  }

  // 取出队列头部包
  acquire(&bp->lock);
  struct udp_packet* pkt = bp->head;
  bp->head = pkt->next;
  if (bp->head == 0) {
    bp->tail = 0;
  }
  bp->packet_count--;
  release(&bp->lock);
  release(&netlock);

  // 拷贝数据到用户空间
  int copy_len = pkt->payload_len;
  if (copy_len > maxlen) {
    copy_len = maxlen;
  }

  // 拷贝源 IP 地址和源端口
  if (copyout(p->pagetable, src_addr, (char*)&pkt->src_ip, sizeof(uint32)) < 0) {
    kfree(pkt->buf);
    kfree(pkt);
    return -1;
  }

  if (copyout(p->pagetable, sport_addr, (char*)&pkt->src_port, sizeof(uint16)) < 0) {
    kfree(pkt->buf);
    kfree(pkt);
    return -1;
  }

  // 拷贝 UDP 负载
  if (copyout(p->pagetable, buf_addr, pkt->payload, copy_len) < 0) {
    kfree(pkt->buf);
    kfree(pkt);
    return -1;
  }

  // 释放包内存
  kfree(pkt->buf);
  kfree(pkt);

  return copy_len;
}

// This code is lifted from FreeBSD's ping.c, and is copyright by the Regents
// of the University of California.
static unsigned short
in_cksum(const unsigned char *addr, int len)
{
  int nleft = len;
  const unsigned short *w = (const unsigned short *)addr;
  unsigned int sum = 0;
  unsigned short answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the
   * carry bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1)  {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(unsigned char *)(&answer) = *(const unsigned char *)w;
    sum += answer;
  }

  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);
  /* guaranteed now that the lower 16 bits of sum are correct */

  answer = ~sum; /* truncate to 16 bits */
  return answer;
}

//
// send(int sport, int dst, int dport, char *buf, int len)
//
uint64
sys_send(void)
{
  struct proc *p = myproc();
  int sport;
  int dst;
  int dport;
  uint64 bufaddr;
  int len;

  argint(0, &sport);
  argint(1, &dst);
  argint(2, &dport);
  argaddr(3, &bufaddr);
  argint(4, &len);

  int total = len + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  if(total > PGSIZE)
    return -1;

  char *buf = kalloc();
  if(buf == 0){
    printf("sys_send: kalloc failed\n");
    return -1;
  }
  memset(buf, 0, PGSIZE);

  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, host_mac, ETHADDR_LEN);
  memmove(eth->shost, local_mac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);

  struct ip *ip = (struct ip *)(eth + 1);
  ip->ip_vhl = 0x45; // version 4, header length 4*5
  ip->ip_tos = 0;
  ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udp) + len);
  ip->ip_id = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 100;
  ip->ip_p = IPPROTO_UDP;
  ip->ip_src = htonl(local_ip);
  ip->ip_dst = htonl(dst);
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(*ip));

  struct udp *udp = (struct udp *)(ip + 1);
  udp->sport = htons(sport);
  udp->dport = htons(dport);
  udp->ulen = htons(len + sizeof(struct udp));

  char *payload = (char *)(udp + 1);
  if(copyin(p->pagetable, payload, bufaddr, len) < 0){
    kfree(buf);
    printf("send: copyin failed\n");
    return -1;
  }

  e1000_transmit(buf, total);

  return 0;
}

void
ip_rx(char *buf, int len)
{
  // don't delete this printf; make grade depends on it.
  static int seen_ip = 0;
  if(seen_ip == 0)
    printf("ip_rx: received an IP packet\n");
  seen_ip = 1;

  //
  // Your code here.
  //
   // 1. 检查长度是否足够
  if (len < sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp)) {
    kfree(buf);
    return;
  }

  // 2. 获取 IP 头和 UDP 头
  struct ip* ip = (struct ip*)(buf + sizeof(struct eth));
  struct udp* udp = (struct udp*)(buf + sizeof(struct eth) + sizeof(struct ip));

  // 3. 检查是否是 UDP 协议
  if (ip->ip_p != IPPROTO_UDP) {
    kfree(buf);
    return;
  }

  // 4. 获取目标端口（网络字节序转主机字节序）
  uint16 dport = ntohs(udp->dport);
  uint16 sport = ntohs(udp->sport);
  uint32 src_ip = ntohl(ip->ip_src);
  uint16 udp_len = ntohs(udp->ulen);
  int payload_len = udp_len - sizeof(struct udp);

  // 5. 检查 UDP 长度是否合法
  if (udp_len < sizeof(struct udp) || payload_len < 0) {
    kfree(buf);
    return;
  }

  // 6. 查找绑定的端口
  acquire(&netlock);
  int idx = find_bound_port(dport);
  if (idx < 0) {
    release(&netlock);
    kfree(buf);
    return;
  }

  struct bound_port* bp = &bound_ports[idx];

  // 7. 检查队列是否已满
  acquire(&bp->lock);
  if (bp->packet_count >= MAX_PACKETS_PER_PORT) {
    release(&bp->lock);
    release(&netlock);
    kfree(buf);
    return;
  }

  // 8. 创建包队列项
  struct udp_packet* pkt = (struct udp_packet*)kalloc();
  if (pkt == 0) {
    release(&bp->lock);
    release(&netlock);
    kfree(buf);
    return;
  }

  pkt->buf = buf;
  pkt->len = len;
  pkt->src_ip = src_ip;
  pkt->src_port = sport;
  pkt->dst_port = dport;
  pkt->payload_len = payload_len;
  pkt->payload = (char*)(udp + 1);  // UDP 负载起始位置
  pkt->next = 0;

  // 9. 加入队列尾部
  if (bp->head == 0) {
    bp->head = pkt;
    bp->tail = pkt;
  }
  else {
    bp->tail->next = pkt;
    bp->tail = pkt;
  }
  bp->packet_count++;

  release(&bp->lock);
  release(&netlock);

  // 10. 唤醒等待该端口的进程
  wakeup(&bp->lock);
}

//
// send an ARP reply packet to tell qemu to map
// xv6's ip address to its ethernet address.
// this is the bare minimum needed to persuade
// qemu to send IP packets to xv6; the real ARP
// protocol is more complex.
//
void
arp_rx(char *inbuf)
{
  static int seen_arp = 0;

  if(seen_arp){
    kfree(inbuf);
    return;
  }
  printf("arp_rx: received an ARP packet\n");
  seen_arp = 1;

  struct eth *ineth = (struct eth *) inbuf;
  struct arp *inarp = (struct arp *) (ineth + 1);

  char *buf = kalloc();
  if(buf == 0)
    panic("send_arp_reply");
  
  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, ineth->shost, ETHADDR_LEN); // ethernet destination = query source
  memmove(eth->shost, local_mac, ETHADDR_LEN); // ethernet source = xv6's ethernet address
  eth->type = htons(ETHTYPE_ARP);

  struct arp *arp = (struct arp *)(eth + 1);
  arp->hrd = htons(ARP_HRD_ETHER);
  arp->pro = htons(ETHTYPE_IP);
  arp->hln = ETHADDR_LEN;
  arp->pln = sizeof(uint32);
  arp->op = htons(ARP_OP_REPLY);

  memmove(arp->sha, local_mac, ETHADDR_LEN);
  arp->sip = htonl(local_ip);
  memmove(arp->tha, ineth->shost, ETHADDR_LEN);
  arp->tip = inarp->sip;

  e1000_transmit(buf, sizeof(*eth) + sizeof(*arp));

  kfree(inbuf);
}

void
net_rx(char *buf, int len)
{
  struct eth *eth = (struct eth *) buf;

  if(len >= sizeof(struct eth) + sizeof(struct arp) &&
     ntohs(eth->type) == ETHTYPE_ARP){
    arp_rx(buf);
  } else if(len >= sizeof(struct eth) + sizeof(struct ip) &&
     ntohs(eth->type) == ETHTYPE_IP){
    ip_rx(buf, len);
  } else {
    kfree(buf);
  }
}
