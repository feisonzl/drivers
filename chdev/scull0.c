//#include <unistd.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <asm/div64.h>
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
	filp->private_data=dev;

	if((filp->f_flags&O_ACCMODE)==O_WRONLY){
		scull0_trim(scull0_dev);
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
	while(1){
		scull0_qset=scull0_dev->data;
		if(scull0_qset==NULL)
			break;
		if(page_rest==0)
			break;
		scull0_qset=scull0_qset->next;
		page_rest-=1;
	}
	
	return scull0_qset;

}
ssize_t scull0_read(struct file *filp,char __user *buff,size_t count,loff_t *offp)
{
	t_scull0 *scull0_dev=filp->private_data;
	t_scull0_qset *scull0_qset;
	int ret;
	long long off=*offp;
	int qset=scull0_dev->qset;
	int quantum=scull0_dev->quantum;
	int pagesize=qset*quantum;
	int page_off=do_div(off,pagesize);
	int page_rest=off;
	int set_off=do_div(page_off,quantum);
	int set_rest=page_off;
	if(*offp >scull0_dev->size)
		goto out;
	if(*offp+count >scull0_dev->size)
		count=scull0_dev->size-*offp;
	scull0_qset=scull0_fllow(scull0_dev,page_rest);//未实现该函数
	if(scull0_qset==NULL||!scull0_qset->data||scull0_qset->data[set_rest])
		goto out;
	if(count>quantum-set_off)
		count=quantum-set_off;
	
	if(copy_to_user(buff,scull0_qset->data[set_rest]+set_off,count)){
		ret=-EFAULT;
		goto out;
	}
	offp+=count;
	return count;
out:
	up(&scull0_dev->sem);
	return 0;
	
}
ssize_t scull0_write(struct file *filp,char __user *buff,size_t count,loff_t *offp)
{
	t_scull0 *scull0_dev=filp->private_data;
	t_scull0_qset *scull0_qset;
	int ret;
	long long off=*offp;
	int qset=scull0_dev->qset;
	int quantum=scull0_dev->quantum;
	int pagesize=qset*quantum;
	int page_off=do_div(off,pagesize);
	int page_rest=off;
	int set_off=do_div(page_off,quantum);
	int set_rest=page_off;
	if(*offp >scull0_dev->size)
		goto out;
	if(*offp+count >scull0_dev->size)
		count=scull0_dev->size-*offp;
	scull0_qset=scull0_fllow(scull0_dev,page_rest);//未实现该函数
	if(scull0_qset==NULL)
		goto out;
	if(!scull0_qset->data){
		scull0_qset->data=kmalloc(qset*sizeof(char *),GFP_KERNEL);
		if(!scull0_qset->data)
			goto out;
		memset(scull0_qset->data,0,qset*sizeof(char *));
	}	
	if(!scull0_qset->data[set_off]){
		scull0_qset->data[set_off]=kmalloc(quantum,GFP_KERNEL);
		if(!scull0_qset->data[set_off])
			goto out;
		memset(scull0_qset->data[set_off],0,quantum);
	}	
	
	if(count>quantum-set_off)
		count=quantum-set_off;
	
	if(copy_from_user(scull0_qset->data[set_rest]+set_off,buff,count)){
		ret=-EFAULT;
		goto out;
	}
	offp+=count;
	if(scull0_dev->size<*offp)
		scull0_dev->size=*offp;
	return count;
out:
	up(&scull0_dev->sem);
	return 0;
	
}
struct file_operations scull0_fops={
	.owner=THIS_MODULE,
	.open=scull0_open,
	.release=scull0_release,
	.read=scull0_read,
	.write=scull0_write,
};
int scull0_init(void)
{	
	t_scull0 scull0_dev;
	//dev=MKDEV("scull0",0);
	//register_chrdev_region(dev,1,"scull0");
	ret=alloc_chrdev_region(&dev,0,1,"scull0");
	if(ret!=0){
		printk("alloc_chrdev_region error!\n");
		goto unregister_dev;
	}
	cdev_init(&scull0_dev.cdev,&scull0_fops);
	scull0_dev.cdev.owner=THIS_MODULE;
	scull0_dev.cdev.ops=&scull0_fops;
	ret=cdev_add(&scull0_dev.cdev,dev,1);
	if(ret!=0){
		printk("cdev_add error!\n");
		goto cdev_delete;
	}
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
	unregister_chrdev_region(MINOR(dev),1);
	printk(KERN_ERR "scull0_exit\n");
	return;
}
module_init(scull0_init);
module_exit(scull0_exit);
MODULE_LICENSE ("Dual BSD/GPL");
