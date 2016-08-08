#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/ioctl.h>

#define DEV_NAME "scull1"
#define QUANTUM   4000
#define QSET      1000

#define SCULL1_IOC_MAGIC 's'
#define SCULL1_IOCRESET    _IO(SCULL1_IOC_MAGIC, 0)

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": switch G and S atomically
 * H means "sHift": switch T and Q atomically
 */
#define SCULL1_IOCSQUANTUM _IOW(SCULL1_IOC_MAGIC,  1, int)
#define SCULL1_IOCSQSET    _IOW(SCULL1_IOC_MAGIC,  2, int)
#define SCULL1_IOCTQUANTUM _IO(SCULL1_IOC_MAGIC,   3)
#define SCULL1_IOCTQSET    _IO(SCULL1_IOC_MAGIC,   4)
#define SCULL1_IOCGQUANTUM _IOR(SCULL1_IOC_MAGIC,  5, int)
#define SCULL1_IOCGQSET    _IOR(SCULL1_IOC_MAGIC,  6, int)
#define SCULL1_IOCQQUANTUM _IO(SCULL1_IOC_MAGIC,   7)
#define SCULL1_IOCQQSET    _IO(SCULL1_IOC_MAGIC,   8)
#define SCULL1_IOCXQUANTUM _IOWR(SCULL1_IOC_MAGIC, 9, int)
#define SCULL1_IOCXQSET    _IOWR(SCULL1_IOC_MAGIC,10, int)
#define SCULL1_IOCHQUANTUM _IO(SCULL1_IOC_MAGIC,  11)
#define SCULL1_IOCHQSET    _IO(SCULL1_IOC_MAGIC,  12)

/*
 * The other entities only have "Tell" and "Query", because they're
 * not printed in the book, and there's no need to have all six.
 * (The previous stuff was only there to show different ways to do it.
 */
#define SCULL1_P_IOCTSIZE _IO(SCULL1_IOC_MAGIC,   13)
#define SCULL1_P_IOCQSIZE _IO(SCULL1_IOC_MAGIC,   14)
/* ... more to come */

#define SCULL1_IOC_MAXNR 14



int main()
{
	int fd,ret;
	char buf[100]={0};
	fd = open("/dev/scull1",O_RDWR);			
    ret=write(fd, "scull1 write test\n",sizeof("scull1 write test\n"));
	printf("write %d char!\n",ret);
	ret=read(fd,buf,10);
	printf("read test:%s ret:%d\n",buf,ret);
	int quantum;
	ioctl(fd,SCULL1_IOCSQUANTUM,&quantum);
	ioctl(fd,SCULL1_IOCTQUANTUM,quantum);

	ioctl(fd,SCULL1_IOCGQUANTUM,&quantum);
	quantum=ioctl(fd,SCULL1_IOCQQUANTUM);

	ioctl(fd,SCULL1_IOCXQUANTUM,&quantum);
	quantum=ioctl(fd,SCULL1_IOCHQUANTUM,quantum);
	return 0;
}
