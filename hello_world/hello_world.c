#include <linux/init.h>
#include <linux/module.h>


static int hello_init()
{
	printk(KERN_ERR "HELLO INIT!\n");
	return 0;
}

static void hello_exit()
{
	printk(KERN_ERR "HELLO EXIT!\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("Dual BSD/GPL");
