# neatnetcode
Examples of interesting network related code snippets and examples.

`tcpwin.c` is a demonstration of two similar ways to see the values of the [TCP receive and send windows](https://en.wikipedia.org/wiki/TCP_tuning#Window_size) of a socket. Compile with `gcc -o tcpwin tcpwin.c`. Run it twice, once as server, e.g. `tcpwin -l 2000` and once as client, e.g. `tcpwin localhost 2000`. Watch as the client sends 1000 byte frames to the server, while the server refuses to read incoming frames. So you can see the send window on the client, and the receive window on the server, go to zero. Once they do, the server will start reading, until the window gets big -- it then has a dumb heuristic to decide when to stop reading again, repeating ad infinitum. Linux may actually make the maximum window size even get bigger as the cycle repeats. 

When run *without* the `-R` option, `tcpwin` uses `TCP_INFO` socket option to obtain the receive and send window sizes. *With* the `-R` option give, `tcpwin` uses `TCP_REPAIR_WINDOW` socket option to obtain the same. (It also uses `TCP_REPAIR` option, because that needs to be enabled to allow before `TCP_REPAIR_WINDOW` can be used.) Also, to use `-R`, it also needs to be run as root user, under `sudo`, or with the NET_ADMIN capability (e.g. `sudo setcap net_cap_admin=+ep tcpwinrepair`). (Reminder to self that `setcap` doesn't work on NFS, apparently.)

`rawread.c` is a demonstration of putting an ethernet device into PROMISC mode, then reading whole ethernet frames from it. It also does writes some ARP packets but I can't remember why. Hardcodes lots of things, for e.g. `eth0` as the network device. Compile with `gcc -o rawread rawread.c`. Needs to be run as root / under `sudo`. Probably also works if given NET_ADMIN capability, but it was written before that feature existed (or at least, before capabilities were widely available in Linux kernels.)

`rawpack/rawdev.c` is a bidirectional rewrite of `rawread.c`, intended to be used as the Layer 2 implementation for `rawpack`. `rawpack` was intended to be a usermode implementation of `netcat`, but once I realized how much work implementing TCP would be I ran out of enthusiam for the project.

`ifaces.c` is a simple demonstration of how to get a list of network interface devices, then figure out their IP addresses. Compile with `gcc -o ifaces ifaces.c`.

`setvlan_topriority.c` is code that will create a mapping between a Linux sk_priority value and an 802.1Q VLAN Priority Code Point. This can already be done with the `tc`, `vconfig`, and `ip` commands, but is here for reference. It needs to be run as root or have appropriate capability.

`sendbcast_withpriority.c` is example code of sending a UDP broadcast that uses SO_PRIORITY to cause the Linux kernel to set an `sk_priority` value to all packets in sent to the socket. Use this in place of messy `iptables` or `netfilter` commands, especially if you want application code to be in charge. Note this is different than seting IP_TOS, although it is similar. (Check [this analysis](https://www.rationali.st/blog/looking-into-dscp-and-ieee-8021p-vlan-priorities.html) out to understand how.) If you use this code with `setvlan_topriority.c` on a VLAN subinterface, you can see the VLAN PCP values be set. This code also has an example of using SO_BINDTODEVICE for sending broadcast packets, rather than using a network-specific broadcast address and relying on the routing table to send to the correct ethernet interface.
