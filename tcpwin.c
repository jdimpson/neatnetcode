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
 This is sample code for displaying the send and receive TCP window sizes of a socket. In client mode, it connect to the given port and sends ~1000 byte
 messages infinitely, while printing the send and receive TCP window sizes. In server mode, it waits for a connetion to the given port, then does 
 nothing except wait or the receive TCP window size to go to zero. When it does, it will start reading from the socket until the receive window size gets
 suitably large (according to a janky heurstic) then it will once again stop reading from the socket until the receive window size goes to zero, repeat
 ad infinitum.

 It can use either of two methods, but both of them only works on Linux. The two methods are the TCP_INFO socket option, and the TCP_REPAIR_WINDOW 
 socket option. There does not seem to be a difference in the results between the two methods, but the TCP_REPAIR_WINDOW socket option requires 
 NET_ADMIN capability (or run as root), while TCP_INFO does not. So that is the default and recommended method.

 Compile with
   gcc -o tcpwin tcpwin.c
 Run with
   ./tcpwin -l 2000
 as server side, and
   ./tcpwin localhost 2000
 as client side. Use any port you want, and run them on localhost or across a real network as you please.

 Use the -R flag to cause this software to use TCP_REPAIR_WINDOW instead of TCP_INFO. That requires elevanted privleges, so run under
 sudo, or better yet grant the program the NET_ADMIN capability (only need to do once):

   sudo setcap cap_net_admin=+ep tcpwin

 ******/

int noreadserver(int sockfd, int port);
int client(int sockfd, int port, char * server);
int gettcpwin_info(int sockfd, int * snd_wnd, int * rcv_wnd);
int gettcpwin_repair(int sockfd, int * snd_wnd, int * rcv_wnd);
void usage(char * cmdname);


int use_repair = 0;

int main(int argc, char ** argv) {
	int sockfd, port, porti;
	char * server = 0;
	int snd_wnd, rcv_wnd;
	int do_server = 0;

	if (argc <= 2) {
		usage(argv[0]);
		exit(1);
	}

	for (int i = 1; i < argc; i++) {
		if (strcmp("-R", argv[i]) == 0) {
			use_repair = 1;
		} else if (strcmp("-l", argv[i]) == 0) {
			do_server = 1;
		} else {
			if (do_server || server) {
				porti = i;
				port   = atoi(argv[i]);
			} else
				server = argv[i];
		}
	}

	if (!server && !do_server) {
		fprintf(stderr,"Need to provide either server name or -l flag\n");
		usage(argv[0]);
		exit(2);
	}

	if ( port <= 0 ) {
		fprintf(stderr,"invalid port given %s\n",argv[porti]);
		usage(argv[0]);
		exit(2);
	}

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr,"fail to create socket\n");
		exit(3);
	}

	int r;
	if (use_repair)
		r = gettcpwin_repair(sockfd, &snd_wnd, &rcv_wnd);
	else
		r = gettcpwin_info(sockfd, &snd_wnd, &rcv_wnd);
	printf("preconnect: sndwnd %i rcvwnd %i return %i\n",snd_wnd,rcv_wnd,r);

	if (do_server)
		noreadserver(sockfd, port);
	else
		client(sockfd, port, server);
}

void usage(char * cmdname) {
	fprintf(stderr, "Usage: %s [-R] <server iP or hostname> <port> # for client\n", cmdname);
	fprintf(stderr, "    or %s [-R] -l <port> # for non-reading server\n", cmdname);
}

int noreadserver(int lsock, int port) {
	char * listen_addr = "0.0.0.0";
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
	printf("Got a new connection from client %s.\n",inet_ntoa(caddr.sin_addr));

	/* ok, now, do nothing */
	struct timespec tim, rem;
	int donothing = 1;
	printf("Doing nothing\n");
	int last = 0;
	int snd_wnd, rcv_wnd;
	int red = 0;
	while(1) {
		if (use_repair)
			gettcpwin_repair(csock, &snd_wnd, &rcv_wnd);
		else
			gettcpwin_info(csock, &snd_wnd, &rcv_wnd);
		printf("receiving: sndwnd %i rcvwnd %i recvbytes %i (%s)\n",snd_wnd,rcv_wnd,red, (use_repair?"repair":"info"));
		if (donothing) {
			red = 0;
			tim.tv_sec = 0;
			tim.tv_nsec = 500000000L;
			nanosleep(&tim, &rem);
			if (rcv_wnd == 0) {
				donothing = 0;
				printf("OK, I'll do something now\n");
			}
		} else {
			red = recv(csock, mesgbuf, buflen, 0);
			//printf("%s\n", mesgbuf);
			if (last >= 65536 && rcv_wnd == last) { // if same twice in a row, we've worked off the backlog
				donothing = 1;
				printf("Caught up. Not gonna do anything anymore\n");
			}
			last = rcv_wnd;
		}
	}
}

