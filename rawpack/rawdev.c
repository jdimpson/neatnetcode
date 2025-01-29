#include <stdlib.h> //exit
#include <string.h> //strncpy
#include <stdio.h>  //printf
#include <unistd.h> //setuid

#include <sys/types.h>  //socket,bind,recvfrom
#include <sys/socket.h> //socket,bind,recvfrom,PF_PACKET

#include <netpacket/packet.h> //PF_PACKET,sockaddr_ll
#include <net/ethernet.h>  //PF_PACKET,ETH_P_ALL

#include <sys/ioctl.h> //ioctl
#include <net/if.h> //SIOCGIF*, see netdevice

#include <arpa/inet.h> //htons

#include <sys/time.h> //gettimeofday

#include "rawdev.h"

#define ETHLEN 1524

typedef unsigned char u8_t;
u8_t uip_buf[ETHLEN];
int uip_len = ETHLEN;
int s = -1;
struct ifreq ifr;
struct sockaddr_ll sll;
struct packet_mreq pmr;

main () {

	struct timeval a;
	struct timeval b;
	char ethdev[] = "eth0";
	char * eth = ethdev;
	int t;
	uid_t uid = 0;
	gid_t gid = 0;
	rawdev_init(eth,uid,gid);
	gettimeofday(&a,NULL);
	while (1) {
		t += rawdev_read();
		gettimeofday(&b,NULL);
		/* ioctl SIOCGSTAMP can be used to receive the 
		 * time stamp of the last received packet. Argument 
		 * is a struct timeval. */
		if ((b.tv_sec - a.tv_sec > 10)) { // || (t > 65000)) {
			fprintf(stderr,"rate %li bps\n", (t*8)/(b.tv_sec - a.tv_sec));
			t = 0;
			gettimeofday(&a,NULL);
		}
	}
}

void
rawdev_init(char * devname, uid_t uid, gid_t gid)
{

	int r;

	strncpy(ifr.ifr_name,devname,IFNAMSIZ);
	s = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (s < 0) {
		perror("socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))");
		exit(1);
	}

	r = ioctl(s, SIOCGIFINDEX, &ifr);
	if (r < 0) {
		perror("ioctl(s, SIOCGIFINDEX, &ifr)");
		exit(2);
	}
	int ifindx = ifr.ifr_ifindex;

	r = ioctl(s, SIOCGIFHWADDR, &ifr);
	if (r < 0) {
		perror("ioctl(s, SIOCGIFHWADDR, &ifr)");
		exit(3);
	}
	fprintf(stderr, "if %s hwaddr: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", devname,
		(unsigned char)ifr.ifr_hwaddr.sa_data[0],
		(unsigned char)ifr.ifr_hwaddr.sa_data[1],
		(unsigned char)ifr.ifr_hwaddr.sa_data[2],
		(unsigned char)ifr.ifr_hwaddr.sa_data[3],
		(unsigned char)ifr.ifr_hwaddr.sa_data[4],
		(unsigned char)ifr.ifr_hwaddr.sa_data[5]);

	sll.sll_family=AF_PACKET;
	sll.sll_protocol=htons(ETH_P_ALL);
	sll.sll_ifindex=ifindx;
	sll.sll_pkttype=PACKET_HOST; // iptraf uses PACKET_OUTGOING, see packet(7) man page
	r = bind(s, (struct sockaddr *)&sll,sizeof(struct sockaddr_ll));
	if (r < 0) {
		perror("bind()");
		exit(4);
	}

	struct packet_mreq pmr;
	pmr.mr_ifindex=ifindx;
	pmr.mr_type=PACKET_MR_PROMISC;
	pmr.mr_alen=0;
	pmr.mr_address[0]='\0';
	r = setsockopt(s, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &pmr, sizeof(struct packet_mreq));
	if (r < 0) {
		perror("setsockopts(PROMISC)");
		exit(4);
	}

	if (gid) {
		if(setgid(gid) < 0) {
			perror("setgid()");
			exit(5);
		}
	}
	if (uid) {
		if(setuid(uid) < 0) {
			perror("setuid()");
			exit(5);
		}
	}
}

unsigned int
rawdev_read(void)
{

	fd_set fdset;
	struct timeval tv;
	struct sockaddr from;
	socklen_t fromlen = sizeof(struct sockaddr);

	tv.tv_sec = 0;
	tv.tv_usec = 1000;

	FD_ZERO(&fdset);
	FD_SET(s, &fdset);

	int ret = select(s + 1, &fdset, NULL, NULL, &tv);
	if(ret == 0) {
		return 0;
	}
	ssize_t o = recvfrom(s, (void *)uip_buf, ETHLEN,
		MSG_TRUNC, &from, &fromlen);
	
	//fprintf(stderr,"--- rawdev: rawdev_read: read %i bytes\n", o);
	int i=0;
	while (i < o && i < ETHLEN-8) {
		fprintf(stdout,"%08x %02x ", i, uip_buf[i]); i++;
		fprintf(stdout,"%02x ", uip_buf[i]); i++;
		fprintf(stdout,"%02x ", uip_buf[i]); i++;
		fprintf(stdout,"%02x ", uip_buf[i]); i++;
		fprintf(stdout,"%02x ", uip_buf[i]); i++;
		fprintf(stdout,"%02x ", uip_buf[i]); i++;
		fprintf(stdout,"%02x ", uip_buf[i]); i++;
		fprintf(stdout,"%02x ", uip_buf[i]); i++;
		fprintf(stdout,"........\n");
	}
	/*  check_checksum(uip_buf, o);*/
	return o;
}

void
rawdev_send(void)
{

	//fprintf(stderr,"--- rawdev: rawdev_send: sending %d bytes\n", uip_len);
	// args 4 and 5 can be NULL/0 because of bind() call in rawdev_init(?)
	ssize_t p = sendto(s, (void*)uip_buf, uip_len, 0, NULL, 0);
	if (p<0) {
		perror("rawdev: rawdev_send: sendto()");
		exit(5);
	}
}


