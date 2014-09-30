#ifndef __JZ_VPU_H__
#define __JZ_VPU_H__

/*
  _H264E_SliceInfo:
  H264 Encoder Slice Level Information
 */
typedef struct _H264E_SliceInfo {
  /*basic*/
  int     i_csp;
  uint8_t frame_type;
  uint8_t mb_width;
  uint8_t mb_height;
  uint8_t first_mby;
  uint8_t last_mby;  //for multi-slice

  /*vmau scaling list*/
  uint8_t scaling_list[4][16];

  /*loop filter*/
  uint8_t deblock;      // DBLK CTRL : enable deblock
  uint8_t rotate;       // DBLK CTRL : rotate
  int8_t alpha_c0_offset;   // cavlc use, can find in bs.h
  int8_t beta_offset;

  /*cabac*/   // current hw only use cabac, no cavlc
  uint8_t state[1024];
  unsigned int bs;          /* encode bitstream start address */
  uint8_t qp;

  /*frame buffer address: all of the buffers should be 256byte aligned!*/
  unsigned int fb[3][2];       /*{curr, ref, raw}{tile_y, tile_c}*/
  /* fb[0] : DBLK output Y/C address
   * fb[1] : MCE reference Y/C address
   * fb[2] : EFE input Y/C buffer address
   */
  unsigned int raw[3];         /*{rawy, rawu, rawv} or {rawy, rawc, N/C}*/
  int stride[2];          /*{stride_y, stride_c}, only used in raster raw*/

  /*descriptor address*/
  unsigned int * des_va, des_pa;

  /*TLB address*/
  unsigned int tlba;

}_H264E_SliceInfo;

#define REG_VPU_GLBC      0x00000
#define VPU_INTE_ACFGERR     (0x1<<20)
#define VPU_INTE_TLBERR      (0x1<<18)
#define VPU_INTE_BSERR       (0x1<<17)
#define VPU_INTE_ENDF        (0x1<<16)

#define REG_VPU_STAT      0x00034
#define VPU_STAT_ENDF    (0x1<<0)
#define VPU_STAT_BPF     (0x1<<1)
#define VPU_STAT_ACFGERR (0x1<<2)
#define VPU_STAT_TIMEOUT (0x1<<3)
#define VPU_STAT_JPGEND  (0x1<<4)
#define VPU_STAT_BSERR   (0x1<<7)
#define VPU_STAT_TLBERR  (0x1F<<10)
#define VPU_STAT_SLDERR  (0x1<<16)

#define REG_VPU_JPGC_STAT 0xE0008
#define JPGC_STAT_ENDF   (0x1<<31)

#define REG_VPU_SDE_STAT  0x90000
#define SDE_STAT_BSEND   (0x1<<1)
#define REG_VPU_ENC_LEN  (0x90038)

#define REG_VPU_DBLK_STAT 0x70070
#define DBLK_STAT_DOEND  (0x1<<0)

#define REG_VPU_AUX_STAT  0xA0010
#define AUX_STAT_MIRQP   (0x1<<0)

#define vpu_readl(vpu, offset)		__raw_readl((vpu)->iomem + offset)
#define vpu_writel(vpu, offset, value)	__raw_writel((value), (vpu)->iomem + offset)

/* we add them to wait for vpu end before suspend */
#define VPU_NEED_WAIT_END_FLAG 0x80000000
#define VPU_WAIT_OK 0x40000000
#define VPU_END 0x1

#define REG_VPU_STATUS ( *(volatile unsigned int*)0xb3200034 )
#define REG_VPU_LOCK ( *(volatile unsigned int*)0xb329004c )
#define REG_VPUCDR ( *(volatile unsigned int*)0xb0000030 )
#define REG_CPM_VPU_SWRST ( *(volatile unsigned int*)0xb00000c4 )
#define CPM_VPU_SR           (0x1<<31)
#define CPM_VPU_STP          (0x1<<30)
#define CPM_VPU_ACK          (0x1<<29)
#endif	/* __JZ_VPU_H__ */
