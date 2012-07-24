/*
 * lib_nand/nand_io.c
 *
 * JZ4770 Nand Driver
 *
 * NAND I/O utils.
 */

#include "nand_api.h"

extern JZ_IO jz_nand_io;
extern JZ_IO jz_nand_dma_io;

static inline void jz4780_io_init(void *pnand_io)
{
	JZ_IO *pjz_nand_io = (JZ_IO *)pnand_io;
	jz_nand_io.io_init(pjz_nand_io);
	
	pjz_nand_io->send_cmd_norb = jz_nand_io.send_cmd_norb;
	pjz_nand_io->send_cmd_withrb = jz_nand_io.send_cmd_withrb;
	pjz_nand_io->send_addr = jz_nand_io.send_addr;
	pjz_nand_io->read_data_norb = jz_nand_io.read_data_norb;
	pjz_nand_io->write_data_norb = jz_nand_io.write_data_norb;
	pjz_nand_io->read_data_withrb = jz_nand_io.read_data_withrb;
	pjz_nand_io->verify_data = jz_nand_io.verify_data;
	pjz_nand_io->wait_ready = jz_nand_io.wait_ready;
}

JZ_IO jznand_io =
{
	.io_init = jz4780_io_init,
};

//EXPORT_SYMBOL(jznand_io);
/*
static inline void jz4780_dma_io_init(void *pnand_io)
{
	JZ_IO *pjz_nand_io = (JZ_IO *)pnand_io;
	dprintf("\nDEBUG nand:go into jz4770_dma_io_init \n");
	jz_nand_dma_io.io_init(pjz_nand_io);
	
	pjz_nand_io->read_data_norb = jz_nand_dma_io.read_data_norb;
	pjz_nand_io->write_data_norb = jz_nand_dma_io.write_data_norb;
	pjz_nand_io->read_data_withrb = jz_nand_dma_io.read_data_withrb;
	pjz_nand_io->dma_nand_finish = jz_nand_dma_io.dma_nand_finish;
	dprintf("\nDEBUG nand:go out jz4770_dma_io_init 0x%x\n",(int)pjz_nand_io->read_data_norb);
}

JZ_IO jznand_dma_io =
{
	.io_init = jz4780_dma_io_init,
};
*/
//EXPORT_SYMBOL(jznand_dma_io);
