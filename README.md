# neatnetcode
Examples of interesting network related code snippets and examples.

`tcpwinrepair.c` is a demonstration of using TCP_REPAIR_WINDOW to see the values of the TCP receive and send windows of a socket. Compile with `gcc -o tcpwinrepair tcpwinrepair.c`. Run it twice, once as server, e.g. `sudo tcpwinrepair -l 2000` and once as client, e.g. `sudo tcpwinrepair localhost 2000`. Watch as the client sends 1000 byte frames to the server, while the server refuses to read incoming frames. So you can see the send window on the client, and the receive window on the server, go to zero. Once they do, the server will start reading, until the window gets big -- it then has a dumb heuristic to decide when to stop reading again, repeating ad infinitum. Linux may actually make the maximum window size even get bigger as the cycle repeats. Needs to be run as root, under `sudo`, or with the NET_ADMIN capability (e.g. `sudo setcap net_cap_admin=+ep tcpwinrepair`). (Reminder to self that `setcap` doesn't work on NFS, apparently.)

`rawread.c` is a demonstration of putting an ethernet device into PROMISC mode, then reading whole ethernet frames from it. It also does writes some ARP packets but I can't remember why. Hardcodes lots of things, for e.g. `eth0` as the network device. Compile with `gcc -o rawread rawread.c`. Needs to be run as root / under `sudo`. Probably also works ig given NET_ADMIN capability, but it was written before that feature existed (or at least, before capabilities were widely available in Linux kernels.)

`rawpack/rawdev.c` is a bidirectional rewrite of `rawread.c`, intended to be used as the Layer 2 implementation for `rawpack`. `rawpack` was intended to be a usermode implementation of `netcat`, but once I realized how much work implementing TCP would be I ran out of enthusiam for the project.
