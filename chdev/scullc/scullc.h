#ifndef _SCULLC_H_
#define _SCULLC_H_

#define DEV_NAME "scullc"
#define QUANTUM   4000
#define QSET      1000

#define SCULLC_IOC_MAGIC 's'
#define SCULLC_IOCRESET    _IO(SCULLC_IOC_MAGIC, 0)

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": switch G and S atomically
 * H means "sHift": switch T and Q atomically
 */
#define SCULLC_IOCSQUANTUM _IOW(SCULLC_IOC_MAGIC,  1, int)
#define SCULLC_IOCSQSET    _IOW(SCULLC_IOC_MAGIC,  2, int)
#define SCULLC_IOCTQUANTUM _IO(SCULLC_IOC_MAGIC,   3)
#define SCULLC_IOCTQSET    _IO(SCULLC_IOC_MAGIC,   4)
#define SCULLC_IOCGQUANTUM _IOR(SCULLC_IOC_MAGIC,  5, int)
#define SCULLC_IOCGQSET    _IOR(SCULLC_IOC_MAGIC,  6, int)
#define SCULLC_IOCQQUANTUM _IO(SCULLC_IOC_MAGIC,   7)
#define SCULLC_IOCQQSET    _IO(SCULLC_IOC_MAGIC,   8)
#define SCULLC_IOCXQUANTUM _IOWR(SCULLC_IOC_MAGIC, 9, int)
#define SCULLC_IOCXQSET    _IOWR(SCULLC_IOC_MAGIC,10, int)
#define SCULLC_IOCHQUANTUM _IO(SCULLC_IOC_MAGIC,  11)
#define SCULLC_IOCHQSET    _IO(SCULLC_IOC_MAGIC,  12)

/*
 * The other entities only have "Tell" and "Query", because they're
 * not printed in the book, and there's no need to have all six.
 * (The previous stuff was only there to show different ways to do it.
 */
#define SCULLC_P_IOCTSIZE _IO(SCULLC_IOC_MAGIC,   13)
#define SCULLC_P_IOCQSIZE _IO(SCULLC_IOC_MAGIC,   14)
/* ... more to come */

#define SCULLC_IOC_MAXNR 14

typedef struct scullc_qset{
	void **data;
	struct scullc_qset *next;
} t_scullc_qset;
typedef struct scullc{
	void *pdata;
	t_scullc_qset *data;
	int quantum;
	int qset;
	unsigned long size;
	unsigned int access_key;
	struct semaphore sem;
	struct cdev cdev;
} t_scullc;

#endif /*_SCULLC_H_*/
