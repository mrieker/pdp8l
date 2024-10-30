//    Copyright (C) Mike Rieker, Beverly, MA USA
//    www.outerworldapps.com
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; version 2 of the License.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    EXPECT it to FAIL when someone's HeALTh or PROpeRTy is at RISk.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//    http://www.gnu.org/licenses/gpl-2.0.html

// kernel module for raspi to map the zynq.vhd fpga 4K page
// sets it up as a mmap page accessed from /proc/zynqpdp8l

#include <linux/version.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");

#define ZG_PROCNAME "zynqpdp8l"
#define ZG_PHYSADDR 0x43C00000
#define ZG_EXTMEMPA 0x43C20000

typedef struct FoCtx {
    unsigned long pagepa;
} FoCtx;

static int zg_open (struct inode *inode, struct file *filp);
static ssize_t zg_read (struct file *filp, char __user *buff, size_t size, loff_t *posn);
static ssize_t zg_write (struct file *filp, char const __user *buff, size_t size, loff_t *posn);
static int zg_release (struct inode *inode, struct file *filp);
static long zg_ioctl (struct file *filp, unsigned int cmd, unsigned long arg);
static int zg_mmap (struct file *filp, struct vm_area_struct *vma);


static struct file_operations const zg_fops = {
    .mmap    = zg_mmap,
    .open    = zg_open,
    .read    = zg_read,
    .release = zg_release,
    .unlocked_ioctl = zg_ioctl,
    .write   = zg_write
};

int init_module ()
{
    struct proc_dir_entry *procDirEntry = proc_create (ZG_PROCNAME, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH, NULL, &zg_fops);
    if (procDirEntry == NULL) {
        printk ("zynqgpio: error creating /proc/%s entry\n", ZG_PROCNAME);
        return -1;
    }
    return 0;
}

void cleanup_module ()
{
    remove_proc_entry (ZG_PROCNAME, NULL);
}

static int zg_open (struct inode *inode, struct file *filp)
{
    FoCtx *foctx;

    filp->private_data = NULL;
    if (filp->f_flags != O_RDWR) return -EACCES;
    foctx = kmalloc (sizeof *foctx, GFP_KERNEL);
    if (foctx == NULL) return -ENOMEM;
    memset (foctx, 0, sizeof *foctx);
    filp->private_data = foctx;
    return 0;
}

static ssize_t zg_read (struct file *filp, char __user *buff, size_t size, loff_t *posn)
{
    return -EIO;
}

static ssize_t zg_write (struct file *filp, char const __user *buff, size_t size, loff_t *posn)
{
    return -EIO;
}

static int zg_release (struct inode *inode, struct file *filp)
{
    FoCtx *foctx = filp->private_data;
    if (foctx != NULL) {
        filp->private_data = NULL;
        kfree (foctx);
    }
    return 0;
}

static long zg_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
    //FoCtx *foctx = filp->private_data;
    return -EINVAL;
}

static int zg_mmap (struct file *filp, struct vm_area_struct *vma)
{
    int rc;

    long len = vma->vm_end - vma->vm_start;

    printk ("zg_mmap: len=%lX start=%lX end=%lX pgoff=%lu\n", len, vma->vm_start, vma->vm_end, vma->vm_pgoff);

    if (vma->vm_pgoff == 0) {
        if (len != PAGE_SIZE) {
            printk ("zg_mmap: not mapping single page at va %lX..%lX => %lX\n", vma->vm_start, vma->vm_end, len);
            return -EINVAL;
        }
        vma->vm_page_prot = pgprot_noncached (vma->vm_page_prot);
        rc = vm_iomap_memory (vma, ZG_PHYSADDR, PAGE_SIZE);
        printk ("zg_mmap: iomap %lu status %d\n", vma->vm_pgoff, rc);
        return rc;
    }

    if ((vma->vm_pgoff >= 32) && (vma->vm_pgoff < 64)) {
        if (len != 0x20000) {
            printk ("zg_mmap: not mapping 128K at va %lX..%lX => %lX\n", vma->vm_start, vma->vm_end, len);
            return -EINVAL;
        }
        vma->vm_page_prot = pgprot_noncached (vma->vm_page_prot);
        ///rc = vm_iomap_memory (vma, ZG_EXTMEMPA, PAGE_SIZE);
        rc = io_remap_pfn_range (vma, vma->vm_start, ZG_EXTMEMPA >> PAGE_SHIFT, 0x20000, vma->vm_page_prot);
        printk ("zg_mmap: iomap %lu status %d\n", vma->vm_pgoff, rc);
        return rc;
    }

    printk ("zg_map: bad file page offset %lu\n", vma->vm_pgoff);
    return -EINVAL;
}