int client(int sockfd, int port, char * server) {
	struct hostent *host;
	struct sockaddr_in srv_addr;
	struct timespec tim, rem;
	int buflen = 1010;
	int cnt;
	char mesgbuf[buflen];
	int snd_wnd, rcv_wnd;

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
		if (use_repair)
			gettcpwin_repair(sockfd, &snd_wnd, &rcv_wnd);
		else
			gettcpwin_info(sockfd, &snd_wnd, &rcv_wnd);
		printf("sending: sndwnd %i rcvwnd %i sentbytes %i (%s)\n",snd_wnd,rcv_wnd, s, (use_repair?"repair":"info"));
		tim.tv_sec = 0;
		tim.tv_nsec = 100000000L;
		nanosleep(&tim, &rem);
	}
}

/***
 This uses using TCP_INFO socket option to peak at the send and receive window sizes of a socket. This is better than using TCP_REPAIR_WINDOW, 
 because that requires "turning off" TCP protocol in order to peak at the window sizes, which requires NET_ADMIN capability, and also means 
 for a split second, packets could be lost.

 Idea from https://stackoverflow.com/questions/54070889/how-to-get-the-tcp-window-size-of-a-socket-in-linux/54071393#54071393 
 Documentation at http://linuxgazette.net/136/pfeiffer.html and https://github.com/torvalds/linux/blob/master/include/uapi/linux/tcp.h
 ***/

int gettcpwin_info(int sockfd, int * snd_wnd, int * rcv_wnd) {
	// from https://stackoverflow.com/a/54071393
	struct tcp_info ti;
	socklen_t tisize = sizeof(ti);
	int r = getsockopt(sockfd, IPPROTO_TCP, TCP_INFO, &ti, &tisize);
	if ( r < 0 ) perror("getsockopts TCP_INFO");
	if (snd_wnd != NULL) *snd_wnd = ti.tcpi_snd_wnd;
	if (rcv_wnd != NULL) *rcv_wnd = ti.tcpi_rcv_wnd;
	return r;
}


#define _REPAIR_ON 1
#define _REPAIR_OFF 0
int _repair(int sockfd, int st) {
	int r = setsockopt(sockfd, IPPROTO_TCP, TCP_REPAIR, &st, sizeof( st ));
	if (r < 0 ) {
		char * state;
		if (st == _REPAIR_ON)
			state = "setsockopts TCP_REPAIR on ";
		else
			state = "setsockopts TCP_REPAIR off";
		perror(state);
	}
	return r;
}
/***
 TCP_REPAIR_WINDOW can not be used to peak at the state of a socket's TCP Send and Receive windows 
 without the socket first being put into TCP_REPAIR mode. But that turns the TCP Protocol engine off, 
 so you need to take it out of TCP_REPAIR mode after getting the window states, otherwise your socket is broken. 
 While TCP_REPAIR mode is on, there may be a change that packets get lost. 

 Can't find a lot of documentation on TCP_REPAIR_WINDOW socket option. Can only find a bit about TCP_REPAIR socket option.

 https://criu.org/TCP_connection talks about TCP_REPAIR, as does https://lwn.net/Articles/495304/ . The former link suggests you can't 
    leave TCP_REPAIR turned on for a socket that you intend to keep using, because it actually shuts off the TCP protocol.

 https://stackoverflow.com/questions/54070889/how-to-get-the-tcp-window-size-of-a-socket-in-linux/54071393#54071393 suggests using 
    TCP_REPAIR_WINDOW to get the window sizes, but doesn't mention needing to turn on TCP_REPAIR first (except in a comment I just left).

 https://github.com/YutaroHayakawa/libtcprepair is a little standalone library that does TCP socket freeze/thaw aka checkpoint/restore. 
    This doesn't help us easily get the window sizes, but I just think it's neat.
 ***/

int gettcpwin_repair(int sockfd, int * snd_wnd, int * rcv_wnd) {
	// from https://stackoverflow.com/a/54071393
	struct tcp_repair_window trw;
	socklen_t trwsize = sizeof(trw);
	_repair(sockfd,_REPAIR_ON);
	int r = getsockopt(sockfd, IPPROTO_TCP, TCP_REPAIR_WINDOW, &trw, &trwsize);
	_repair(sockfd,_REPAIR_OFF);
	if ( r < 0 ) perror("getsockopts TCP_REPAIR_WINDOW");
	if (snd_wnd != NULL) *snd_wnd = trw.snd_wnd;
	if (rcv_wnd != NULL) *rcv_wnd = trw.rcv_wnd;
	return r;
}
