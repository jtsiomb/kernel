#ifndef FILE_H_
#define FILE_H_

#include "fs.h"

struct file {
	struct inode *inode;
	long ptr;
};

#endif	/* FILE_H_ */
