#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/ppdev.h>
#include <linux/file.h>
#include <linux/kobject.h>

MODULE_LICENSE("GPL");

static int major = 0;
module_param(major, int, 0);

static char *default_tty = "/dev/ttyS0";
module_param(default_tty, charp, 0);

// this is a file pointer to the serial port we are going to write to
struct file* fp_out = NULL;

int parportser_open(struct inode* ind, struct file* f) {
    return 0;
}

long parportser_ioctl(struct file* fp, unsigned int cmd, unsigned long arg) {
    unsigned char reg;
    void __user *argp = (void __user *)arg;

    switch(cmd) {
    case PPCLAIM:
	// don't allow claiming the fake parallel port if no output tty is configured
	if (fp_out != NULL) {
	    return 0;
	}
	return -EFAULT;

    case PPRELEASE:
	return 0;

    case PPRDATA:
	reg = 0;
	if (copy_to_user(argp, &reg, sizeof(reg)))
	    return -EFAULT;
	return 0;

    case PPRSTATUS:
	// TODO: maybe this status is important in some cases? for now always report zero
	reg = 0;
	if (copy_to_user(argp, &reg, sizeof(reg)))
	    return -EFAULT;
	return 0;

    case PPWDATA:
	if (copy_from_user(&reg, argp, sizeof(reg)))
	    return -EFAULT;

	// printk(KERN_NOTICE "parportser: got ppwdata %x\n", reg);

	if (fp_out != NULL) {
	    // always expected to be a single byte
		ssize_t num_written;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14, 0)
	    num_written = vfs_write(fp_out, &reg, 1, &fp_out->f_pos);
#else
		num_written = kernel_write(fp_out, &reg, 1, &fp_out->f_pos);
#endif
		pr_info("parportser: Written %ld bytes, %#2x - %d - %c\n",
				num_written,
				reg,
				reg,
				reg);
	}
	else {
	    printk(KERN_ALERT "parportser: not writing data because tty is not open\n");
	}

	return 0;

    default:
	printk(KERN_WARNING "parportser: got unknown ioctl %x %lx\n", cmd, arg);
    }

    return 0;
}

int open_tty(const char* path) {
    fp_out = filp_open(path, O_RDWR|O_NOCTTY|O_NONBLOCK, 0);
    if (IS_ERR(fp_out)) {
	printk(KERN_ALERT "parportser: filp_open failed (%ld)\n", PTR_ERR(fp_out));
	fp_out = NULL;
	return -EFAULT;
    }

    printk(KERN_NOTICE "parportser: opened tty (%s)\n", path);
    return 0;
}

void close_tty(void) {
    if (fp_out != NULL) {
	filp_close(fp_out, NULL);
	fp_out = NULL;
    }
}

struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = NULL,
    .write = NULL,
    .poll = NULL,
    .open = parportser_open,
    .release = NULL,
    .unlocked_ioctl = parportser_ioctl
};


int parportser_init(void) {
    int result;
    printk(KERN_NOTICE "parportser: Hello from parportser\n");

    // register device
    result = register_chrdev(0, "parportser", &fops);

    if (result < 0) {
	printk(KERN_ALERT "parportser: can't get major number\n");
	return result;
    }

    printk(KERN_NOTICE "parportser: got major number %i\n", result);
    if (major == 0) major = result;

    // open target tty
    result = open_tty(default_tty);
    if (result < 0) {
	return result;
    }

    return 0;
}

void parportser_exit(void) {
    if (fp_out != NULL) {
	filp_close(fp_out, NULL);
    }
    unregister_chrdev(major, "parportser");
    printk(KERN_NOTICE "parportser: Goodbye from parport-ser\n");
}

module_init(parportser_init);
module_exit(parportser_exit);
