# neatnetcode
Examples of interesting network related code snippets and examples.

`tcpwin.c` is a demonstration of using TCP_REPAIR_WINDOW to see the values of the TCP receive and send windows of a socket. Compile with `gcc -o tcpwin tcpwin.c`. Needs to be run as root, under `sudo`, or with the NET_ADMIN capability (e.g. `sudo setcap net_cap_admin=+ep tcpwin`). (Reminder to self that `setcap` doesn't work on NFS, apparently.)

`rawread.c` is a demonstration of putting an ethernet device into PROMISC mode, then reading whole ethernet frames from it. It also does writes some ARP packets but I can't remember why. Hardcodes lots of things, for e.g. `eth0` as the network device. Compile with `gcc -o rawread rawread.c`. Needs to be run as root / under `sudo`. Probably also works ig given NET_ADMIN capability, but it was written before that feature existed (or at least, before capabilities were widely available in Linux kernels.)

`rawpack/rawdev.c` is a bidirectional rewrite of `rawread.c`, intended to be used as the Layer 2 implementation for `rawpack`. `rawpack` was intended to be a usermode implementation of `netcat`, but once I realized how much work implementing TCP would be I ran out of enthusiam for the project.
