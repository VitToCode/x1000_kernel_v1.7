#include <linux/proc_fs.h>
#include <asm/mipsregs.h>
#include <asm/uaccess.h>

#define get_pmon_csr()		__read_32bit_c0_register($16, 7)
#define set_pmon_csr(val)	__write_32bit_c0_register($16, 7, val)

#define get_pmon_high()		__read_32bit_c0_register($16, 4)
#define set_pmon_high(val)	__write_32bit_c0_register($16, 4, val)
#define get_pmon_lc()		__read_32bit_c0_register($16, 5)
#define set_pmon_lc(val)	__write_32bit_c0_register($16, 5, val)
#define get_pmon_rc()		__read_32bit_c0_register($16, 6)
#define set_pmon_rc(val)	__write_32bit_c0_register($16, 6, val)

#define pmon_clear_cnt() do {			\
		set_pmon_high(0);		\
		set_pmon_lc(0);			\
		set_pmon_rc(0);			\
	} while(0)

#define pmon_start() do {			\
		unsigned int csr;		\
		csr = get_pmon_csr();		\
		csr |= 0x100;			\
		set_pmon_csr(csr);		\
	} while(0)
#define pmon_stop() do {			\
		unsigned int csr;		\
		csr = get_pmon_csr();		\
		csr &= ~0x100;			\
		set_pmon_csr(csr);		\
	} while(0)

#define PMON_EVENT_CYCLE 0
#define PMON_EVENT_CACHE 1
#define PMON_EVENT_INST  2

#define pmon_prepare(event) do {		\
		unsigned int csr;		\
		pmon_stop();			\
		pmon_clear_cnt();		\
		csr = get_pmon_csr();		\
		csr &= ~0xf000;			\
		csr |= (event)<<12;		\
		set_pmon_csr(csr);		\
	} while(0)

#define BUF_LEN		64
struct pmon_data {
	char buf[BUF_LEN];
};

static int pmon_read_proc(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	int len = 0;
#define PRINT(ARGS...) len += sprintf (page+len, ##ARGS)
	PRINT("%x %x %x %x\n",
	      get_pmon_csr(),
	      get_pmon_high(),
	      get_pmon_lc(),
	      get_pmon_rc());
	return len;
}
static int pmon_write_proc(struct file *file, const char __user *buffer,
			   unsigned long count, void *data)
{
	struct pmon_data *d = data;
	int i;

	if (count > BUF_LEN)
		count = BUF_LEN;
	if (copy_from_user(d->buf, buffer, count))
		return -EFAULT;

	for (i = 0; i < count; ) {
		if (strncmp(&d->buf[i], "cycle", 5) == 0) {
			pmon_prepare(PMON_EVENT_CYCLE);
			i += 5 + 1;
		}
		else if (strncmp(&d->buf[i], "cache", 5) == 0) {
			pmon_prepare(PMON_EVENT_CACHE);
			i += 5 + 1;
		}
		else if (strncmp(&d->buf[i], "inst", 5) == 0) {
			pmon_prepare(PMON_EVENT_INST);
			i += 4 + 1;
		}
		else if (strncmp(&d->buf[i], "start", 5) == 0) {
			pmon_start();
			break;
		}
	}
	return count;
}

static int __init init_proc_pmon(void)
{
	struct proc_dir_entry *res;

	res = create_proc_entry("pmon", 0644, NULL);
	if (res) {
		res->read_proc = pmon_read_proc;
		res->write_proc = pmon_write_proc;
		res->data = kmalloc(sizeof(struct pmon_data),GFP_KERNEL);
	}
	return 0;
}

module_init(init_proc_pmon);
