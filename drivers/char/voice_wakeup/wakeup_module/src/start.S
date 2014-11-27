/*
 * start.S
 *
 * Entry point of the firmware.
 * The firmware code are executed in the ICache.
 *
 * Copyright (C) 2006 Ingenic Semiconductor Inc.
 *
 */

	.text

	.globl	start

	.extern open
	.extern handler
	.extern close
	.extern cache_prefetch

	.extern set_handler
	.extern get_dma_address
	.extern ioctl
	.extern get_resource_addr
	.extern process_data
	.extern is_cpu_wakeup_by_dmic
	.extern set_sleep_buffer
	.extern get_sleep_process
start:
	.word open
	.word handler
	.word close
	.word cache_prefetch
	.word set_handler
	.word get_dma_address
	.word ioctl
	.word get_resource_addr
	.word process_data
	.word is_cpu_wakeup_by_dmic
	.word set_sleep_buffer
	.word get_sleep_process
