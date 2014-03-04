#ifndef _SERIALUTIL_H
#define _SERIALUTIL_H

//////////////////////////////////////////////////////////////////////////////
// The set_interface_attribs and set_blocking functions at the bottom of this
// file are courtesy of a post from 'wallyk' on StackOverflow.com.  Minor
// modifications have been made to use our logging, indention, etc..
//////////////////////////////////////////////////////////////////////////////
int set_interface_attribs (int fd, int speed, int parity);
void set_blocking (int fd, int should_block);

#endif /* _SERIALUTIL_H */
