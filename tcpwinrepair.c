#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/******
 This is sample code for usint TCP_REPAIR_WINOW socket option to peak at the send and receive TCP window sizes of a socket.

 TCP_REPAIR_WINDOW can not be used to peak at the state of a socket's TCP Send and Receive windows 
 without the socket first being put into TCP_REPAIR mode. But then if you do anything to the socket with respect 
 to TCP, the TCP protocol engine doesn't get involved, so you need to take it out of TCP_REPAIR mode after getting the
 window states. I wish getsockopt(TCP_REPAIR_WINDOW) didn't require that.

 Can't find a lot of documentation on TCP_REPAIR_WINDOW socket option. Can only find a bit about TCP_REPAIR socket option.

 https://criu.org/TCP_connection talks about TCP_REPAIR, as does https://lwn.net/Articles/495304/ . The former link suggests you can't 
    leave TCP_REPAIR turned on for a socket that you intend to keep using, because it actually shuts off the TCP protocol.

 https://stackoverflow.com/questions/54070889/how-to-get-the-tcp-window-size-of-a-socket-in-linux/54071393#54071393 suggests using 
    TCP_REPAIR_WINDOW to get the window sizes, but doesn't mention needing to turn on TCP_REPAIR first (except in a comment I just left).

 https://github.com/YutaroHayakawa/libtcprepair is a little standalone library that does TCP socket freeze/thaw aka checkpoint/restore. 
    This doesn't help us easily get the window sizes, but I just think it's neat.

 The stack overflow link talks about a TCP_INFO socket option. I should probably make a version of this code that uses that instead.

 Note that to go into TCP_REPAIR mode, you need to run as root, under sudo, or (preferably) with the NET_ADMIN capability set.

 Compile with
   gcc -o tcpwinrepair tcpwinrepait.c
 Set capability with
   sudo setcap cap_net_admin=+ep tcpwinrepair
 Run with
   ./tcpwinrepair -l 2000
 as server side, and
   ./tcpwinrepait localhost 2000
 as client side. Use any port you want, and run them on localhost or across a real network as you please.

 ******/

int noreadserver(int sockfd, int port);
int client(int sockfd, int port, char * server);
#define REPAIR_ON 1
#define REPAIR_OFF 0
int repair(int sockfd, int st);

int main(int argc, char ** argv) {
	struct tcp_repair_window trw;
	socklen_t trwsize = sizeof(trw);


	int sockfd, port;
	char * server;

	if (argc <= 2) {
		fprintf(stderr, "Usage: %s <server iP or hostname> <port> # for client\n", argv[0]);
		fprintf(stderr, "    or %s -l <port> # for non-reading server\n", argv[0]);
		exit(1);
	}


	server = argv[1];
	port   = atoi(argv[2]);

	if ( port <= 0 ) {
		fprintf(stderr,"invalid port given %s\n",argv[2]);
		exit(2);
	}

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr,"fail to create socket\n");
		exit(3);
	}

	repair(sockfd,REPAIR_ON);
	// from https://stackoverflow.com/a/54071393
	int r = getsockopt(sockfd, IPPROTO_TCP, TCP_REPAIR_WINDOW, &trw, &trwsize);
	repair(sockfd,REPAIR_OFF);

	if ( r < 0 ) perror("getsockopts TCP_REPAIR_WINDOW");
	printf("preconnect: sndwnd %i rcvwnd %i return %i\n",trw.snd_wnd,trw.rcv_wnd,r);

	if (strcmp("-l", server) == 0)
		noreadserver(sockfd, port);
	else
		client(sockfd, port, server);
}

