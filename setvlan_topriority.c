#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <linux/if_vlan.h>
#include <linux/sockios.h>

/*
 * Create VLAN subinterface with 
 *	ip link add link IF name SUBIF type lan id ID
 */

/* 
 * originally inspired by https://stackoverflow.com/questions/19254834/setting-up-vlan-on-c-sockets-receiving-it-on-other-end 
 * but informed by a very thorough analysis at https://www.rationali.st/blog/looking-into-dscp-and-ieee-8021p-vlan-priorities.html
 */

int main(int argc, char **argv) {

	char * iface;
	char buf[255];
	int  prio = 0;
	int  qos = 0;
	int  ret = 0;
	struct vlan_ioctl_args vlan_args;

	if (argc != 4) {
		fprintf(stderr, "Usage: %s <root eth iface> <vlan PCP> <linux sk_priority>\n",argv[0]);
		exit(7);
	}

	iface = argv[1];
	qos   = atoi(argv[2]);
	prio  = atoi(argv[3]);

	fprintf(stderr, "On %s, mapping linux sk_priority %i to VLAN PCP %i\n", iface, prio, qos);

	int sock_fd = socket (AF_INET, SOCK_DGRAM, 0);
	if (sock_fd < 0) {
		perror("socket(AF_INET, SOCK_DGRAM, 0)");
		exit(3);
	}

	// see https://github.com/torvalds/linux/blob/master/net/8021q/vlan.c
	vlan_args.cmd = SET_VLAN_EGRESS_PRIORITY_CMD;
	vlan_args.u.skb_priority = prio;
	vlan_args.vlan_qos = qos;
	strcpy(vlan_args.device1, iface);

	ret = ioctl (sock_fd, SIOCSIFVLAN, &vlan_args);
	if (ret < 0) {
		snprintf(buf, 255, "ioctl(SIOCSIFVLAN, device=\"%s\")", iface);
		perror(buf);
		exit(2);
	}

	close(sock_fd);
	exit(0);
	return(0);
}
