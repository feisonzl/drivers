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
#include "scullc.h"
#include <linux/sched.h>
#include <linux/tty.h>

static dev_t dev;
int ret;

t_scullc scullc_dev;
struct class *scullc_class;


int scullc_trim(t_scullc *scullc_dev);
/*
 *owner by oneself alone
 *
 */

t_scullc scullc_s_device;
static atomic_t scullc_s_available=ATOMIC_INIT(1);
static int scullc_s_open(struct inode *inode,struct file *filp)
{
	t_scullc* 	dev=&scullc_s_device;
	if(!atomic_dec_and_test(&scullc_s_available)){
		atomic_inc(&scullc_s_available);
		return -EBUSY;
	}
	if((filp->f_flags&O_ACCMODE)==O_WRONLY){
		scullc_trim(dev);

	}
	filp->private_data=dev;
	return 0;
}
static int scullc_s_release(struct inode *inode,struct file *filp)
{
	atomic_inc(&scullc_s_available);
	return 0;

}

struct file_operations scullc_s_fops={
	.open=scullc_s_open,
	.release=scullc_s_release,
};

/*
 *just one user could access at the same time
 *
 */

t_scullc scullc_u_device;
static int scullc_u_count=0;
static int scullc_u_owner=0;
static spinlock_t scullc_u_lock=SPIN_LOCK_UNLOCKED;

static int scullc_u_open(struct inode *inode,struct file *filp)
{
	t_scullc* dev=&scullc_u_device;
	spin_lock(&scullc_u_lock);	
	if(scullc_u_count&&
		(scullc_u_owner!=current->uid)&&
		(scullc_u_owner!=current->euid)&&
		(!capable(CAP_DAC_OVERRIDE))){
		spin_unlock(&scullc_u_lock);
		return -EBUSY;
	}
	if(scullc_u_count==0)
		scullc_u_owner=current->uid;
	scullc_u_count++;
	spin_unlock(&scullc_u_lock);
}
static int scullc_u_release(struct inode *inode,struct file *filp)
{
	spin_lock(&scullc_u_lock);
	scullc_u_count--;
	spin_unlock(&scullc_u_lock);
	return 0;

}
struct file_operations scullc_u_fops={
	.open=scullc_u_open,
	.release=scullc_u_release,
};


/*
 *when device is busy,wait a minute then check
 *
 *
 */

t_scullc scullc_w_device;
static int scullc_w_count=0;
static int scullc_w_owner=0;
static spinlock_t scullc_w_lock=SPIN_LOCK_UNLOCKED;
//static wait_queue_t scullc_w_wait;
DECLARE_WAIT_QUEUE_HEAD(scullc_w_wait);

static int scullc_w_available(void)
{
	if(scullc_w_count&&
		(scullc_w_owner!=current->uid)&&
		(scullc_w_owner!=current->euid)&&
		(!capable(CAP_DAC_OVERRIDE)))return -1;
		return 0;
}
static int scullc_w_open(struct inode *inode,struct file *filp)
{
	t_scullc* dev=&scullc_w_device;
	spin_lock(&scullc_w_lock);	
	while(scullc_w_available()<0){
		spin_unlock(&scullc_w_lock);
		//return -EBUSY;
		if(filp->f_flags&O_NONBLOCK) return -EAGAIN;
		if(wait_event_interruptible(scullc_w_wait,(scullc_w_available()==0)))
			return -ERESTARTSYS;
		spin_lock(&scullc_w_lock);
	}
	if(scullc_w_count==0)
		scullc_w_owner=current->uid;
	scullc_w_count++;
	spin_unlock(&scullc_w_lock);
}
static int scullc_w_release(struct inode *inode,struct file *filp)
{
	int tmp;
	spin_lock(&scullc_w_lock);
	scullc_w_count--;
	tmp=scullc_w_count;
	spin_unlock(&scullc_w_lock);
	if(tmp==0)
		wake_up_interruptible_sync(&scullc_w_wait);
	return 0;

}

struct file_operations scullc_w_fops={
	.open=scullc_w_open,
	.release=scullc_w_release,
};

/*
 *copy devices when opening 
 */

t_scullc scullc_c_device;
static LIST_HEAD(scullc_c_list);
static spinlock_t scullc_c_lock=SPIN_LOCK_UNLOCKED;
typedef struct scullc_listitem{
	t_scullc device;
	dev_t key;
	struct list_head list;
} t_scullc_listitem;

