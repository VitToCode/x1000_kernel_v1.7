
#include <linux/config.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <asm/processor.h>
#include <asm/uaccess.h>
 
/*
  file I/O in kernel module
*/
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
 
// Context : User
// Parameter :
// filename : filename to open
// flags :
// O_RDONLY, O_WRONLY, O_RDWR
// O_CREAT, O_EXCL, O_TRUNC, O_APPEND, O_NONBLOCK, O_SYNC, ...
// mode : file creation permission.
// S_IRxxx S_IWxxx S_IXxxx (xxx = USR, GRP, OTH), S_IRWXx (x = U, G, O)
// Return :
// file pointer. if error, return NULL
 
struct file *klib_fopen(const char *filename, int flags, int mode)
{ 
    struct file *filp = filp_open(filename, flags, mode);
 
    return (IS_ERR(filp)) ? NULL : filp;
}
 
// Context : User
// Parameter :
// filp : file pointer
// Return :
void klib_fclose(struct file *filp)
{
    if (filp)
        fput(filp);
}
 
// Context : User
// Parameter :
// filp : file pointer
// offset :
// whence : SEEK_SET, SEEK_CUR
// Comment :
// do not support SEEK_END
// no boundary check (file position may exceed file size)
int klib_fseek(struct file *filp, int offset, int whence)
{
 
    int pos = filp->f_pos;
 
    if (filp) {
 
        if (whence == SEEK_SET)
            pos = offset;
        else if (whence == SEEK_CUR)
            pos = offset;
 
        if (pos < 0)
            pos = 0;
 
        return (filp->f_pos = pos);
    } else
        return -ENOENT;
 
}
 
// Context : User
// Parameter :
// buf : buffer to read into
// len : number of bytes to read
// filp : file pointer
// Return :
// actually read number. 0 = EOF, negative = error
int klib_fread(char *buf, int len, struct file *filp)
{
    int readlen;
 
    mm_segment_t oldfs;
 
    if (filp == NULL)
        return -ENOENT;
 
    if (filp->f_op->read == NULL)
        return -ENOSYS;
 
    if (((filp->f_flags & O_ACCMODE) & O_RDONLY) != 0)
        return -EACCES;
 
    oldfs = get_fs();
    set_fs(KERNEL_DS);
 
    readlen = filp->f_op->read(filp, buf, len, &filp->f_pos);
 
    set_fs(oldfs);
 
    return readlen;
}
 
// Context : User
// Parameter :
// filp : file pointer
// Return :
// read character, EOF if end of file
 
int klib_fgetc(struct file *filp)
{
 
    int len;
    unsigned char buf[4];
 
    len = klib_fread((char *)buf, 1, filp);
 
    if (len > 0)
        return buf[0];
    else if (len == 0)
        return EOF;
    else
        return len;
}
 
// Context : User
// Parameter :
// str : string
// size : size of str buffer
// filp : file pointer
// Return :
// read string. NULL if end of file
// Comment :
// Always append trailing null character
 
char *klib_fgets(char *str, int size, struct file *filp)
{
 
    char *cp;
    int len, readlen;
    mm_segment_t oldfs;
 
    if (filp && filp->f_op->read
        && ((filp->f_flags & O_ACCMODE) & O_WRONLY) == 0) {
        oldfs = get_fs();
        set_fs(KERNEL_DS);
 
        for (cp = str, len = -1, readlen = 0; readlen < size - 1;
             cp, readlen) {
 
            if ((len =
                 filp->f_op->read(filp, cp, 1, &filp->f_pos)) <= 0)
                break;
 
            if (*cp == '/n') {
                cp;
                readlen;
                break;
 
            }
        }
 
        *cp = 0;
        set_fs(oldfs);
 
        return (len < 0 || readlen == 0) ? NULL : str;
    } else
        return NULL;
 
}
 
// Context : User
// Parameter :
// buf : buffer containing data to write
// len : number of bytes to write
// filp : file pointer
// Return :
// actually written number. 0 = retry, negative = error
 
int klib_fwrite(char *buf, int len, struct file *filp)
{
    int writelen;
    mm_segment_t oldfs;
 
    if (filp == NULL)
        return -ENOENT;
 
    if (filp->f_op->write == NULL)
        return -ENOSYS;
 
    if (((filp->f_flags & O_ACCMODE) & (O_WRONLY | O_RDWR)) == 0)
        return -EACCES;
 
    oldfs = get_fs();
    set_fs(KERNEL_DS);
    writelen = filp->f_op->write(filp, buf, len, &filp->f_pos);
    set_fs(oldfs);
 
    return writelen;
}
 
// Context : User
// Parameter :
// filp : file pointer
// Return :
// written character, EOF if error
int klib_fputc(int ch, struct file *filp)
{
    int len;
    unsigned char buf[4];
 
    buf[0] = (unsigned char)ch;
    len = klib_fwrite(buf, 1, filp);
 
    if (len > 0)
        return buf[0];
    else
        return EOF;
}
 
// Context : User
// Parameter :
// str : string
// filp : file pointer
// Return :
// count of written characters. 0 = retry, negative = error
int klib_fputs(char *str, struct file *filp)
{
    return klib_fwrite(str, strlen(str), filp);
}
 
// Context : User
// Parameter :
// filp : file pointer
// fmt : printf() style formatting string
// Return :
// same as klib_fputs()
int klib_fprintf(struct file *filp, const char *fmt, ...)
{
    static char s_buf[1024];
 
    va_list args;
    va_start(args, fmt);
    vsprintf(s_buf, fmt, args);
    va_end(args);
    return klib_fputs(s_buf, filp);
} 

