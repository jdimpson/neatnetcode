/*
 * Copyright 2008 Jeremy D. Impson <jdimpson@acm.org>
 * Reference code for doing raw ethernet packet capture with promiscuous mode
 */

#include <stdlib.h> //exit
#include <string.h> //strncpy
#include <stdio.h> //printf

#include <sys/types.h>  //socket,bind,recvfrom
#include <sys/socket.h> //socket,bind,recvfrom,PF_PACKET

#include <netpacket/packet.h> //PF_PACKET,sockaddr_ll
#include <net/ethernet.h>  //PF_PACKET,ETH_P_ALL

#include <sys/ioctl.h> //ioctl
#include <net/if.h> //SIOCGIF*, see netdevice

#define ETH0 "eth0"

int main (int argc, char * argv[]) {
	struct ifreq ifr;
	if ( argc > 1 ) {
		strncpy(ifr.ifr_name,argv[1],IFNAMSIZ);
	} else {
		strncpy(ifr.ifr_name,ETH0,IFNAMSIZ);
	}
	fprintf(stderr, "dev             : %s\n", ifr.ifr_name);
	int s = -1;
	/* Put socket into raw mode, i.e. read everything that the Ethernet card passes up */
	s = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (s < 0) {
		perror("socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))");
		exit(1);
	}
	fprintf(stderr, "socket fd       : %i\n", s);

	int r = ioctl(s, SIOCGIFINDEX, &ifr);
	if (r < 0) {
		perror("ioctl(s, SIOCGIFINDEX, &ifr)");
		exit(2);
	}
	int ifindx = ifr.ifr_ifindex;
	fprintf(stderr, "socket if index : %i\n", ifindx);
	r = -1;

	r = ioctl(s, SIOCGIFHWADDR, &ifr);
	if (r < 0) {
		perror("ioctl(s, SIOCGIFHWADDR, &ifr)");
		exit(3);
	}
	fprintf(stderr, "socket if hwaddr: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
		(unsigned char)ifr.ifr_hwaddr.sa_data[0],
		(unsigned char)ifr.ifr_hwaddr.sa_data[1],
		(unsigned char)ifr.ifr_hwaddr.sa_data[2],
		(unsigned char)ifr.ifr_hwaddr.sa_data[3],
		(unsigned char)ifr.ifr_hwaddr.sa_data[4],
		(unsigned char)ifr.ifr_hwaddr.sa_data[5]);

	r = -1;
	struct sockaddr_ll sll;
	sll.sll_family=AF_PACKET;
	sll.sll_protocol=htons(ETH_P_ALL);
	sll.sll_ifindex=ifindx;
	sll.sll_pkttype=PACKET_HOST;
	r = bind(s, (struct sockaddr *)&sll,sizeof(struct sockaddr_ll));
	if (r < 0) {
		perror("bind()");
		exit(4);
	}

	r = -1;
	struct packet_mreq pmr;
	pmr.mr_ifindex=ifindx;
	pmr.mr_type=PACKET_MR_PROMISC; /* to set up multicast, use PACKET_MR_MULTICAST then set mr_address and mr_alen, or use PACKET_MR_ALLMULTI to get all multicast */
	pmr.mr_alen=0;
	pmr.mr_address[0]='\0';
	/* put socket into promiscuous mode, i.e. cause Ethernet to send up everything it sees, even if not addressed */
	r = setsockopt(s, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &pmr, sizeof(struct packet_mreq));
	if (r < 0) {
		perror("setsockopts(PROMISC)");
		exit(4);
	}
	/* alternate way to go promisc is ioctl(s, SIOCSIFFLAGS, {ifr_name="eth0", ifr_flags=IFF_UP|IFF_BROADCAST|IFF_RUNNING|IFF_PROMISC|IFF_MULTICAST}) = 0 */

	unsigned char arp[] = {0xff,0xff,0xff,0xff,0xff,0xff,0x00,0x30,0x1b,0xa0,0x6e,0xe6,0x08,0x06,0x00,0x01,0x08,0x00,0x06,0x04,0x00,0x01,0x00,0x30,0x1b,0xa0,0x6e,0xe6,0x0a,0x00,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x0a,0x00,0x00,0x03};

	// args 4 and 5 can be NULL/0 because of bind() call above(?)
	ssize_t p = sendto(s, (void*)&arp, sizeof(arp), 0, NULL, 0);
	if (p<0) {
		perror("sendto()");
		exit(5);
	}


	size_t snaplen = 65535;
	unsigned char buf[snaplen];
	struct sockaddr from;
	ssize_t fromlen = sizeof(struct sockaddr);
	ssize_t sizeofsockaddr = sizeof(struct sockaddr);

	while(1) {
		size_t fromlen = sizeofsockaddr;
		ssize_t o = recvfrom(s, (void *)&buf, snaplen, 
			MSG_TRUNC, &from, &fromlen);
	
		{ // this format can be read by text2pcap
			//printf("--- recvfrom: %i\n", o); //although this messes text2pcap up
			int i=0;
			while (i < o) {
				printf("%08x %02x ", i, buf[i]); i++;
				printf("%02x ", buf[i]); i++;
				printf("%02x ", buf[i]); i++;
				printf("%02x ", buf[i]); i++;
				printf("%02x ", buf[i]); i++;
				printf("%02x ", buf[i]); i++;
				printf("%02x ", buf[i]); i++;
				printf("%02x ", buf[i]); i++;
				printf("........\n");
			}
		}
	}
}
