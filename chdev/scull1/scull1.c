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
#include "scull1.h"
#include <linux/sched.h>

static dev_t dev;
int ret;

t_scull1 scull1_dev;
struct class *scull1_class;
int scull1_trim(t_scull1 *scull1_dev)
{
	t_scull1_qset *next,*dptr;
	int qset,quantum;
	qset=scull1_dev->qset;
	quantum=scull1_dev->quantum;
	int i;
	for(dptr=scull1_dev->data;dptr;dptr=next){
		if(dptr->data){
			for(i=0;i<qset;i++)
				kfree(dptr->data[i]);
		}
		next=dptr->next;
		kfree(dptr->data);
	}
	scull1_dev->data=NULL;
	scull1_dev->quantum=quantum;
	scull1_dev->qset=qset;
	scull1_dev->size=0;
	return 0;
}
int scull1_open(struct inode *inode,struct file *filp)
{
	t_scull1 *scull1_dev;
	scull1_dev=container_of(inode->i_cdev,t_scull1,cdev);
	filp->private_data=scull1_dev;

	if((filp->f_flags&O_ACCMODE)==O_WRONLY){
		//scull1_trim(scull1_dev);
	}
	return 0;
}
int scull1_release(struct inode *inode,struct file *filp)
{
	return 0;
}
t_scull1_qset *scull1_fllow(t_scull1 * scull1_dev,int page_rest)
{	
	int tmpnum=page_rest;
	t_scull1_qset *scull1_qset=NULL;
	if(scull1_dev==NULL)
		return NULL;
	scull1_qset=scull1_dev->data;
	//printk("scull1:%x\n",scull1_qset);
	if(scull1_qset==NULL){
		scull1_qset=scull1_dev->data=kmalloc(sizeof(t_scull1_qset),GFP_KERNEL);
		if(scull1_qset==NULL){
			printk("kmalloc scull1_qset error!\n");
			return NULL;
		}
		memset(scull1_qset,0,sizeof(t_scull1_qset));
	}
	//printk("scull1:%x scull1_dev->data:%x\n",scull1_qset,scull1_dev->data);
	while(tmpnum--){
		scull1_qset=scull1_qset->next;
		if(scull1_qset==NULL)
			scull1_qset=kmalloc(sizeof(t_scull1_qset),GFP_KERNEL);
		if(scull1_qset==NULL){
			printk("kmalloc scull1_qset error in while!\n");
			return NULL;
		}
		memset(scull1_qset,0,sizeof(t_scull1_qset));
		//printk("scull1:%x scull1_dev->data:%x\n",scull1_qset,scull1_dev->data);
	}
	
	return scull1_qset;

}
ssize_t scull1_read(struct file *filp,char __user *buff,size_t count,loff_t *offp)
{
	t_scull1 *scull1_dev=filp->private_data;
	t_scull1_qset *scull1_qset;

	int ret=0;
	long long off=*offp;
	//printk("scull1_dev *offp:%d\n",*offp);
	*offp=0;
	int qset=scull1_dev->qset;
	int quantum=scull1_dev->quantum;
	int pagesize=qset*quantum;
	//printk("scull1_dev qset:%d quantum:%d\n",qset,quantum);
	/*
	int page_rest=do_div(*offp,pagesize);
	int page_off=*offp;
	int set_rest=do_div(page_rest,quantum);
	printk("scull1_dev qset:%d quantum:%d page_rest:%d page_off:%d\n",qset,quantum,page_rest,page_off);
	int set_off=page_rest;
	printk("set_rest:%d\n",set_rest);
	set_rest=(set_rest>=4000)?(set_rest-4000):set_rest;
	printk("set_rest:%d\n",set_rest);
	printk("scull1_read scull1_dev:%x count:%d size:%d *offp:%d quantum:%d set_rest:%d\n",\
			scull1_dev,count,scull1_dev->size,*offp,quantum,set_rest);

	*/
	int page_off,page_rest,set_off,set_rest;
	page_off=div_u64_rem(*offp,pagesize,&page_rest);
	set_off=div_u64_rem(page_rest,quantum,&set_rest);
	
	if(down_interruptible(&scull1_dev->sem))
		return -ERESTARTSYS;
#if 1

	if(*offp >scull1_dev->size)
		goto out;
	if(*offp+count >scull1_dev->size)
		count=scull1_dev->size-*offp;
	

	scull1_qset=scull1_fllow(scull1_dev,page_off);//未实现该函数
	if(scull1_qset==NULL||!scull1_qset->data||!scull1_qset->data[set_off]){
		printk("scull1_qset==NULL||!scull1_qset->data||!scull1_qset->data[set_rest]\n");
		goto out;
	}
	//printk("set_rest:%d\n",set_rest);
#if 1

	if(count>scull1_dev->quantum-set_rest)
		count=scull1_dev->quantum-set_rest;
	
	if(copy_to_user(buff,scull1_qset->data[set_off]+set_rest,count)){
		ret=-EFAULT;
		goto out;
	}
	*offp+=count;
	ret=count;
//	printk("count:%d ret:%d set_rest:%d set_off:%d page_rest:%d size:%d\n",\
		count,ret,set_rest,set_off,page_rest,scull1_dev->size);
