/**
 * @file gpio_driver.c
 * @brief GPIO driver for Linux kernel
 */

#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/workqueue.h>


/* GPIO pins */
#define PIN_A 526 // pin 14
#define PIN_B 527 // pin 15 
#define SAMPLE_BUFFER_SIZE 50

/* Globals */
static char sample_buffer_PIN_A[SAMPLE_BUFFER_SIZE];
static char sample_buffer_PIN_B[SAMPLE_BUFFER_SIZE];
static int sample_index = 0;
static int sample_count = 0;
static dev_t first;       // first device number 
static struct cdev c_dev; // character device structure 
static struct class *cl;  // device class 
static struct delayed_work my_work;    // Delayed work structure to poll GPIO periodically
static struct workqueue_struct *my_wq; // Workqueue structure 

static char last_value[8] = "0\n"; // Buffer to store the last read value
static int read_period = HZ/5;   //2 second delay 

/* ---------------- */
/* File operations */
/* ---------------- */

/**
 * @brief Release function for the device
 * @param inode Pointer to the inode structure
 * @param file Pointer to the file structure
 * @return 0 on success
 */
static int my_release(struct inode *inode, struct file *file) {
  printk("Closing signals sampler\n");
  return 0;
}

/**
 * @brief Open function for the device
 * @param i Pointer to the inode structure
 * @param f Pointer to the file structure
 * @return 0 on success
 */
static int my_open(struct inode *i, struct file *f) {
  printk("Opening signals sampler\n");
  return 0;
}

/**
 * 
 * Copies the sampled values to userspace.
 * @param file Pointer to the file structure
 * @param buf A pointer to the user-space buffer where the data should be copied to.
 * @param num_of_bytes Number of bytes to read
 * @param offset The current file offset; used to prevent multiple reads returning the same data.
 * @return Number of bytes read on success, negative value on failure
 */
ssize_t my_read(struct file *file, char __user *buf, size_t num_of_bytes,loff_t *offset) {

  char temp_buffer[2 * SAMPLE_BUFFER_SIZE] = {0};
  int i, index;

  if (*offset > 0)
    return 0; // Only allow one read per open

  for (i = 0; i < sample_count; i++) {
    index = (sample_index + SAMPLE_BUFFER_SIZE - sample_count + i) % SAMPLE_BUFFER_SIZE;
    temp_buffer[i] = sample_buffer_PIN_A[index];           // First signal
    temp_buffer[SAMPLE_BUFFER_SIZE + i] = sample_buffer_PIN_B[index]; // Second signal
  }

  if (copy_to_user(buf, temp_buffer, 2 * sample_count))
    return -EFAULT;

  *offset += 2 * sample_count;

  return 2 * sample_count;

}


/** File operations structure */
static struct file_operations my_fops = {
    .read = my_read,
    .open = my_open,
    .release = my_release,
};

/**
 * Called every second 200ms .Reads current GPIOs values and stores them .
 * Re-enqueues itself for continuous polling.
 * 
 * @param work Pointer to the work structure
 */
static void take_sample(struct work_struct *work) {

  u8 value_a = gpio_get_value(PIN_A);
  u8 value_b = gpio_get_value(PIN_B);

  sample_buffer_PIN_A[sample_index] = value_a ? '1' : '0';
  sample_buffer_PIN_B[sample_index] = value_b ? '1' : '0';

  sample_index = (sample_index + 1) % SAMPLE_BUFFER_SIZE;

  if (sample_count < SAMPLE_BUFFER_SIZE)
    sample_count++;

  queue_delayed_work(my_wq, &my_work, read_period);
}

/* ------------------- */
/* Lifecycle functions */
/* ------------------- */

/**
GPIO Setup:

    -Request IO_1 and IO_2 using gpio_request.
    -Set them as input using gpio_direction_input.

Character Device Setup:

    -Allocate a major/minor number with alloc_chrdev_region.
    -Create device class and /dev/signals_reader.
    -Register cdev with my_fops.

Workqueue Setup:

    -Create a single-threaded workqueue.
    -Initialize and schedule the first delayed_work. 



 * @return 0 on success, negative value on failure
 */
