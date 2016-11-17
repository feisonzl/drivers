#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/device.h>
#include <linux/jiffies.h>// for jiffies
#include <linux/kernel.h>
#define MAX_COOKIE_SIZE PAGE_SIZE


/* class start */
static struct class sys_class={
	.name = "sys_class",
};
static ssize_t sys_class_show(struct class *class, struct class_attribute *attr,char *buf)
{
	printk("sys_class_show!\n");
	snprintf(buf,PAGE_SIZE,"sys_class_show!\n");
	return strlen(buf);

}
static ssize_t sys_class_store(struct class *class, struct class_attribute *attr,const char *buf, size_t count)
{
		printk("%s\n",buf);
		return count;
}
static CLASS_ATTR(sys_class,S_IRUGO|S_IWUSR,sys_class_show,sys_class_store);
/* class end */


/* bus type start */
int ldd_match(struct device *dev, struct device_driver *drv)
{
	return !strcmp(dev->init_name,drv->name);
}
int ldd_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	if(add_uevent_var(env,"LDD-VERSION:%s\n","V1.0"))
		return -ENOMEM;
	
	return 0;
}

struct bus_type ldd_bus_type={
	.name = "ldd",
	.match = ldd_match,
	.uevent = ldd_uevent,
};

static ssize_t ldd_bus_show(struct bus_type *bus,char *buf)
{
	printk("ldd_bus_type show!\n");
	return sprintf(buf,"ldd_bus_type show!\n");
}
static BUS_ATTR(ldd_bus,S_IRUGO,ldd_bus_show,NULL);

/* bus type end */


/* device start */
static void ldd_bus_release(struct device *dev)
{
	printk("ldd_bus_release!\n");
}

struct device ldd_bus={
	.init_name="ldd0",
	.release = ldd_bus_release,
	.class = &sys_class,
};

static ssize_t sys_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	printk("sys_show!\n");
	snprintf(buf,PAGE_SIZE,"sys_show!\n");
	return strlen(buf);
}
static ssize_t sys_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
		printk("%s\n",buf);
		return count;
}
static DEVICE_ATTR(sys_node, S_IRUGO | S_IWUSR, sys_show, sys_store);
/* device end */

/* driver start */
static struct device_driver sys_driver={
	.name = "sys_driver",
	.bus = &ldd_bus_type,
};

static ssize_t sys_driver_show(struct device_driver *driver, char *buf)
{
	printk("sys_driver_show!\n");
	snprintf(buf,PAGE_SIZE,"sys_driver_show!\n");
	return strlen(buf);

}
static ssize_t sys_driver_store(struct device_driver *driver, const char *buf,size_t count)
{
		printk("%s\n",buf);
		return count;

}
static DRIVER_ATTR(sys_driver,S_IRUGO|S_IWUSR,sys_driver_show,sys_driver_store);
/* driver end */



static int __init sys_init(void)
{
	int ret=0;
	ret = class_register(&sys_class);
	if(ret)
		printk("Unable to register sys_class!\n");
	ret = class_create_file(&sys_class,&class_attr_sys_class);

	ret = bus_register(&ldd_bus_type);
	if(ret)
		printk("Unable to register ldd_bus_type!\n");
	ret = bus_create_file(&ldd_bus_type,&bus_attr_ldd_bus);

	ret = device_register(&ldd_bus);
	if(ret)
		printk("Unable to register ldd_bus!\n");
	ret = device_create_file(&ldd_bus,&dev_attr_sys_node);
	ret = driver_register(&sys_driver);
	if(ret)
		printk("Unable to register sys_driver!\n");
	ret = driver_create_file(&sys_driver,&driver_attr_sys_driver);
	printk(KERN_ERR "HELLO INIT!\n");
	return ret;
}

static void __exit sys_exit(void)
{
	printk(KERN_ERR "HELLO EXIT!111111111111111111\n");
	//device_remove_file(&ldd_bus,&dev_attr_sys_node);
	driver_unregister(&sys_driver);
	printk(KERN_ERR "HELLO EXIT!222222222222222222\n");
	device_unregister(&ldd_bus);
	printk(KERN_ERR "HELLO EXIT!333333333333333333\n");
	bus_unregister(&ldd_bus_type);
	class_unregister(&sys_class);
}

module_init(sys_init);
module_exit(sys_exit);

MODULE_LICENSE("Dual BSD/GPL");
