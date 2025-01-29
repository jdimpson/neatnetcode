/* Copyright (c) 2008, Jeremy Impson, <jdimpson@acm.org>.
 */

#ifndef __RAWDEV_H__
#define __RAWDEV_H__

void rawdev_init(char * devname, uid_t uid, gid_t gid);
unsigned int rawdev_read(void);
void rawdev_send(void);

#endif /* __RAWDEV_H__ */