int __init my_init(void) {
  printk("Initializing ports\n");

  int status;
  int ret;
  struct device *dev_ret;

  // request GPIO pins

  status = gpio_request(PIN_A, "PIN_A");
  if (status != 0) {
    printk("Error requesting PIN_A\n");
    return status;
  }

  status = gpio_request(PIN_B, "PIN_B");
  if (status != 0) {
    printk("Error requesting PIN_B\n");
    gpio_free(PIN_A);
    return status;
  }

  // set pins as inputs
  status = gpio_direction_input(PIN_A);
  if (status != 0) {
    printk("Error setting direction input PIN_A\n");
    gpio_free(PIN_A);
    gpio_free(PIN_B);
    return status;
  }

  status = gpio_direction_input(PIN_B);
  if (status != 0) {
    printk("Error setting direction input PIN_B\n");
    gpio_free(PIN_A);
    gpio_free(PIN_B);
    return status;
  }

  
  /** 
   * Dynamically allocates a major number (and one minor number starting from 0) 
   * for the character device.
   
   * first: where the assigned device number will be stored.
   * 0: starting minor number.
   * 1: number of devices.
   * "signals_reader": name for /proc/devices.
  */
  if ((ret = alloc_chrdev_region(&first, 0, 1, "signals_reader")) < 0) {
    gpio_free(PIN_A);
    gpio_free(PIN_B);
    return ret;
  }

  // Creates a device class which will appear under /sys/class/signals_reader.
  // Purpose: This class is used later to create the actual device node in /dev/.
  if (IS_ERR(cl = class_create(THIS_MODULE, "signals_reader"))) {
    unregister_chrdev_region(first, 1);
    gpio_free(PIN_A);
    gpio_free(PIN_B);
    return PTR_ERR(cl);
  }

  
  /** 
   * Creates /dev/signals_reader and links it to your driver.
   * 
   * cl: the previously created class 
   * first: device number.
   * "signals_reader": the filename in /dev.
   * 
   * Result: Now user programs can open /dev/signals_reader and interact with the driver 
   * using read, write, etc.
  */
  if (IS_ERR(dev_ret = device_create(cl, NULL, first, NULL, "signals_reader"))) {
    class_destroy(cl);
    unregister_chrdev_region(first, 1);
    gpio_free(PIN_A);
    gpio_free(PIN_B);
    return PTR_ERR(dev_ret);
  }
  
  /**
   * cdev_init: Initializes the cdev structure with custom file_operations (read, write, etc.).
   * cdev_add: Adds the character device to the kernel so it can start handling syscalls.
   * If it fails it destroys the device node, class, unregister the device number,and free the GPIOs. 
   */ 

  cdev_init(&c_dev, &my_fops);
  if ((ret = cdev_add(&c_dev, first, 1)) < 0) {
    device_destroy(cl, first);
    class_destroy(cl);
    unregister_chrdev_region(first, 1);
    gpio_free(PIN_A);
    gpio_free(PIN_B);
    return ret;
  }

  // Create workqueue
  my_wq = create_singlethread_workqueue("my_wq");
  if (!my_wq) {
    cdev_del(&c_dev);
    device_destroy(cl, first);
    class_destroy(cl);
    unregister_chrdev_region(first, 1);
    gpio_free(PIN_A);
    gpio_free(PIN_B);
    return -ENOMEM;
  }

  // Initialize delayed work
  INIT_DELAYED_WORK(&my_work, take_sample);

  // Queue the first work
  queue_delayed_work(my_wq, &my_work, read_period);

  return 0;
}

/**
 * @brief Exit function
 */
void __exit my_exit(void) {
  printk("Goodbye! \n");

  // Cancel the delayed work and destroy the workqueue
  cancel_delayed_work_sync(&my_work);
  destroy_workqueue(my_wq);
  
  // Unregister device
  cdev_del(&c_dev);
  device_destroy(cl, first);
  class_destroy(cl);
  unregister_chrdev_region(first, 1);
  
  // free gpios
  gpio_free(PIN_A);
  gpio_free(PIN_B);
}

// register functions
module_init(my_init);
module_exit(my_exit);

/* Module information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Grupo:Epsilon");
MODULE_DESCRIPTION("Signal sampler driver");