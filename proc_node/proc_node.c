#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>

#include <linux/proc_fs.h>// for the proc filesystem
#include <linux/seq_file.h>// for sequence files
#include <linux/jiffies.h>// for jiffies
#define MAX_COOKIE_SIZE PAGE_SIZE


static struct proc_dir_entry *proc_entry;
static char *cookie;
static int cookie_index=0;
static int read_pos=0;

ssize_t proc_write(struct file *filp,const char __user *buff,unsigned long len,void *data)
{
	int available_space=MAX_COOKIE_SIZE-cookie_index+1;
	printk("%s\n",__func__);
	if(len>available_space){
		printk("no space to write!\n");
		return -ENOSPC;
	}
	if(copy_from_user(&cookie[cookie_index],buff,len)){
		return -EFAULT;
	}
	printk("%s\n",&cookie[cookie_index]);
	cookie_index+=len;
	cookie[cookie_index-1]=0;
	return len;

}

ssize_t proc_read(char *page,char **start,off_t off,int count,int *eof,void *data)
{
	int len;
	printk("%s\n",__func__);
	if(off>0){
		*eof=1;
		return 0;
	}
	if(read_pos>=cookie_index) read_pos=0;
	len=sprintf(page,"%s\n",&cookie[read_pos]);
	read_pos+=len;
	return len;
}



static int proc_init()
{
	int ret=0;
	printk(KERN_ERR "HELLO INIT!\n");
	cookie=(char *)vmalloc(MAX_COOKIE_SIZE);
	if(cookie==NULL){
		printk("create cookie error!\n");
		ret=-ENOMEM;
	}
	proc_entry=create_proc_entry("cookie_test",0666,NULL);
	if(proc_entry==NULL){
		printk("create_proc_entry error!\n");
		ret=-ENOMEM;
		vfree(cookie);
	}else{
		cookie_index=0;
		read_pos=0;
		proc_entry->read_proc=proc_read;
		proc_entry->write_proc=proc_write;
		//proc_entry->owner = THIS_MODULE;
		printk("proc init success!\n");
	}

	return ret;
}

static void proc_exit()
{
	remove_proc_entry(proc_entry,NULL);
	vfree(cookie);
	printk(KERN_ERR "HELLO EXIT!\n");

}

module_init(proc_init);
module_exit(proc_exit);

MODULE_LICENSE("Dual BSD/GPL");
