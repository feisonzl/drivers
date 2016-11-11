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

static int seq_proc_show(struct seq_file *s,void *v)
{
	return seq_printf(s,"%s\n","it is a test!");
}

static struct seq_operations seq_seq_ops={
	.show=seq_proc_show,	
};

static int seq_proc_open(struct inode *inode,struct file *filp)
{
	//return seq_open(filp,&seq_seq_ops);
	return single_open(filp,seq_proc_show,NULL);
}
static struct file_operations seq_ops={
	.owner=THIS_MODULE,
	.open = seq_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

static int proc_init()
{
	int ret=0;
	printk(KERN_ERR "HELLO INIT!\n");

	/* you can create proc node in one of 2 ways */
	/*
	 * 1.
	 * proc_entry=create_proc_entry("cookie_test",0666,NULL);
	 * if(proc_entry)
	 * 		proc_entry->proc_fops=&seq_ops;
	 * 2.
	 * proc_entry=create_proc("cookie_test",0666,NULL,&seq_ops);
	 * */
	proc_entry=proc_create("proc_seq",0666,NULL,&seq_ops);
	
	if(proc_entry==NULL){
		printk("create_proc_entry error!\n");
		ret=-ENOMEM;
	}else{
		printk("proc init success!\n");
	}

	return ret;
}

static void proc_exit()
{
	remove_proc_entry(proc_entry,NULL);
	printk(KERN_ERR "HELLO EXIT!\n");

}

module_init(proc_init);
module_exit(proc_exit);

MODULE_LICENSE("Dual BSD/GPL");