static t_scullc* scullc_c_lookfor_device(dev_t key)
{
	t_scullc_listitem *listitem;
	list_for_each_entry(listitem,&scullc_c_list,list){
		if(listitem->key==key)
			return &(listitem->device);
	}
	listitem=kmalloc(sizeof(t_scullc_listitem),GFP_KERNEL);
	if(listitem==NULL)
		return NULL;
	memset(listitem,0,sizeof(t_scullc_listitem));
	listitem->key=key;
	scullc_trim(&(listitem->device));
	init_MUTEX(&(listitem->device.sem));
	list_add(&listitem->list,&scullc_c_list);
	return &(listitem->device);
}

static int scullc_c_open(struct inode* inode,struct file* filp)
{
	t_scullc *dev;
	dev_t key;
	if(!current->signal->tty){
		return -EINVAL;
	}
	key=tty_devnum(current->signal->tty);
	spin_lock(&scullc_c_lock);
	dev=scullc_c_lookfor_device(key);
	spin_unlock(&scullc_c_lock);
	if(dev==NULL)
		return -ENOMEM;
	if((filp->f_flags&O_ACCMODE)==O_WRONLY)
		scullc_trim(dev);
	filp->private_data=dev;
	return 0;

}

static int scullc_c_release(struct inode * inode ,struct file* filp)
{

	return 0;
}

struct file_operations scullc_c_fops={
	.open=scullc_c_open,
	.release=scullc_c_release,

};

/*
 *scullc_x_init & scullc_x_exit
 *
 * 
 */
struct scullc_x_devinfo{
	char name[50];
	t_scullc *device;
	struct file_operations *fops;
} scullc_x_devinfos[]={
	{.name="scullc_s",.device=&scullc_s_device,.fops=&scullc_s_fops},
	{.name="scullc_u",.device=&scullc_u_device,.fops=&scullc_u_fops},
	{.name="scullc_w",.device=&scullc_w_device,.fops=&scullc_w_fops},
	{.name="scullc_c",.device=&scullc_c_device,.fops=&scullc_c_fops}
};

int scullc_x_setup(dev_t devnum,struct scullc_x_devinfo * devinfo)
{	
	t_scullc *scullc_dev=devinfo->device;
	struct file_operations *fops=devinfo->fops;
	char *dev_name=devinfo->name;
	sema_init(&scullc_dev->sem,1);
	
	/*
	ret=alloc_chrdev_region(&dev,0,1,"scullc");
	if(ret!=0){
		printk("alloc_chrdev_region error!\n");
		return -1;
	}
	*/
	cdev_init(&scullc_dev->cdev,fops);
	scullc_dev->cdev.owner=THIS_MODULE;
	scullc_dev->cdev.ops=fops;
	scullc_dev->quantum=QUANTUM;
	scullc_dev->qset=QSET;
	scullc_dev->size=0;
	kobject_set_name(&scullc_dev->cdev.kobj,dev_name);
	ret=cdev_add(&scullc_dev->cdev,devnum,1);
	if(ret!=0){
		printk("cdev_add error!\n");
		goto kobj_put;
	}
	scullc_class = class_create(THIS_MODULE,dev_name);
	if(IS_ERR(scullc_class)){
		printk("class create error!\n");
		goto cdev_delete;
	}
	device_create(scullc_class,NULL, dev,NULL,dev_name);
	printk(KERN_ERR "scullc_init\n");
	
	return 0;
cdev_delete:
	cdev_del(&scullc_dev->cdev);
kobj_put:
	kobject_put(&scullc_dev->cdev.kobj);

	return 0;
}

static int scullc_x_init(dev_t majordev)
{
	int ret,i;
	int num=sizeof(scullc_x_devinfos)/sizeof(scullc_x_devinfos[0]);
	ret=register_chrdev_region(majordev,num,"scullc_x");
	if(ret!=0)
		return -1;
	for(i=0;i<num;i++){
		scullc_x_setup(majordev+i,scullc_x_devinfos+i);
	}
	return num;
}

/*
 *common operations
 *
 */

