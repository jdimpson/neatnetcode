#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
 * This code sets directly sets the Linux sk_priority value for frames associated with a broadcast socket
 * This code sends to INADDR_BROADCAST, then binds the socket to an ethernet device with SO_BINDTODEVICE
 * This results in a frame exiting the ethernet device destinted to 255.255.255.255. This is primarily so I
 * don't have to write code to figure out the subnet broadcast address. But it won't work dependably on multi-
 * homed interfaces (depending on which network/source address you want to use). 
 *
 *
 * note that if you configure a subinterface, ensure it's in state "up", but do not assign an IP address,
 * it seems to use the parent interface's IP address, but still attaches the VLAN tag. Unlikely to work
 * the same on receive.
 */

int main(int argc, char **argv) {

	int bcast_port = 0;

	char * iface;
	char * message = NULL;
	size_t size = 0;
	char buf[255];
	int prio = 0;
	int ret = 0;
	struct sockaddr_in s;

	if (argc !=  4) {
		fprintf(stderr, "Usage: %s <interface or subiface> <dest port> <linux sk_priority>\n",argv[0]);
		fprintf(stderr, "	reads payload string from standard input\n");
		exit(7);
	}

	iface = argv[1];
	bcast_port = atoi(argv[2]);
	prio = atoi(argv[3]);

	fprintf(stderr, "Reading standard input, sending broadcast on interface %s, port %i, at linux sk_priority %i\n", iface, bcast_port, prio);

	int sock_fd = socket (AF_INET, SOCK_DGRAM, 0);
	if (sock_fd < 0) {
		perror("socket(AF_INET, SOCK_DGRAM, 0)");
		exit(3);
	}

	/* make it a broadcast */
	int bcast = 1;
	ret =  setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast));
	if (ret < 0) {
		perror("setsockopt(SO_BROADCAST)");
		exit(4);
	}

	/* this sets the internal linux sk_priority which must be mapped to VLAN PCP */
	ret = setsockopt(sock_fd, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio));
	if (ret < 0) {
		perror("setsockopt(SO_PRIORITY)");
		exit(1);
	}

	ret = setsockopt(sock_fd, SOL_SOCKET, SO_BINDTODEVICE, iface, strlen(iface)); 
	if (ret < 0) {
		snprintf(buf, 255, "setsockopt(SO_BINDTODEVICE, %s) failed",iface);
		perror(buf);
		exit(6);
	}

	memset(&s, '\0', sizeof(s));
	s.sin_family = AF_INET;
	s.sin_port = htons(bcast_port);
	s.sin_addr.s_addr = INADDR_BROADCAST;

	/* caution: getline dynamically assigns memory */
	message = NULL;
	ret = getline(&message, &size, stdin);
	if (ret < 0) {
		fprintf(stderr,"Failed to read anything from standard input");
		free(message);
		exit(7);
	}
	fprintf(stderr,"%i bytes allocated, line is %i bytes: %s",size, strlen(message), message);

	ret = sendto(sock_fd, message, strlen(message), 0, (const struct sockaddr *)&s, sizeof(s));
	if (ret < 0) {
		perror("sendto() failed");
		exit(5);
	}
	fprintf(stderr,"%i\n",ret);

	free(message);
	close(sock_fd);
	exit(0);
	return(0);
}
