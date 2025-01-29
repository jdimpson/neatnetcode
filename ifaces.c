#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

int main (int argc, char *argv[])
{
    struct if_nameindex *ifnames, *ifnm;
    struct ifreq ifr;
    struct sockaddr_in sin, destaddr;
    int sock;

    if (argc > 1) {
        destaddr.sin_addr.s_addr = inet_addr (argv[1]);
    }

    sock = socket (AF_INET, SOCK_DGRAM, 0);
    ifnames = if_nameindex();
    for (ifnm = ifnames; ifnm && ifnm->if_name && ifnm->if_name[0]; ifnm++) {
        strncpy (ifr.ifr_name, ifnm->if_name, IFNAMSIZ);
        if (ioctl (sock, SIOCGIFADDR, &ifr) == 0) {
            memcpy (&sin, &(ifr.ifr_addr), sizeof (sin));
            if (argc > 1) {
                if (memcmp (&(sin.sin_addr), &(destaddr.sin_addr), sizeof (struct in_addr)) == 0)
                    break; /* You found an interface with a matching IP! */
            } else {
                printf ("Interface %s = IP %s\n", ifnm->if_name, inet_ntoa (sin.sin_addr));
            }
        }
    }

    if (argc > 1) {
        if (ifnm && ifnm->if_name && ifnm->if_name[0]) {
            printf ("Interface %s = IP %s\n", ifnm->if_name, inet_ntoa (destaddr.sin_addr));
        } else {
            printf ("No matching interface found for IP %s?!?\n", inet_ntoa (destaddr.sin_addr));
        }
    }
    if_freenameindex (ifnames); 
    close (sock);

    exit (0);
}
