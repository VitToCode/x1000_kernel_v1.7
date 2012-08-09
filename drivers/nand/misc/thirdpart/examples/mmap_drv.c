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

#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/wrapper.h>
#include <linux/slab.h>
#include <asm/io.h>

#define LEN (64*1024)

/* device open */
int mmapdrv_open(struct inode *inode, struct file *file);
/* device close */
int mmapdrv_release(struct inode *inode, struct file *file);
/* device mmap */
int mmapdrv_mmap(struct file *file, struct vm_area_struct *vma);

/* open handler for vm area */
void mmap_drv_vopen(struct vm_area_struct *vma);
/* close handler form vm area */
void mmap_drv_vclose(struct vm_area_struct *vma);
/* page fault handler for callback of vmalloc area */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
unsigned long mmap_drv_vmmap(struct vm_area_struct *vma, unsigned long address, int write_access);
#else
struct page *mmap_drv_vmmap(struct vm_area_struct *vma, unsigned long address, int write_access);
#endif

/* the ordinary device operations */
static struct file_operations mmapdrv_fops =
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
  owner:   THIS_MODULE,
#endif
  mmap:    mmapdrv_mmap,
  open:    mmapdrv_open,
  release: mmapdrv_release,
};

/* memory handler functions */
static struct vm_operations_struct mmap_drv_vm_ops = {
  open:    mmap_drv_vopen, /* mmap-open */
  close:  mmap_drv_vclose,/* mmap-close */
  nopage: mmap_drv_vmmap, /* no-page fault handler */
};

/* pointer to page aligned area */
static int *vmalloc_area = NULL;
/* pointer to unaligend area */
static int *vmalloc_ptr  = NULL;
/* pointer to page aligned area */
static int *kmalloc_area = NULL;
/* pointer to unaligned area */
static int *kmalloc_ptr = NULL;
/* major number of device */
static int major;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
/* Converts a kernel virtual address into a kernel virtual
   address that is part of the direct mapping between
   virtual and physical address. If you e.g. allocated
   memory with vmalloc(), you get virtual addresses part
   of an own area. By converting such an address, 
   you receive a kernel virtual address that you can
   e.g. feed into virt_to_phys() or MAP_NR().
   Note: the function below works for one page. If you
   have a set of pages, in a vmalloc allocated area,
  each page may have a different virtual address in
   the direct mapping.
   Return 0 if no mapping found.
*/
volatile void *virt_to_kseg(volatile void *address)
{
        pgd_t *pgd; pmd_t *pmd; pte_t *ptep, pte;
        unsigned long ret=0UL;
	
        /* if we are below the max direct mappings, we use the
           direct conversion function
        */ 
        if (MAP_NR(address) < max_mapnr)
                return(address);

        /* else we really have to parse the page table to get the map nr */

	/* get the page global directory out of the kernel memory map. */
	pgd = pgd_offset_k((unsigned long)address);

	/* check whether we found an entry */
	if (!pgd_none(*pgd))
        {
	       /* get the page middle directory */
	       pmd = pmd_offset(pgd, (unsigned long)address);
	       /* check for a valid entry */
	       if (!pmd_none(*pmd))
               {
		    /* get a pointer to the page table entry */
	            ptep = pte_offset(pmd, (unsigned long)address);
		    /* get the page table entry itself */
	            pte = *ptep;
		    /* check for a valid page */
	            if (pte_present(pte))
                    {
		      /* get the kseg address of the page */
		      ret = (unsigned long)pte_page(pte);
		      /* add the offset within the page to the page address */
		      ret |= ((unsigned long)address & (PAGE_SIZE - 1));
		    }
	       }
	}
        return((volatile void *)ret);
}
#else
/* we parse the page tables in order to find the direct mapping of
   the page. This works only without holding any locks for pages we
   are sure that they do not move in memory.
   Annother example achieving the same can be found in the
   bttv-driver (drivers/media/video).
*/
volatile void *virt_to_kseg(volatile void *address)
{
        pgd_t *pgd; pmd_t *pmd; pte_t *ptep, pte;
	unsigned long va, ret = 0UL;
	
	va=VMALLOC_VMADDR((unsigned long)address);
	
	/* get the page directory. Use the kernel memory map. */
	pgd = pgd_offset_k(va);

	/* check whether we found an entry */
	if (!pgd_none(*pgd))
        {
	      /* get the page middle directory */
	      pmd = pmd_offset(pgd, va);
	      /* check whether we found an entry */
	      if (!pmd_none(*pmd))
              {
		  /* get a pointer to the page table entry */
	          ptep = pte_offset(pmd, va);
	          pte = *ptep;
		  /* check for a valid page */
	          if (pte_present(pte))
                  {
		        /* get the address the page is refering to */
		        ret = (unsigned long)page_address(pte_page(pte));
			/* add the offset within the page to the page address */
			ret |= (va & (PAGE_SIZE -1));
		  }
	      }
	}
	return((volatile void *)ret);
}
#endif

