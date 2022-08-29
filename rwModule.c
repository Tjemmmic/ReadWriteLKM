#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/types.h>

#define BUFFER_SIZE 255
#define DRIVER_NAME "rwModuleDev"
#define DRIVER_CLASS "rwModuleClass"

static char buffer[BUFFER_SIZE];
static int buffer_seek;
 
volatile int module_value = 0;
static dev_t dev = 0;
static struct class *dev_class;
static struct cdev my_cdev;

/*Linked List Node*/
struct buffer_node{
     struct list_head list;
     char data[BUFFER_SIZE];
     int size;
};
 
/*Head of the linked list*/
LIST_HEAD(head_node);

/* Prototypes */
static int __init ModuleInit(void);
static void __exit ModuleExit(void);
static int module_open(struct inode *inode, struct file *file);
static int module_close(struct inode *inode, struct file *file);
static ssize_t module_read(struct file *mod_file, char __user *buf, size_t len,loff_t * off);
static ssize_t module_write(struct file *mod_file, const char *buf, size_t len, loff_t * off);

/*
** This function is called when adding a node to the linked list
*/
static void node_add(void)
{
        struct buffer_node *temp_node = NULL;
 
        printk(KERN_INFO "Adding node to list\n");
        
        /*Creating Node*/
        temp_node = kmalloc(sizeof(struct buffer_node), GFP_KERNEL);
 
        /*Assgin the data that is received*/
        strscpy(temp_node->data, buffer, buffer_seek);
        temp_node->size = buffer_seek;
 
        /*Init the list within the struct*/
        INIT_LIST_HEAD(&temp_node->list);
 
        /*Add Node to Linked List*/
        list_add(&temp_node->list, &head_node);
}

/*
** File operation structure
*/
static struct file_operations fops =
{
        .owner          = THIS_MODULE,
        .read           = module_read,
        .write          = module_write,
        .open           = module_open,
        .release        = module_close,
};

/*
** This fuction is called when the Device file is opened
*/
static int module_open(struct inode *inode, struct file *file)
{
        printk(KERN_INFO "Device File Successfully Opened\n");
        return 0;
}

/*
** This fuction is called when the Device file is closed
*/   
static int module_close(struct inode *inode, struct file *file)
{
        printk(KERN_INFO "Device File Successfully Closed\n");
        return 0;
}

/*
** This fuction is called when Device file is read
*/
static ssize_t module_read(struct file *mod_file, char __user *user_buffer, size_t len, loff_t *off)
{
	/* Other variables */
    	int count = 0; //Node counter
        int to_copy; //Bytes copied in buffer
        int not_copied; //Bytes that could not be copied
    	int size_count = 0; //Progress of bytes copied to buffer
        struct buffer_node *temp; //Temporary node for seeking through list
        
        /* Temporary buffer for copying */
    	unsigned char* temp_data;
    	temp_data = kmalloc(BUFFER_SIZE*sizeof(char), GFP_KERNEL);
	
    	/* Determines if data was already read from the same user call */
    	if(*off > 0) {
        	return 0;
    	}
    	
    	printk(KERN_INFO "Read Function\n");
             
     	list_for_each_entry(temp, &head_node, list) {
        	/* Stop when buffer is full but don't cut off any entries */
        	if(size_count + temp->size > BUFFER_SIZE){
             		goto userPrint;
         	}
         	/* Copy entries into temporary buffer in reverse order for printing */
                memcpy(temp_data + BUFFER_SIZE - size_count - temp->size, temp->data, temp->size);
                size_count += temp->size;
                printk(KERN_INFO "Node %d Data = %s\n", count++, temp->data);
        }
 userPrint:
    	/* Limit data to size of the buffer */
    	to_copy = min(size_count, len);
    
    	/* Copy data to user */
    	not_copied = copy_to_user(user_buffer, temp_data + (BUFFER_SIZE - size_count), to_copy);
	
	/* Increase offset to stop read call */
    	(*off) += to_copy;
    	
    	/* Free memory and return bytes read */
    	kfree(temp_data);
    	return to_copy;
}

/*
** This fuction is called when Device file is written to
*/
static ssize_t module_write(struct file *mod_file, const char __user *user_buffer, size_t len, loff_t *off)
{
        int to_copy; //Bytes copied in buffer
        int not_copied; //Bytes that could not be copied
        
        printk(KERN_INFO "Write Function\n");

    	/* Limit copy length to the size of the buffer */
    	to_copy = min(len, sizeof(buffer));
    
    	/* Copy from user buffer */
    	not_copied = copy_from_user(buffer, user_buffer, to_copy);
    	buffer_seek = to_copy;
    	
    	/* Add newline for reading */
    	memcpy(buffer + to_copy, "\n", 1);
    	buffer_seek++;
    	
    	/* Add to linked list */
    	node_add();    
   	
   	/* Return number of bytes written*/
    	return to_copy;
}
 
/*
** Module Initialization Function
*/  
static int __init ModuleInit(void)
{
        /*Allocating Major number*/
        if((alloc_chrdev_region(&dev, 0, 1, DRIVER_NAME)) <0){
                printk(KERN_INFO "Could not allocate Major Number\n");
                return -1;
        }
        printk(KERN_INFO "Major = %d Minor = %d n",MAJOR(dev), MINOR(dev));
 
 	/*Creating struct class*/
        if((dev_class = class_create(THIS_MODULE,DRIVER_CLASS)) == NULL){
            printk(KERN_INFO "Could not create struct class\n");
            goto error_class;
        }
        
        /*Creating device*/
        if((device_create(dev_class,NULL,dev,NULL,DRIVER_NAME)) == NULL){
            printk(KERN_INFO "Could not create device\n");
            goto error_device;
        }
 	
        /*Creating cdev structure*/
        cdev_init(&my_cdev,&fops);
 
        /*Adding character device to the system*/
        if((cdev_add(&my_cdev,dev,1)) < 0){
            printk(KERN_INFO "Could not register/add device\n");
            goto error_cdev;
        }
	
        printk(KERN_INFO "Kernel Module Loaded Successfully...\n");
        return 0;

	/* Error cleanup and return*/
error_cdev:
	device_destroy(dev_class, dev);
	cdev_del(&my_cdev);
error_device:
        class_destroy(dev_class);
error_class:
        unregister_chrdev_region(dev,1);
        return -1;
}

/*
** Module Exit function
*/
static void __exit ModuleExit(void)
{
 
        /* Go through the list and free the memory. */
        struct buffer_node *seek;
        struct buffer_node *temp;
        list_for_each_entry_safe(seek, temp, &head_node, list) {
            list_del(&seek->list);
            kfree(seek);
        }
 	
 	/* Unregister and delete device, class, etc. */
        device_destroy(dev_class,dev);
        class_destroy(dev_class);
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev, 1);
        printk(KERN_INFO "Kernel Module Unloaded Successfully...\n");
}
 
module_init(ModuleInit);
module_exit(ModuleExit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Donovan Tjemmes <michaeltdonovan@yahoo.com>");
MODULE_DESCRIPTION("Proc Entry with Read and Write using Linked List");
MODULE_VERSION("1.0");
