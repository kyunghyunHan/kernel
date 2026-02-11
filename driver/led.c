#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define LED_GPIO 17  // 실제 Linux GPIO 번호
#define DEV_NAME "my_led"

static int major;

static int my_open(struct inode *inode, struct file *file) {
    gpio_request(LED_GPIO, "LED");
    gpio_direction_output(LED_GPIO, 0); // LED 초기 OFF
    return 0;
}

static ssize_t my_write(struct file *file, const char __user *buf, size_t len, loff_t *off) {
    char kbuf[2];
    if (copy_from_user(kbuf, buf, 1)) return -EFAULT;

    if (kbuf[0] == '1') gpio_set_value(LED_GPIO, 1); // LED 켬
    if (kbuf[0] == '0') gpio_set_value(LED_GPIO, 0); // LED 끔

    return len;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = my_open,
    .write = my_write,
};

static int __init my_init(void) {
    major = register_chrdev(0, DEV_NAME, &fops);
    printk("my_led driver loaded, major=%d\n", major);
    return 0;
}

static void __exit my_exit(void) {
    gpio_free(LED_GPIO);
    unregister_chrdev(major, DEV_NAME);
    printk("my_led driver unloaded\n");
}

module_init(my_init);
module_exit(my_exit);
MODULE_LICENSE("GPL");