/* load the module */
int init_module(void)
{
        int i;
        unsigned long virt_addr;
        
        if ((major=register_chrdev(0, "mmapdrv", &mmapdrv_fops))<0) {
                printk("mmapdrv: unable to register character device\n");
                return (-EIO);
        }

        /* get a memory area with kmalloc and aligned it to a page. This area
           will be physically contigous */
        kmalloc_ptr=kmalloc(LEN+2*PAGE_SIZE, GFP_KERNEL);
        kmalloc_area=(int *)(((unsigned long)kmalloc_ptr + PAGE_SIZE -1) & PAGE_MASK);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
       for(i=MAP_NR(kmalloc_area); i<=MAP_NR((void *)kmalloc_area+LEN);i++)
       {
                /* reserve all pages to make them remapable */
                mem_map_reserve(i);
       }
#else
       for (virt_addr=(unsigned long)kmalloc_area; virt_addr<(unsigned long)kmalloc_area+LEN;
	    virt_addr+=PAGE_SIZE)
       {
	        /* reserve all pages to make them remapable */
	        mem_map_reserve(virt_to_page(virt_addr));
       }
#endif

        /* get a memory area that is only virtual contigous. */
        vmalloc_ptr=vmalloc(LEN+2*PAGE_SIZE);
        vmalloc_area=(int *)(((unsigned long)vmalloc_ptr + PAGE_SIZE -1) & PAGE_MASK);
        for (virt_addr=(unsigned long)vmalloc_area;
	     virt_addr<(unsigned long)(&(vmalloc_area[LEN/sizeof(int)]));
             virt_addr+=PAGE_SIZE)
        {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
                /* reserve all pages to make them remapable. */
                mem_map_reserve(MAP_NR(virt_to_kseg((void *)virt_addr)));
#else
		mem_map_reserve(virt_to_page(virt_to_kseg((void *)virt_addr)));
#endif
        }
        
        for (i=0; i<(LEN/sizeof(int)); i+=2)
        {
                /* initialise with some dummy values to compare later */
                vmalloc_area[i]=(0xaffe<<16) + i;
                vmalloc_area[i+1]=(0xbeef<<16) + i;
                kmalloc_area[i]=(0xdead<<16) +i;
                kmalloc_area[i+1]=(0xbeef<<16) + i;
        }

        /* and tell the world what we did */
        printk("vmalloc_area at 0x%p (phys 0x%lx)\n", vmalloc_area,
               virt_to_phys((void *)virt_to_kseg(vmalloc_area)));
        printk("kmalloc_area at 0x%p (phys 0x%lx)\n", kmalloc_area,
               virt_to_phys((void *)virt_to_kseg(kmalloc_area)));

        return(0);
}

/* remove the module */
void cleanup_module(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
        int i;
#endif
        unsigned long virt_addr;

        /* unreserve all pages */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
        for(i=MAP_NR(kmalloc_area); i<=MAP_NR((void *)kmalloc_area+LEN);i++)
        {
                mem_map_unreserve(i);
        }
#else
        for(virt_addr=(unsigned long)kmalloc_area; virt_addr<(unsigned long)kmalloc_area+LEN;
	    virt_addr+=PAGE_SIZE)
        {
                mem_map_unreserve(virt_to_page(virt_addr));
        }
#endif
        for (virt_addr=(unsigned long)vmalloc_area;
	     virt_addr<(unsigned long)(&(vmalloc_area[LEN/sizeof(int)]));
             virt_addr+=PAGE_SIZE)
        {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
                mem_map_unreserve(MAP_NR(virt_to_kseg((void *)virt_addr)));
#else
		mem_map_unreserve(virt_to_page(virt_to_kseg((void *)virt_addr)));
#endif
        }

        /* and free the two areas */
        if (vmalloc_ptr)
                vfree(vmalloc_ptr);
        if (kmalloc_ptr)
                kfree(kmalloc_ptr);

        /* unregister the device */
        unregister_chrdev(major, "mmapdrv");
        return;
}

