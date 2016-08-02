#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/ioctl.h>
#include <errno.h>
#include <stdlib.h>

#define DEV_NAME "scullp"
#define QUANTUM   4000
#define QSET      1000

#define SCULLP_IOC_MAGIC 's'
#define SCULLP_IOCRESET    _IO(SCULLP_IOC_MAGIC, 0)

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": switch G and S atomically
 * H means "sHift": switch T and Q atomically
 */
#define SCULLP_IOCSQUANTUM _IOW(SCULLP_IOC_MAGIC,  1, int)
#define SCULLP_IOCSQSET    _IOW(SCULLP_IOC_MAGIC,  2, int)
#define SCULLP_IOCTQUANTUM _IO(SCULLP_IOC_MAGIC,   3)
#define SCULLP_IOCTQSET    _IO(SCULLP_IOC_MAGIC,   4)
#define SCULLP_IOCGQUANTUM _IOR(SCULLP_IOC_MAGIC,  5, int)
#define SCULLP_IOCGQSET    _IOR(SCULLP_IOC_MAGIC,  6, int)
#define SCULLP_IOCQQUANTUM _IO(SCULLP_IOC_MAGIC,   7)
#define SCULLP_IOCQQSET    _IO(SCULLP_IOC_MAGIC,   8)
#define SCULLP_IOCXQUANTUM _IOWR(SCULLP_IOC_MAGIC, 9, int)
#define SCULLP_IOCXQSET    _IOWR(SCULLP_IOC_MAGIC,10, int)
#define SCULLP_IOCHQUANTUM _IO(SCULLP_IOC_MAGIC,  11)
#define SCULLP_IOCHQSET    _IO(SCULLP_IOC_MAGIC,  12)

/*
 * The other entities only have "Tell" and "Query", because they're
 * not printed in the book, and there's no need to have all six.
 * (The previous stuff was only there to show different ways to do it.
 */
#define SCULLP_P_IOCTSIZE _IO(SCULLP_IOC_MAGIC,   13)
#define SCULLP_P_IOCQSIZE _IO(SCULLP_IOC_MAGIC,   14)
/* ... more to come */

#define SCULLP_IOC_MAXNR 14



int main(int argc,char **argv)
{
	int delay=1,n,m=0;
	char buffer[4096]={0};
	if(argc>1)
		delay=atoi(argv[1]);
	fcntl(0,F_SETFL,fcntl(0,F_GETFL)|O_NONBLOCK);
	fcntl(1,F_SETFL,fcntl(1,F_GETFL)|O_NONBLOCK);
	while(1){
		n=read(0,buffer,4096);
		if(n>0)
			m=write(1,buffer,n);
		if((n<0||m<0)&&(errno!=-EAGAIN))
			break;
		sleep(delay);
	}
	perror(n<0?"stdin":"stdout");
	exit(1);

}