int scullc_trim(t_scullc *scullc_dev)
{
	t_scullc_qset *next,*dptr;
	int qset,quantum;
	qset=scullc_dev->qset;
	quantum=scullc_dev->quantum;
	int i;
	for(dptr=scullc_dev->data;dptr;dptr=next){
		if(dptr->data){
			for(i=0;i<qset;i++)
				kfree(dptr->data[i]);
		}
		next=dptr->next;
		kfree(dptr->data);
	}
	scullc_dev->data=NULL;
	scullc_dev->quantum=quantum;
	scullc_dev->qset=qset;
	scullc_dev->size=0;
	return 0;
}
int scullc_open(struct inode *inode,struct file *filp)
{
	t_scullc *scullc_dev;
	scullc_dev=container_of(inode->i_cdev,t_scullc,cdev);
	filp->private_data=scullc_dev;

	if((filp->f_flags&O_ACCMODE)==O_WRONLY){
		//scullc_trim(scullc_dev);
	}
	return 0;
}
int scullc_release(struct inode *inode,struct file *filp)
{
	return 0;
}
t_scullc_qset *scullc_fllow(t_scullc * scullc_dev,int page_rest)
{	
	int tmpnum=page_rest;
	t_scullc_qset *scullc_qset=NULL;
	if(scullc_dev==NULL)
		return NULL;
	scullc_qset=scullc_dev->data;
	//printk("scullc:%x\n",scullc_qset);
	if(scullc_qset==NULL){
		scullc_qset=scullc_dev->data=kmalloc(sizeof(t_scullc_qset),GFP_KERNEL);
		if(scullc_qset==NULL){
			printk("kmalloc scullc_qset error!\n");
			return NULL;
		}
		memset(scullc_qset,0,sizeof(t_scullc_qset));
	}
	//printk("scullc:%x scullc_dev->data:%x\n",scullc_qset,scullc_dev->data);
	while(tmpnum--){
		scullc_qset=scullc_qset->next;
		if(scullc_qset==NULL)
			scullc_qset=kmalloc(sizeof(t_scullc_qset),GFP_KERNEL);
		if(scullc_qset==NULL){
			printk("kmalloc scullc_qset error in while!\n");
			return NULL;
		}
		memset(scullc_qset,0,sizeof(t_scullc_qset));
		//printk("scullc:%x scullc_dev->data:%x\n",scullc_qset,scullc_dev->data);
	}
	
	return scullc_qset;

}
ssize_t scullc_read(struct file *filp,char __user *buff,size_t count,loff_t *offp)
{
	t_scullc *scullc_dev=filp->private_data;
	t_scullc_qset *scullc_qset;

	int ret=0;
	long long off=*offp;
	//printk("scullc_dev *offp:%d\n",*offp);
	*offp=0;
	int qset=scullc_dev->qset;
	int quantum=scullc_dev->quantum;
	int pagesize=qset*quantum;
	//printk("scullc_dev qset:%d quantum:%d\n",qset,quantum);
	/*
	int page_rest=do_div(*offp,pagesize);
	int page_off=*offp;
	int set_rest=do_div(page_rest,quantum);
	printk("scullc_dev qset:%d quantum:%d page_rest:%d page_off:%d\n",qset,quantum,page_rest,page_off);
	int set_off=page_rest;
	printk("set_rest:%d\n",set_rest);
	set_rest=(set_rest>=4000)?(set_rest-4000):set_rest;
	printk("set_rest:%d\n",set_rest);
	printk("scullc_read scullc_dev:%x count:%d size:%d *offp:%d quantum:%d set_rest:%d\n",\
			scullc_dev,count,scullc_dev->size,*offp,quantum,set_rest);

	*/
	int page_off,page_rest,set_off,set_rest;
	page_off=div_u64_rem(*offp,pagesize,&page_rest);
	set_off=div_u64_rem(page_rest,quantum,&set_rest);
	
	if(down_interruptible(&scullc_dev->sem))
		return -ERESTARTSYS;
#if 1

	if(*offp >scullc_dev->size)
		goto out;
	if(*offp+count >scullc_dev->size)
		count=scullc_dev->size-*offp;
	

	scullc_qset=scullc_fllow(scullc_dev,page_off);//未实现该函数
	if(scullc_qset==NULL||!scullc_qset->data||!scullc_qset->data[set_off]){
		printk("scullc_qset==NULL||!scullc_qset->data||!scullc_qset->data[set_rest]\n");
		goto out;
	}
	//printk("set_rest:%d\n",set_rest);
#if 1

	if(count>scullc_dev->quantum-set_rest)
		count=scullc_dev->quantum-set_rest;
	
	if(copy_to_user(buff,scullc_qset->data[set_off]+set_rest,count)){
		ret=-EFAULT;
		goto out;
	}
	*offp+=count;
	ret=count;
//	printk("count:%d ret:%d set_rest:%d set_off:%d page_rest:%d size:%d\n",\
		count,ret,set_rest,set_off,page_rest,scullc_dev->size);