/* device open method */
int mmapdrv_open(struct inode *inode, struct file *file)
{
        MOD_INC_USE_COUNT;
        return(0);
}

/* device close method */
int mmapdrv_release(struct inode *inode, struct file *file)
{
        MOD_DEC_USE_COUNT;
        return(0);
}

/* device memory map method */
/* 2.4.x: this method is called from do_mmap_pgoff, from
   do_mmap, from the syscall. The caller of do_mmap grabs
   the mm semaphore. So we are protected from races here.
*/
int mmapdrv_mmap(struct file *file, struct vm_area_struct *vma)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
        unsigned long offset = vma->vm_offset;
#else
	unsigned long offset = vma->vm_pgoff<<PAGE_SHIFT;
#endif
        unsigned long size = vma->vm_end - vma->vm_start;
        
        if (offset & ~PAGE_MASK)
        {
                printk("offset not aligned: %ld\n", offset);
                return -ENXIO;
        }
        
        if (size>LEN)
        {
                printk("size too big\n");
                return(-ENXIO);
        }
        
	/* we only support shared mappings. Copy on write mappings are
	   rejected here. A shared mapping that is writeable must have the
	   shared flag set.
	*/
	if ((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED))
	{
	     printk("writeable mappings must be shared, rejecting\n");
	     return(-EINVAL);
	}

	/* we do not want to have this area swapped out, lock it */
	vma->vm_flags |= VM_LOCKED;
        
        /* there are two different mapping options implemented here:
           for the virtual contiguous memory area, we install a page fault handler.
           The page fault handler calculates the right physical page on first
           access of the application to the page.
           (method 1 is used for vmalloc'd memory, offset 0..LEN)
           The second way works only for a physical contigous range of pages:
           we create a mapping between the physical pages and the virtual
           addresses of the application with remap_page_range.
           (method 2 is used for kmalloc'd memory, offset LEN..2*LEN)
        */
        if (offset == 0)
        {
                /* method 1: install a page handler */
                vma->vm_ops = &mmap_drv_vm_ops;
                /* call the open routine to increment the usage count */
                mmap_drv_vopen(vma);
        } else if (offset == LEN)
        {
                /* method 2: enter pages into mapping of application */
                if (remap_page_range(vma->vm_start,
                                     virt_to_phys((void*)((unsigned long)kmalloc_area)),
                                     size,
                                     PAGE_SHARED))
                {
                        printk("remap page range failed\n");
                        return -ENXIO;
                }
        } else
        {
                printk("offset out of range\n");
                return -ENXIO;
        }
        return(0);
}

/* open handler for vm area */
void mmap_drv_vopen(struct vm_area_struct *vma)
{
        /* needed to prevent the unloading of the module while
           somebody still has memory mapped */
        MOD_INC_USE_COUNT;
}

/* close handler form vm area */
void mmap_drv_vclose(struct vm_area_struct *vma)
{
        MOD_DEC_USE_COUNT;
}

/* page fault handler */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
unsigned long mmap_drv_vmmap(struct vm_area_struct *vma, unsigned long address, int write_access)
#else
struct page *mmap_drv_vmmap(struct vm_area_struct *vma, unsigned long address, int write_access)
#endif
{
        unsigned long offset;
        unsigned long virt_addr;

         /* determine the offset within the vmalloc'd area  */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
        offset = address - vma->vm_start + vma->vm_offset;
#else
        offset = address - vma->vm_start + (vma->vm_pgoff<<PAGE_SHIFT);
#endif

        /* calculate the kseg virtual address */
        virt_addr = (unsigned long)virt_to_kseg(&vmalloc_area[offset/sizeof(int)]);

	/* check whether we found a translation */
	if (virt_addr == 0UL)
	{
	       printk("page fault out of range\n");
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
	       return(virt_addr);
#else
	       return((struct page *)0UL);
#endif
	}

        /* increment the usage count of the page */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
        atomic_inc(&mem_map[MAP_NR(virt_addr)].count);
#else
	atomic_inc(&(virt_to_page(virt_addr)->count));
#endif
        
        printk("mmap_drv: page fault for offset 0x%lx (kseg x%lx)\n",
               offset, virt_addr);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
        /* return the kseg virtual address, *not* the physical address as stated
           in some wrong examples.
        */
        return(virt_addr);
#else
	/* return the page pointer */
	return(virt_to_page(virt_addr));
#endif
}
