#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/thread_info.h>
#include <asm/uaccess.h>

#define BUF_LEN         64

static int magic_read_proc(char *page, char **start, off_t off,
                          int count, int *eof, void *data)
{
	if(test_thread_flag(TIF_MAGIC_CPUINFO))
		sprintf(page,"arm \n");
	else 
		sprintf(page,"mips\n");
		
	return 5;
}

static int magic_write_proc(struct file *file, const char __user *buffer,
                           unsigned long count, void *data)
{
        char buf[BUF_LEN];

        if (count > BUF_LEN)
                count = BUF_LEN;
        if (copy_from_user(buf, buffer, count))
                return -EFAULT;

        if (strncmp(buf, "arm", 3) == 0) {
	    set_thread_flag(TIF_MAGIC_CPUINFO);
        }
        else if (strncmp(buf, "mips", 4) == 0) {
	    clear_thread_flag(TIF_MAGIC_CPUINFO);
        }
        return count;
}

static int __init init_proc_magic(void)
{
        struct proc_dir_entry *res;

        res = create_proc_entry("magic", 0666, NULL);
        if (res) {
	    res->read_proc = magic_read_proc;
	    res->write_proc = magic_write_proc;
	}
        return 0;
}

module_init(init_proc_magic);


