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

#define DEV_NAME "scull0"
#define QUANTUM   4000
#define QSET      1000


static dev_t dev;
int ret;
typedef struct scull0_qset{
	void **data;
	struct sucll0_qset *next;
} t_scull0_qset;
typedef struct sucll0{
	void *pdata;
	t_scull0_qset *data;
	int quantum;
	int qset;
	unsigned long size;
	unsigned int access_key;
	struct semaphore sem;
	struct cdev cdev;
} t_scull0;

t_scull0 scull0_dev;
struct class *scull0_class;
int scull0_trim(t_scull0 *scull0_dev)
{
	t_scull0_qset *next,*dptr;
	int qset,quantum;
	qset=scull0_dev->qset;
	quantum=scull0_dev->quantum;
	int i;
	for(dptr=scull0_dev->data;dptr;dptr=next){
		if(dptr->data){
			for(i=0;i<qset;i++)
				kfree(dptr->data[i]);
		}
		next=dptr->next;
		kfree(dptr->data);
	}
	scull0_dev->data=NULL;
	scull0_dev->quantum=quantum;
	scull0_dev->qset=qset;
	scull0_dev->size=0;
	return 0;
}
int scull0_open(struct inode *inode,struct file *filp)
{
	t_scull0 *scull0_dev;
	scull0_dev=container_of(inode->i_cdev,t_scull0,cdev);
	filp->private_data=scull0_dev;

	if((filp->f_flags&O_ACCMODE)==O_WRONLY){
		//scull0_trim(scull0_dev);
	}
	return 0;
}
int scull0_release(struct inode *inode,struct file *filp)
{
	return 0;
}
t_scull0_qset *scull0_fllow(t_scull0 * scull0_dev,int page_rest)
{	
	int tmpnum=page_rest;
	t_scull0_qset *scull0_qset=NULL;
	if(scull0_dev==NULL)
		return NULL;
	scull0_qset=scull0_dev->data;
	printk("scull0:%x\n",scull0_qset);
	if(scull0_qset==NULL){
		scull0_qset=scull0_dev->data=kmalloc(sizeof(t_scull0_qset),GFP_KERNEL);//注意指针
		//scull0_qset=kmalloc(sizeof(t_scull0_qset),GFP_KERNEL);
		if(scull0_qset==NULL){
			printk("kmalloc scull0_qset error!\n");
			return NULL;
		}
		memset(scull0_qset,0,sizeof(t_scull0_qset));
	}
	printk("scull0:%x scull0_dev->data:%x\n",scull0_qset,scull0_dev->data);
	while(tmpnum--){
		scull0_qset=scull0_qset->next;
		if(scull0_qset==NULL)
			scull0_qset=kmalloc(sizeof(t_scull0_qset),GFP_KERNEL);
		if(scull0_qset==NULL){
			printk("kmalloc scull0_qset error in while!\n");
			return NULL;
		}
		memset(scull0_qset,0,sizeof(t_scull0_qset));
		printk("scull0:%x scull0_dev->data:%x\n",scull0_qset,scull0_dev->data);
	}
	
	return scull0_qset;

}
ssize_t scull0_read(struct file *filp,char __user *buff,size_t count,loff_t *offp)
{
	t_scull0 *scull0_dev=filp->private_data;
	t_scull0_qset *scull0_qset;

	int ret=0;
	long long off=*offp;
	//printk("scull0_dev *offp:%d\n",*offp);
	*offp=0;
	int qset=scull0_dev->qset;
	int quantum=scull0_dev->quantum;
	int pagesize=qset*quantum;
	//printk("scull0_dev qset:%d quantum:%d\n",qset,quantum);
	/*
	int page_rest=do_div(*offp,pagesize);
	int page_off=*offp;
	int set_rest=do_div(page_rest,quantum);
	printk("scull0_dev qset:%d quantum:%d page_rest:%d page_off:%d\n",qset,quantum,page_rest,page_off);
	int set_off=page_rest;
	printk("set_rest:%d\n",set_rest);
	set_rest=(set_rest>=4000)?(set_rest-4000):set_rest;
	printk("set_rest:%d\n",set_rest);
	printk("scull0_read scull0_dev:%x count:%d size:%d *offp:%d quantum:%d set_rest:%d\n",\
			scull0_dev,count,scull0_dev->size,*offp,quantum,set_rest);

	*/
	int page_off,page_rest,set_off,set_rest;
	page_off=div_u64_rem(*offp,pagesize,&page_rest);
	set_off=div_u64_rem(page_rest,quantum,&set_rest);
	
	if(down_interruptible(&scull0_dev->sem))
		return -ERESTARTSYS;
#if 1

	if(*offp >scull0_dev->size)
		goto out;
	if(*offp+count >scull0_dev->size)
		count=scull0_dev->size-*offp;
	

	scull0_qset=scull0_fllow(scull0_dev,page_off);//未实现该函数
	if(scull0_qset==NULL||!scull0_qset->data||!scull0_qset->data[set_off]){
		printk("scull0_qset==NULL||!scull0_qset->data||!scull0_qset->data[set_rest]\n");
		goto out;
	}
	//printk("set_rest:%d\n",set_rest);
#if 1

	if(count>scull0_dev->quantum-set_rest)
		count=scull0_dev->quantum-set_rest;
	
	if(copy_to_user(buff,scull0_qset->data[set_off]+set_rest,count)){
		ret=-EFAULT;
		goto out;
	}
	*offp+=count;
	ret=count;
	printk("count:%d ret:%d set_rest:%d set_off:%d page_rest:%d size:%d\n",\
		count,ret,set_rest,set_off,page_rest,scull0_dev->size);
#endif 
#endif
	//copy_to_user(buff,scull0_qset->data[0],count);
out:
	up(&scull0_dev->sem);
	return ret;
	
}
ssize_t scull0_write(struct file *filp,const char __user *buff,size_t count,loff_t *offp)
{
	t_scull0 *scull0_dev=filp->private_data;
	t_scull0_qset *scull0_qset;
	int ret= -ENOMEM;
	long long off=*offp;
	int qset=scull0_dev->qset;
	int quantum=scull0_dev->quantum;
	int pagesize=qset*quantum;
	int page_rest=do_div(off,pagesize);
	int page_off=off;
	int set_rest=do_div(page_rest,quantum);
	int set_off=page_rest;
	char buf[100]={0};
	if(down_interruptible(&scull0_dev->sem))
		return -ERESTARTSYS;
	printk("scull0_write scull0_dev:%x\n",scull0_dev);
#if 1
	scull0_qset=scull0_fllow(scull0_dev,page_off);//未实现该函数
	if(scull0_qset==NULL){
		printk("scull0_qset is NULL!\n");
		goto out;
	}
	if(!scull0_qset->data){
		scull0_qset->data=kmalloc(qset*sizeof(char *),GFP_KERNEL);
		if(!scull0_qset->data){
			printk("scull0_qset->data is NULL!\n");
			goto out;
		}
		memset(scull0_qset->data,0,qset*sizeof(char *));
	}
	if(!scull0_qset->data[set_off]){
		scull0_qset->data[set_off]=kmalloc(quantum,GFP_KERNEL);
		if(!scull0_qset->data[set_off]){
			printk("scull0_qset->data[set_off] is NULL!\n");
			goto out;
		}
		memset(scull0_qset->data[set_off],0,quantum);
	}
	printk("count:%d quantum:%d set_off:%d set_rest:%d\n",count,quantum,set_off,set_rest);
	if(count>quantum-set_off)
		count=quantum-set_off;

	copy_from_user(buf,buff,count);
	//printk("copy_from_user(buf,buff,count):%d\n",ret);
	if(copy_from_user(scull0_qset->data[set_off]+set_rest,buff,count)){
		ret=-EFAULT;
		goto out;
	}
	printk("%s\n",(char *)(scull0_qset->data[set_rest]+set_off));
	/*
	int i;
	char *tmpbuf=scull0_qset->data[set_off]+set_rest;
	for(i=0;i<count;i++){
		printk("%c\n",*(char *)(scull0_qset->data[set_rest]+set_off+i));
		*(tmpbuf+i)='c';
		printk("tmpbuf:%c\n",*(tmpbuf+i));
	}
	memcpy((char*)(scull0_qset->data[set_off]+set_rest),buf,count);
	for(i=0;i<count;i++){
		printk("%x\n",*(char *)(scull0_qset->data[set_rest]+set_off+i));
		printk("%s\n",(char *)(scull0_qset->data[set_rest]+set_off));
		//*(tmpbuf+i)='c';
		//printk("tmpbuf:%c\n",*(tmpbuf+i));
	}
	printk("\n");
	printk("buf:%s\n",buf);
	*/
	*offp+=count;
	if(scull0_dev->size<*offp)
		scull0_dev->size=*offp;
	ret=count;
	printk("count:%d ret:%d set_rest:%d set_off:%d page_rest:%d size:%d scull0_dev->data:%x\n",\
		count,ret,set_rest,set_off,page_rest,scull0_dev->size,scull0_dev->data);
#endif
	
out:
	up(&scull0_dev->sem);
	return ret;
	
}
loff_t scull0_llseek(struct file* filp,loff_t off,int whence)
{
	t_scull0 *dev=filp->private_data;
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
			break;
		default:
			return -EINVAL;
	}
	if(newpos<0) 	return -EINVAL;
	filp->f_pos=newpos;
	return newpos;
}
struct file_operations scull0_fops={
	.owner=THIS_MODULE,
	.open=scull0_open,
	.release=scull0_release,
	.read=scull0_read,
	.write=scull0_write,
	.llseek=scull0_llseek,
};
int scull0_init(void)
{	
	//t_scull0 scull0_dev;
	//dev=MKDEV("scull0",0);
	//register_chrdev_region(dev,1,"scull0");
	sema_init(&scull0_dev.sem,1);
	//DECLARE_MUTEX(scull0_dev.sem);
	//DEFINE_SEMAPHORE(&scull0_dev->sem);
	ret=alloc_chrdev_region(&dev,0,1,"scull0");
	if(ret!=0){
		printk("alloc_chrdev_region error!\n");
		goto unregister_dev;
	}
	cdev_init(&scull0_dev.cdev,&scull0_fops);
	scull0_dev.cdev.owner=THIS_MODULE;
	scull0_dev.cdev.ops=&scull0_fops;
	scull0_dev.quantum=QUANTUM;
	scull0_dev.qset=QSET;
	scull0_dev.size=0;
	ret=cdev_add(&scull0_dev.cdev,dev,1);
	if(ret!=0){
		printk("cdev_add error!\n");
		goto cdev_delete;
	}
	scull0_class = class_create(THIS_MODULE,DEV_NAME);
	if(IS_ERR(scull0_class)){
		printk("class create error!\n");
		return -1;
	}
	device_create(scull0_class,NULL, dev,NULL,DEV_NAME);
	printk(KERN_ERR "scull0_init\n");
	
	return 0;
cdev_delete:
	cdev_del(&scull0_dev.cdev);
unregister_dev:
	unregister_chrdev_region(MINOR(dev),1);
	
	return 0;
}
void scull0_exit(void)
{
	device_destroy(scull0_class, dev); 
	class_destroy(scull0_class);
	unregister_chrdev_region(MINOR(dev),1);
	cdev_del(&scull0_dev.cdev);
	printk(KERN_ERR "scull0_exit\n");
	return;
}
module_init(scull0_init);
module_exit(scull0_exit);
MODULE_LICENSE ("Dual BSD/GPL");