#endif 
#endif
	//copy_to_user(buff,scull1_qset->data[0],count);
out:
	up(&scull1_dev->sem);
	return ret;
	
}
ssize_t scull1_write(struct file *filp,const char __user *buff,size_t count,loff_t *offp)
{
	t_scull1 *scull1_dev=filp->private_data;
	t_scull1_qset *scull1_qset;
	int ret= -ENOMEM;
	long long off=*offp;
	int qset=scull1_dev->qset;
	int quantum=scull1_dev->quantum;
	int pagesize=qset*quantum;
	int page_rest=do_div(off,pagesize);
	int page_off=off;
	int set_rest=do_div(page_rest,quantum);
	int set_off=page_rest;
	char buf[100]={0};
	if(down_interruptible(&scull1_dev->sem))
		return -ERESTARTSYS;
	//printk("scull1_write scull1_dev:%x\n",scull1_dev);
#if 1
	scull1_qset=scull1_fllow(scull1_dev,page_off);//未实现该函数
	if(scull1_qset==NULL){
		printk("scull1_qset is NULL!\n");
		goto out;
	}
	if(!scull1_qset->data){
		scull1_qset->data=kmalloc(qset*sizeof(char *),GFP_KERNEL);
		if(!scull1_qset->data){
			printk("scull1_qset->data is NULL!\n");
			goto out;
		}
		memset(scull1_qset->data,0,qset*sizeof(char *));
	}
	if(!scull1_qset->data[set_off]){
		scull1_qset->data[set_off]=kmalloc(quantum,GFP_KERNEL);
		if(!scull1_qset->data[set_off]){
			printk("scull1_qset->data[set_off] is NULL!\n");
			goto out;
		}
		memset(scull1_qset->data[set_off],0,quantum);
	}
	printk("count:%d quantum:%d set_off:%d set_rest:%d\n",count,quantum,set_off,set_rest);
	if(count>quantum-set_off)
		count=quantum-set_off;

	//copy_from_user(buf,buff,count);
	//printk("copy_from_user(buf,buff,count):%d\n",ret);
	if(copy_from_user(scull1_qset->data[set_off]+set_rest,buff,count)){
		ret=-EFAULT;
		goto out;
	}
	printk("%s\n",(char *)(scull1_qset->data[set_rest]+set_off));
	/*
	int i;
	char *tmpbuf=scull1_qset->data[set_off]+set_rest;
	for(i=0;i<count;i++){
		printk("%c\n",*(char *)(scull1_qset->data[set_rest]+set_off+i));
		*(tmpbuf+i)='c';
		printk("tmpbuf:%c\n",*(tmpbuf+i));
	}
	memcpy((char*)(scull1_qset->data[set_off]+set_rest),buf,count);
	for(i=0;i<count;i++){
		printk("%x\n",*(char *)(scull1_qset->data[set_rest]+set_off+i));
		printk("%s\n",(char *)(scull1_qset->data[set_rest]+set_off));
		//*(tmpbuf+i)='c';
		//printk("tmpbuf:%c\n",*(tmpbuf+i));
	}
	printk("\n");
	printk("buf:%s\n",buf);
	*/
	*offp+=count;
	if(scull1_dev->size<*offp)
		scull1_dev->size=*offp;
	ret=count;
	//printk("count:%d ret:%d set_rest:%d set_off:%d page_rest:%d size:%d scull1_dev->data:%x\n",\
	//	count,ret,set_rest,set_off,page_rest,scull1_dev->size,scull1_dev->data);
#endif
	
out:
	up(&scull1_dev->sem);
	return ret;
	
}
int scull1_ioctl(struct inode *inode, struct file *filp,unsigned int cmd, unsigned long arg)
{
	int err=0,tmp;
	int ret=0;
	int scull1_quantum=0,scull1_qset=0;
	if(_IOC_TYPE(cmd)!=SCULL1_IOC_MAGIC) return -ENOTTY;
	if(_IOC_NR(cmd)>SCULL1_IOC_MAXNR) return -ENOTTY;
	
	if(_IOC_DIR(cmd)&_IOC_READ)
		err=!access_ok(VERIFY_WRITE,(void __user *)arg,_IOC_SIZE(cmd));
	else if(_IOC_DIR(cmd)&_IOC_WRITE)
		err=!access_ok(VERIFY_READ,(void __user *)arg,_IOC_SIZE(cmd));
	if(err)
		return -EFAULT;
	switch(cmd){
		case SCULL1_IOCSQUANTUM :
			if(!capable(CAP_SYS_ADMIN))
				return -EPERM;
			ret=__get_user(scull1_quantum,(int __user*)arg);
			break;
		case SCULL1_IOCSQSET   :
			scull1_quantum=QUANTUM;
			scull1_qset=QSET;
			break;
		case SCULL1_IOCTQUANTUM:
			if(!capable(CAP_SYS_ADMIN))
				return -EPERM;
			scull1_quantum=arg;
			break;
		case SCULL1_IOCGQUANTUM:
			ret=__put_user(scull1_quantum,(int __user *)arg);
			break;
		case SCULL1_IOCQQUANTUM:
			return scull1_quantum;
		case SCULL1_IOCXQUANTUM:
			if(!capable(CAP_SYS_ADMIN))
				return -EPERM;
			tmp=scull1_quantum;
			ret=__get_user(scull1_quantum,(int __user*)arg);
			if(ret==0)
				ret=__put_user(tmp,(int __user*)arg);
			break;
		case SCULL1_IOCHQUANTUM:
			if(!capable(CAP_SYS_ADMIN))
				return -EPERM;
			tmp=scull1_quantum;
			scull1_quantum=arg;
			return arg;
		default:
			return -ENOTTY;
	}
	return 0;
}
loff_t scull1_llseek(struct file *filp,loff_t off,int whence)
{
	t_scull1 *dev=filp->private_data;
	loff_t newpos;
	switch(whence){
		case 0://SEEK_SET
			newpos=off;
			break;
		case 1://SEEK_CUR
			newpos=filp->f_pos+off;
			break;
		case 2://SEEK_END
			newpos=dev->size+off;
		default:
			return -EINVAL;
	}
	if(newpos<0) return -EINVAL;
	filp->f_pos=newpos;
	return newpos;
}

