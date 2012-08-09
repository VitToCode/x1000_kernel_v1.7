#include <linux/config.h>
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
#include <linux/smp_lock.h>

#include <asm/signal.h>
#include <asm/unistd.h>
#include <asm/semaphore.h>

#define NTHREADS 5

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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
        wait_queue_head_t queue;
#else
        struct wait_queue *queue;
#endif
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
        /* initialize the semaphore:
           we start with the semaphore locked. The new kernel
           thread will setup its stuff and unlock it. This
           control flow (the one that creates the thread) blocks
           in the down operation below until the thread has reached
           the up() operation.
         */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
        init_MUTEX_LOCKED(&thread->startstop_sem);
#else
        thread->startstop_sem = MUTEX_LOCKED;
#endif

        /* create the new thread */
        kernel_thread(func, (void *)thread, 0);

        /* wait till it has reached the setup_thread routine */
        down(&thread->startstop_sem);
               
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
        lock_kernel();
        
        /* initialize the semaphore. We lock it here, the
           leave_thread call of the thread to be terminated
           will unlock it. As soon as we see the semaphore
           unlocked, we know that the thread has exited.
	*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
        init_MUTEX_LOCKED(&thread->startstop_sem);
#else
        thread->startstop_sem = MUTEX_LOCKED;
#endif
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
        kill_proc(thread->thread->pid, SIGKILL, 1);
       
        /* block till thread terminated */
        down(&thread->startstop_sem);

        /* release the big kernel lock */
        unlock_kernel();
}

/* function to put a thread in background. This call is
   part of 2.2.18 and newer kernels, but not the older
   ones.
*/
#if  LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18)
static void daemonize(void)
{
         struct fs_struct *fs;
        /* release insmod's memory map, use the kernel one's */
        exit_mm(current);
        /* set session leader and process group to init process */
        current->session = 1;
        current->pgrp = 1;
        /* disconnect from the fs usage */
        exit_fs(current);
	fs = init_task.fs;
	current->fs= fs;
	atomic_inc(&fs->count);
	/* close all open files */
        exit_files(current);
	current->files = init_task.files;
	atomic_inc(&current->files->count);
} 
#endif

/* initialize new created thread. Called by the new thread. */
static void setup_thread(my_threads *thread)
{
        /* lock the kernel. A new kernel thread starts without
           the big kernel lock, regardless of the lock state
           of the creator (the lock level is *not* inheritated)
        */
        lock_kernel();

	/* put ourself in background, release all user space
	   connection.
	*/
	daemonize();

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,18)
	/* the 2.2.18 daemonize() call does not call exit_files */
        exit_files(current);
	current->files = init_task.files;
	atomic_inc(&current->files->count);
#endif
#endif

        /* set name of this process (max 15 chars + 0 !) */
        sprintf(current->comm, "example thread");

        /* fill in thread structure */
        thread->thread = current;

        /* set signal mask to what we want to respond */
        siginitsetinv(&current->blocked, sigmask(SIGKILL)|sigmask(SIGINT)|sigmask(SIGTERM));

        /* initialise wait queue */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
        init_waitqueue_head(&thread->queue);
#else
	init_waitqueue(&thread->queue);
#endif

        /* initialise termination flag */
        thread->terminate = 0;
        
        /* let others run */
        unlock_kernel();

        /* tell the creator that we are ready and let him run */
        up(&thread->startstop_sem);

}

/* cleanup of thread. Called by the exiting thread. */
static void leave_thread(my_threads *thread)
{
        /* we are terminating */

	/* lock the kernel, the exit will unlock it */
        lock_kernel();
        thread->thread = NULL;
        mb();

        /* notify the kill_thread() routine that we are terminating. */
	up(&thread->startstop_sem);
	/* the kernel_thread that called clone() does a do_exit here. */

	/* there is no race here between unloading the module and real termination
	   of the thread (race window between up and do_exit), since both the
	   thread and the unload function are running with the kernel lock hold.
	   The kernel lock will be freed after the thread exited, so the code
	   is really not executed anymore as soon as the unload functions gets
	   the kernel lock back.
	   The init process may not have made the cleanup of the process here,
	   but the cleanup can be done safely with the module unloaded.
	*/

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
                interruptible_sleep_on_timeout(&thread->queue, HZ/1000);

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