#endif 
#endif
	//copy_to_user(buff,scullc_qset->data[0],count);
out:
	up(&scullc_dev->sem);
	return ret;
	
}
ssize_t scullc_write(struct file *filp,const char __user *buff,size_t count,loff_t *offp)
{
	t_scullc *scullc_dev=filp->private_data;
	t_scullc_qset *scullc_qset;
	int ret= -ENOMEM;
	long long off=*offp;
	int qset=scullc_dev->qset;
	int quantum=scullc_dev->quantum;
	int pagesize=qset*quantum;
	int page_rest=do_div(off,pagesize);
	int page_off=off;
	int set_rest=do_div(page_rest,quantum);
	int set_off=page_rest;
	char buf[100]={0};
	if(down_interruptible(&scullc_dev->sem))
		return -ERESTARTSYS;
	//printk("scullc_write scullc_dev:%x\n",scullc_dev);
#if 1
	scullc_qset=scullc_fllow(scullc_dev,page_off);//未实现该函数
	if(scullc_qset==NULL){
		printk("scullc_qset is NULL!\n");
		goto out;
	}
	if(!scullc_qset->data){
		scullc_qset->data=kmalloc(qset*sizeof(char *),GFP_KERNEL);
		if(!scullc_qset->data){
			printk("scullc_qset->data is NULL!\n");
			goto out;
		}
		memset(scullc_qset->data,0,qset*sizeof(char *));
	}
	if(!scullc_qset->data[set_off]){
		scullc_qset->data[set_off]=kmalloc(quantum,GFP_KERNEL);
		if(!scullc_qset->data[set_off]){
			printk("scullc_qset->data[set_off] is NULL!\n");
			goto out;
		}
		memset(scullc_qset->data[set_off],0,quantum);
	}
	printk("count:%d quantum:%d set_off:%d set_rest:%d\n",count,quantum,set_off,set_rest);
	if(count>quantum-set_off)
		count=quantum-set_off;

	//copy_from_user(buf,buff,count);
	//printk("copy_from_user(buf,buff,count):%d\n",ret);
	if(copy_from_user(scullc_qset->data[set_off]+set_rest,buff,count)){
		ret=-EFAULT;
		goto out;
	}
	printk("%s\n",(char *)(scullc_qset->data[set_rest]+set_off));
	/*
	int i;
	char *tmpbuf=scullc_qset->data[set_off]+set_rest;
	for(i=0;i<count;i++){
		printk("%c\n",*(char *)(scullc_qset->data[set_rest]+set_off+i));
		*(tmpbuf+i)='c';
		printk("tmpbuf:%c\n",*(tmpbuf+i));
	}
	memcpy((char*)(scullc_qset->data[set_off]+set_rest),buf,count);
	for(i=0;i<count;i++){
		printk("%x\n",*(char *)(scullc_qset->data[set_rest]+set_off+i));
		printk("%s\n",(char *)(scullc_qset->data[set_rest]+set_off));
		//*(tmpbuf+i)='c';
		//printk("tmpbuf:%c\n",*(tmpbuf+i));
	}
	printk("\n");
	printk("buf:%s\n",buf);
	*/
	*offp+=count;
	if(scullc_dev->size<*offp)
		scullc_dev->size=*offp;
	ret=count;
	//printk("count:%d ret:%d set_rest:%d set_off:%d page_rest:%d size:%d scullc_dev->data:%x\n",\
	//	count,ret,set_rest,set_off,page_rest,scullc_dev->size,scullc_dev->data);
#endif
	
out:
	up(&scullc_dev->sem);
	return ret;
	
}
int scullc_ioctl(struct inode *inode, struct file *filp,unsigned int cmd, unsigned long arg)
{
	int err=0,tmp;
	int ret=0;
	int scullc_quantum=0,scullc_qset=0;
	if(_IOC_TYPE(cmd)!=SCULLC_IOC_MAGIC) return -ENOTTY;
	if(_IOC_NR(cmd)>SCULLC_IOC_MAXNR) return -ENOTTY;
	
	if(_IOC_DIR(cmd)&_IOC_READ)
		err=!access_ok(VERIFY_WRITE,(void __user *)arg,_IOC_SIZE(cmd));
	else if(_IOC_DIR(cmd)&_IOC_WRITE)
		err=!access_ok(VERIFY_READ,(void __user *)arg,_IOC_SIZE(cmd));
	if(err)
		return -EFAULT;
	switch(cmd){
		case SCULLC_IOCSQUANTUM :
			if(!capable(CAP_SYS_ADMIN))
				return -EPERM;
			ret=__get_user(scullc_quantum,(int __user*)arg);
			break;
		case SCULLC_IOCSQSET   :
			scullc_quantum=QUANTUM;
			scullc_qset=QSET;
			break;
		case SCULLC_IOCTQUANTUM:
			if(!capable(CAP_SYS_ADMIN))
				return -EPERM;
			scullc_quantum=arg;
			break;
		case SCULLC_IOCGQUANTUM:
			ret=__put_user(scullc_quantum,(int __user *)arg);
			break;
		case SCULLC_IOCQQUANTUM:
			return scullc_quantum;
		case SCULLC_IOCXQUANTUM:
			if(!capable(CAP_SYS_ADMIN))
				return -EPERM;
			tmp=scullc_quantum;
			ret=__get_user(scullc_quantum,(int __user*)arg);
			if(ret==0)
				ret=__put_user(tmp,(int __user*)arg);
			break;
		case SCULLC_IOCHQUANTUM:
			if(!capable(CAP_SYS_ADMIN))
				return -EPERM;
			tmp=scullc_quantum;
			scullc_quantum=arg;
			return arg;
		default:
			return -ENOTTY;
	}
	return 0;
}
loff_t scullc_llseek(struct file *filp,loff_t off,int whence)
{
	t_scullc *dev=filp->private_data;
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

struct file_operations scullc_fops={
	.owner=THIS_MODULE,
	.open=scullc_open,
	.release=scullc_release,
	.read=scullc_read,
	.write=scullc_write,
	.ioctl=scullc_ioctl,
};


int scullc_init(void)
{	
	//t_scullc scullc_dev;
	//dev=MKDEV("scullc",0);
	//register_chrdev_region(dev,1,"scullc");
	sema_init(&scullc_dev.sem,1);
	//DECLARE_MUTEX(scullc_dev.sem);
	//DEFINE_SEMAPHORE(&scullc_dev->sem);
	ret=alloc_chrdev_region(&dev,0,1,"scullc");
	if(ret!=0){
		printk("alloc_chrdev_region error!\n");
		goto unregister_dev;
	}
	cdev_init(&scullc_dev.cdev,&scullc_fops);
	scullc_dev.cdev.owner=THIS_MODULE;
	scullc_dev.cdev.ops=&scullc_fops;
	scullc_dev.quantum=QUANTUM;
	scullc_dev.qset=QSET;
	scullc_dev.size=0;
	ret=cdev_add(&scullc_dev.cdev,dev,1);
	if(ret!=0){
		printk("cdev_add error!\n");
		goto cdev_delete;
	}
	scullc_class = class_create(THIS_MODULE,DEV_NAME);
	if(IS_ERR(scullc_class)){
		printk("class create error!\n");
		return -1;
	}
	device_create(scullc_class,NULL, dev,NULL,DEV_NAME);
	printk(KERN_ERR "scullc_init\n");
	
	return 0;
cdev_delete:
	cdev_del(&scullc_dev.cdev);
unregister_dev:
	unregister_chrdev_region(MINOR(dev),1);
	
	return 0;
}
void scullc_exit(void)
{
	device_destroy(scullc_class, dev); 
	class_destroy(scullc_class);
	unregister_chrdev_region(MINOR(dev),1);
	cdev_del(&scullc_dev.cdev);
	printk(KERN_ERR "scullc_exit\n");
	return;
}
module_init(scullc_init);
module_exit(scullc_exit);
MODULE_LICENSE ("Dual BSD/GPL");