struct file_operations scull1_fops={
	.owner=THIS_MODULE,
	.open=scull1_open,
	.release=scull1_release,
	.read=scull1_read,
	.write=scull1_write,
	.ioctl=scull1_ioctl,
};
int scull1_init(void)
{	
	//t_scull1 scull1_dev;
	//dev=MKDEV("scull1",0);
	//register_chrdev_region(dev,1,"scull1");
	sema_init(&scull1_dev.sem,1);
	//DECLARE_MUTEX(scull1_dev.sem);
	//DEFINE_SEMAPHORE(&scull1_dev->sem);
	ret=alloc_chrdev_region(&dev,0,1,"scull1");
	if(ret!=0){
		printk("alloc_chrdev_region error!\n");
		goto unregister_dev;
	}
	cdev_init(&scull1_dev.cdev,&scull1_fops);
	scull1_dev.cdev.owner=THIS_MODULE;
	scull1_dev.cdev.ops=&scull1_fops;
	scull1_dev.quantum=QUANTUM;
	scull1_dev.qset=QSET;
	scull1_dev.size=0;
	ret=cdev_add(&scull1_dev.cdev,dev,1);
	if(ret!=0){
		printk("cdev_add error!\n");
		goto cdev_delete;
	}
	scull1_class = class_create(THIS_MODULE,DEV_NAME);
	if(IS_ERR(scull1_class)){
		printk("class create error!\n");
		return -1;
	}
	device_create(scull1_class,NULL, dev,NULL,DEV_NAME);
	printk(KERN_ERR "scull1_init\n");
	
	return 0;
cdev_delete:
	cdev_del(&scull1_dev.cdev);
unregister_dev:
	unregister_chrdev_region(MINOR(dev),1);
	
	return 0;
}
void scull1_exit(void)
{
	device_destroy(scull1_class, dev); 
	class_destroy(scull1_class);
	unregister_chrdev_region(MINOR(dev),1);
	cdev_del(&scull1_dev.cdev);
	printk(KERN_ERR "scull1_exit\n");
	return;
}
module_init(scull1_init);
module_exit(scull1_exit);
MODULE_LICENSE ("Dual BSD/GPL");