int noreadserver(int lsock, int port) {
	char * listen_addr = "0.0.0.0";
	struct tcp_repair_window trw;
	socklen_t trwsize = sizeof(trw);
	int buflen = 1010;
	int cnt;
	char mesgbuf[buflen];

	struct sockaddr_in laddr;
	laddr.sin_family = AF_INET;
	laddr.sin_addr.s_addr = inet_addr(listen_addr);
	laddr.sin_port = htons((uint16_t) port);

	int r = bind(lsock, (struct sockaddr *)&laddr, sizeof(laddr));
	if (r<0) {
		perror("bind");
		exit(7);
	}
	listen(lsock, 1);

	printf("Listening on %s:%d ...\n", listen_addr, port);

	struct sockaddr_in caddr;
	socklen_t clen = sizeof(caddr);

	int csock = accept(lsock, (struct sockaddr *)&caddr, &clen);
	if (csock < 0) {
		perror("accept");
		exit(8);
	}

	/* ok, now, do nothing */
	struct timespec tim, rem;
	int donothing = 1;
	printf("Doing nothing\n");
	int last = 0;
	while(1) {
		repair(csock,REPAIR_ON);
		getsockopt(csock, IPPROTO_TCP, TCP_REPAIR_WINDOW, &trw, &trwsize);
		repair(csock,REPAIR_OFF);
		printf("interval: sndwnd %i rcvwnd %i\n",trw.snd_wnd,trw.rcv_wnd);
		if (donothing) {
			tim.tv_sec = 0;
			tim.tv_nsec = 500000000L;
			nanosleep(&tim, &rem);
			if (trw.rcv_wnd == 0) {
				donothing = 0;
				printf("OK, I'll do something now\n");
			}
		} else {
			int r = recv(csock, mesgbuf, buflen, 0);
			if (last >= 65536 && trw.rcv_wnd == last) { // if same twice in a row, we've worked off the backlog
				donothing = 1;
				printf("Caught up. Not gonna do anything anymore\n");
			}
			last = trw.rcv_wnd;
			if (r>0) {
				printf("%s\n", mesgbuf);
			} else {
				perror("recv");
			}
		}
	}
}

int client(int sockfd, int port, char * server) {
	struct tcp_repair_window trw;
	socklen_t trwsize = sizeof(trw);

	struct hostent *host;
	struct sockaddr_in srv_addr;
	struct timespec tim, rem;
	int buflen = 1010;
	int cnt;
	char mesgbuf[buflen];

	if ((host=gethostbyname(server)) == NULL) {  // get the host info 
		fprintf(stderr,"gethostbyname fail\n");
		exit(5);
	}

	printf("connecting to %s %i\n",server,port);

	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons(port);
	srv_addr.sin_addr = *((struct in_addr *)host->h_addr);
	memset(&(srv_addr.sin_zero), '\0', 8);

	if (connect(sockfd, (struct sockaddr *)&srv_addr, sizeof(struct sockaddr)) == -1) {
		fprintf(stderr,"connect fail\n");
		exit(6);
	}

	cnt=0;
	while(1) {
		int l = snprintf(mesgbuf, buflen,"MESSAGE %i ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ\r\n",cnt);
		cnt+=1;
		int s = send(sockfd, mesgbuf, l, 0);
		repair(sockfd,REPAIR_ON);
		getsockopt(sockfd, IPPROTO_TCP, TCP_REPAIR_WINDOW, &trw, &trwsize);
		repair(sockfd,REPAIR_OFF);
		printf("interval: sndwnd %i rcvwnd %i sentbytes %i\n",trw.snd_wnd,trw.rcv_wnd, s);
		tim.tv_sec = 0;
		tim.tv_nsec = 100000000L;
		nanosleep(&tim, &rem);
	}
}
int repair(int sockfd, int st) {
	int r = setsockopt(sockfd, IPPROTO_TCP, TCP_REPAIR, &st, sizeof( st ));
	if (r < 0 ) {
		char * state;
		if (st == REPAIR_ON)
			state = "setsockopts TCP_REPAIR on ";
		else
			state = "setsockopts TCP_REPAIR off";
		perror(state);
	}
	return r;
}

