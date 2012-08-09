//#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#if defined(MODVERSIONS)
#include <linux/modversions.h>
#endif

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/sched.h>
//#include <linux/smp_lock.h>

#include <asm/signal.h>
#include <asm/unistd.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#define NTHREADS 1

MODULE_LICENSE("GPL");

/* a structure to store all information we need
   for our thread */
typedef struct
{
        /* Linux task structure of thread */
        struct task_struct *thread;
        /* semaphore needed on start and creation of thread. */
        struct semaphore startstop_sem;
        /* flag to tell thread whether to die or not */
        int terminate;
        /* queue thread is waiting on */
} my_threads;

/* prototype of our example thread */
void example_thread(my_threads *thread);

/* prototype to create a new thread */
static void launch_thread(int (*func)(void *), my_threads *thread);

/* prototype to kill a running thread */
static void kill_thread(my_threads *thread);

/* the variable that contains the thread data */
my_threads example[NTHREADS];

/* load the module */
int init_module(void)
{
        int i;
        
        /* create new kernel threads */
	for (i=0; i <NTHREADS; i++)
	  launch_thread((int (*)(void *))example_thread, &example[i]);
        
        return(0);
}

/* remove the module */
void cleanup_module(void)
{
        int i;

	
        /* terminate the kernel threads */
	for (i=0; i<NTHREADS; i++)
	  kill_thread(&example[i]);
        
        return;
}

/* private functions */

/* create a new kernel thread. Called by the creator. */
static void launch_thread(int (*func)(void *), my_threads *thread)
{
	static const struct sched_param param = { .sched_priority = 1 };
		
        /* initialize the semaphore:
           we start with the semaphore locked. The new kernel
           thread will setup its stuff and unlock it. This
           control flow (the one that creates the thread) blocks
           in the down operation below until the thread has reached
           the up() operation.
         */

		sema_init(&thread->startstop_sem,1);

        /* create the new thread */
        thread->thread = kthread_create(func, (void *)thread, "dthread");
		sched_setscheduler(thread->thread, SCHED_IDLE, &param);
		if (!IS_ERR(thread->thread))
			wake_up_process(thread->thread);
}

/* remove a kernel thread. Called by the removing instance */
static void kill_thread(my_threads *thread)
{
        if (thread->thread == NULL)
        {
                printk("thread_drv: killing non existing thread!\n");
                return;
        }

        /* this function needs to be protected with the big
	   kernel lock (lock_kernel()). The lock must be
           grabbed before changing the terminate
	   flag and released after the down() call. */
        
       /* We need to do a memory barrier here to be sure that
           the flags are visible on all CPUs. 
        */
        mb();

        /* set flag to request thread termination */
        thread->terminate = 1;

        /* We need to do a memory barrier here to be sure that
           the flags are visible on all CPUs. 
        */
        mb();

		kthread_stop(thread->thread);


}

/* initialize new created thread. Called by the new thread. */
static void setup_thread(my_threads *thread)
{

        /* initialise termination flag */
        thread->terminate = 0;
        

}

/* cleanup of thread. Called by the exiting thread. */
static void leave_thread(my_threads *thread)
{
	/* we are terminating */

}

/* this is the thread function that we are executing */
void example_thread(my_threads *thread)
{
        /* setup the thread environment */
        setup_thread(thread);

        printk("hi, here is the kernel thread\n");
        
        /* an endless loop in which we are doing our work */
        for(;;)
        {
                /* fall asleep for one second */
               	msleep(500);
                /* We need to do a memory barrier here to be sure that
                   the flags are visible on all CPUs. 
                */
                 mb();
                
                /* here we are back from sleep, either due to the timeout
                   (one second), or because we caught a signal.
                */
                if (thread->terminate)
                {
                        /* we received a request to terminate ourself */
                        break;    
                }
                
                /* this is normal work to do */
                printk("example thread: thread woke up\n");
        }
        /* here we go only in case of termination of the thread */

        /* cleanup the thread, leave */
        leave_thread(thread);

	/* returning from the thread here calls the exit functions */
}

