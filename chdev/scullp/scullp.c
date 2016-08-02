//#include <unistd.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <asm/div64.h>
#include <linux/device.h>
#include <linux/semaphore.h>
#include <linux/math64.h>
#include "scullp.h"
#include <linux/sched.h>
#include <linux/poll.h>


#define SCULLP_BUFFERSIZE 4000
static dev_t dev;
int ret;

t_scullp scullp_dev;
struct class *scullp_class;

int scullp_open(struct inode *inode,struct file *filp)
{
	t_scullp *scullp_dev;
	printk("scullp_open\n");
	scullp_dev=container_of(inode->i_cdev,t_scullp,cdev);
	filp->private_data=scullp_dev;

	if((filp->f_flags&O_ACCMODE)==O_WRONLY){
		//scullp_trim(scullp_dev);
	}
	return 0;


}
int scullp_release(struct inode *inode,struct file *filp)
{
	return 0;
}

ssize_t scullp_read(struct file *filp,char __user *buff,size_t count,loff_t *offp)
{
	t_scullp *dev=filp->private_data;
	printk("%s:%d\n",__FUNCTION__,__LINE__);
	if(down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	
	printk("[%s:%d]:dev->wp:%p\n",__FUNCTION__,__LINE__,dev->wp);
	while(dev->rp==dev->wp){
		up(&dev->sem);
		if(filp->f_flags&O_NONBLOCK)
			return -EAGAIN;
		if(wait_event_interruptible(dev->readq,(dev->rp!=dev->wp)))
			return -ERESTARTSYS;
		if(down_interruptible(&dev->sem))
			return -ERESTARTSYS;
	}
	printk("%s:%d\n",__FUNCTION__,__LINE__);
	
	if(dev->rp<dev->wp){
		count =min(count,(dev->wp-dev->rp));
	}else{
		count =min(count,(dev->end-dev->rp));
	}
	printk("%s:%d\n",__FUNCTION__,__LINE__);
	if(copy_to_user(buff,dev->rp,count)){
		up(&dev->sem);
		return -ERESTARTSYS;
	}
	dev->rp+=count;
	if(dev->rp==dev->end)
		dev->rp=dev->buffer;
	up(&dev->sem);
	wake_up_interruptible(&dev->readq);
	return count;
}
int scullp_spacefree(t_scullp *dev)
{
	int ret_off,ret_rest,div;
	div=dev->rp+dev->buffersize-dev->wp;

	if(dev->wp==dev->rp)
		return dev->buffersize-1;
	//return ((dev->wp>dev->rp)?(dev->buffersize-(dev->wp-dev->rp)-1):(dev->buffersize-1-(dev->rp-dev->wq)));
	ret=div_u64_rem(div,dev->buffersize,&ret_rest);
	return ret-1;
}
int scullp_getwritespace(t_scullp *dev,struct file *filp)
{
	while(scullp_spacefree(dev)==0){
		DEFINE_WAIT(wait);
		up(&dev->sem);		
		if(filp->f_flags&O_NONBLOCK)
		return -EAGAIN;
		prepare_to_wait(&dev->writeq,&wait,TASK_INTERRUPTIBLE);
		if(scullp_spacefree(dev)==0)
			schedule();
		finish_wait(&dev->writeq,&wait);
		if(signal_pending(current))
			return -ERESTARTSYS;
		if(down_interruptible(&dev->sem)){
			return -ERESTARTSYS;
		}
	}
	return 0;
}
ssize_t scullp_write(struct file *filp,const char __user *buff,size_t count,loff_t *offp)
{
	t_scullp *dev=filp->private_data;
	int ret;
	printk("scullp_write\n");
	if(down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	printk("------1------\n");	
	ret=scullp_getwritespace(dev,filp);
	if(ret)
		return ret;
	printk("------2-------\n");
	ret=scullp_spacefree(dev);
	printk("------3-------\n");
	//down_interruptible(&dev->sem);
	if(dev->wp>=dev->rp){
		count=min(count,ret);
	}else{
		count=min(count,dev->rp-dev->wp-1);
	}
	printk("going to accept %li bytes to %p form %p\n",(long)count,dev->wp,buff);
	if(copy_from_user(dev->wp,buff,count)){
		up(&dev->sem);
		return -ERESTARTSYS;
	}
	dev->wp+=count;
	printk("dev->wp:%p\n",dev->wp);
	if(dev->wp==dev->end)
		dev->wp=dev->buffer;
	up(&dev->sem);
	wake_up_interruptible(&dev->readq);//writing success,wake up read queue
	if(dev->async_queue)
		kill_fasync(&dev->async_queue,SIGIO,POLL_IN);
	return count;

}

int scullp_fasync(int fd,struct file *filp,int mode)
{
	t_scullp *dev=filp->private_data;
	return fasync_helper(fd,filp,mode,&dev->async_queue);

}

unsigned int scullp_poll(struct file *filp,poll_table *wait)
{
	t_scullp *dev=filp->private_data;
	unsigned int mask=0;
	down(&dev->sem);
	poll_wait(filp,&dev->rp,wait);
	poll_wait(filp,&dev->wp,wait);
	if(dev->wp!=dev->rp)
		mask|=POLLIN|POLLRDNORM;
	if(scullp_spacefree(dev))
		mask|=POLLOUT|POLLWRNORM;
	up(&dev->sem);
	return mask;
}

struct file_operations scullp_fops={
	.owner=THIS_MODULE,
	.open=scullp_open,
	.release=scullp_release,
	.read=scullp_read,
	.write=scullp_write,
};
int scullp_init(void)
{	
	//t_scullp scullp_dev;
	//dev=MKDEV("scullp",0);
	//register_chrdev_region(dev,1,"scullp");
	sema_init(&scullp_dev.sem,1);
	//DECLARE_MUTEX(scullp_dev.sem);
	//DEFINE_SEMAPHORE(&scullp_dev->sem);
	ret=alloc_chrdev_region(&dev,0,1,"scullp");
	if(ret!=0){
		printk("alloc_chrdev_region error!\n");
		goto unregister_dev;
	}
	cdev_init(&scullp_dev.cdev,&scullp_fops);
	scullp_dev.cdev.owner=THIS_MODULE;
	scullp_dev.cdev.ops=&scullp_fops;
	scullp_dev.buffer=kmalloc(SCULLP_BUFFERSIZE,GFP_KERNEL);
	scullp_dev.buffersize=SCULLP_BUFFERSIZE;
	scullp_dev.end=scullp_dev.buffer+scullp_dev.buffersize;
	scullp_dev.rp=scullp_dev.wp=scullp_dev.buffer;
	//scullp_dev.quantum=QUANTUM;
	//scullp_dev.qset=QSET;
	//scullp_dev.size=0;
	init_waitqueue_head(&scullp_dev.writeq);
	init_waitqueue_head(&scullp_dev.readq);
	ret=cdev_add(&scullp_dev.cdev,dev,1);
	if(ret!=0){
		printk("cdev_add error!\n");
		goto cdev_delete;
	}
	scullp_class = class_create(THIS_MODULE,DEV_NAME);
	if(IS_ERR(scullp_class)){
		printk("class create error!\n");
		return -1;
	}
	device_create(scullp_class,NULL, dev,NULL,DEV_NAME);
	printk(KERN_ERR "scullp_init\n");
	
	return 0;
cdev_delete:
	cdev_del(&scullp_dev.cdev);
unregister_dev:
	unregister_chrdev_region(MINOR(dev),1);
	
	return 0;
}
void scullp_exit(void)
{
	kfree(scullp_dev.buffer);
	scullp_dev.buffer=NULL;
	scullp_dev.rp=scullp_dev.wp=NULL;

	device_destroy(scullp_class, dev); 
	class_destroy(scullp_class);
	unregister_chrdev_region(MINOR(dev),1);
	cdev_del(&scullp_dev.cdev);
	printk(KERN_ERR "scullp_exit\n");
	return;
}
module_init(scullp_init);
module_exit(scullp_exit);
MODULE_LICENSE ("Dual BSD/GPL");
