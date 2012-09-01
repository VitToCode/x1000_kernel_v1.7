#if defined(__ASSEMBLY__) || defined(__LANGUAGE_ASSEMBLY)
        #ifndef __MIPS_ASSEMBLER
                #define __MIPS_ASSEMBLER
        #endif
        #define REG8(addr)	(addr)
        #define REG16(addr)	(addr)
        #define REG32(addr)	(addr)
#else
        #define REG8(addr)	*((volatile unsigned char *)(addr))
        #define REG16(addr)	*((volatile unsigned short *)(addr))
        #define REG32(addr)	*((volatile unsigned int *)(addr))

        #define INREG8(x)               ((unsigned char)(*(volatile unsigned char *)(x)))
        #define OUTREG8(x, y)           *(volatile unsigned char *)(x) = (y)
        #define SETREG8(x, y)           OUTREG8(x, INREG8(x)|(y))
        #define CLRREG8(x, y)           OUTREG8(x, INREG8(x)&~(y))
        #define CMSREG8(x, y, m)        OUTREG8(x, (INREG8(x)&~(m))|(y))

        #define INREG16(x)              ((unsigned short)(*(volatile unsigned short *)(x)))
        #define OUTREG16(x, y)          *(volatile unsigned short *)(x) = (y)
        #define SETREG16(x, y)          OUTREG16(x, INREG16(x)|(y))
        #define CLRREG16(x, y)          OUTREG16(x, INREG16(x)&~(y))
        #define CMSREG16(x, y, m)       OUTREG16(x, (INREG16(x)&~(m))|(y))

        #define INREG32(x)              ((unsigned int)(*(volatile unsigned int *)(x)))
        #define OUTREG32(x, y)          *(volatile unsigned int *)(x) = (y)
        #define SETREG32(x, y)          OUTREG32(x, INREG32(x)|(y))
        #define CLRREG32(x, y)          OUTREG32(x, INREG32(x)&~(y))
        #define CMSREG32(x, y, m)       OUTREG32(x, (INREG32(x)&~(m))|(y))

#endif


/*
 * Define the bit field macro to avoid the bit mistake
 */
#define BIT0            (1 << 0)
#define BIT1            (1 << 1)
#define BIT2            (1 << 2)
#define BIT3            (1 << 3)
#define BIT4            (1 << 4)
#define BIT5            (1 << 5)
#define BIT6            (1 << 6)
#define BIT7            (1 << 7)
#define BIT8            (1 << 8)
#define BIT9            (1 << 9)
#define BIT10           (1 << 10)
#define BIT11           (1 << 11)
#define BIT12 	        (1 << 12)
#define BIT13 	        (1 << 13)
#define BIT14 	        (1 << 14)
#define BIT15 	        (1 << 15)
#define BIT16 	        (1 << 16)
#define BIT17 	        (1 << 17)
#define BIT18 	        (1 << 18)
#define BIT19 	        (1 << 19)
#define BIT20 	        (1 << 20)
#define BIT21 	        (1 << 21)
#define BIT22 	        (1 << 22)
#define BIT23 	        (1 << 23)
#define BIT24 	        (1 << 24)
#define BIT25 	        (1 << 25)
#define BIT26 	        (1 << 26)
#define BIT27 	        (1 << 27)
#define BIT28 	        (1 << 28)
#define BIT29 	        (1 << 29)
#define BIT30 	        (1 << 30)
#define BIT31 	        (1 << 31)


/* Generate the bit field mask from msb to lsb */
#define BITS_H2L(msb, lsb)  ((0xFFFFFFFF >> (32-((msb)-(lsb)+1))) << (lsb))


/* Get the bit field value from the data which is read from the register */
#define get_bf_value(data, lsb, mask)  (((data) & (mask)) >> (lsb))



#define	CPM_BASE	0xb0000000
#define CPM_USBPCR1       (CPM_BASE + CPM_USBPCR1_OFFSET)
#define REG_CPM_USBPCR1         REG32(CPM_USBPCR1)


/*
 * Clock reset and power controller module(CPM) address definition
 */
#define	CPM_BASE	0xb0000000


/*
 * CPM registers offset address definition
 */
#define CPM_CPCCR_OFFSET        (0x00)  /* rw, 32, 0x01011100 */
#define CPM_LCR_OFFSET          (0x04)  /* rw, 32, 0x000000f8 */
#define CPM_RSR_OFFSET          (0x08)  /* rw, 32, 0x???????? */
#define CPM_CPPCR0_OFFSET       (0x10)  /* rw, 32, 0x28080011 */
#define CPM_CPPSR_OFFSET        (0x14)  /* rw, 32, 0x80000000 */
#define CPM_CLKGR0_OFFSET       (0x20)  /* rw, 32, 0x3fffffe0 */
#define CPM_OPCR_OFFSET         (0x24)  /* rw, 32, 0x00001570 */
#define CPM_CLKGR1_OFFSET       (0x28)  /* rw, 32, 0x0000017f */
#define CPM_CPPCR1_OFFSET       (0x30)  /* rw, 32, 0x28080002 */
#define CPM_CPSPR_OFFSET        (0x34)  /* rw, 32, 0x???????? */
#define CPM_CPSPPR_OFFSET       (0x38)  /* rw, 32, 0x0000a5a5 */
#define CPM_USBPCR_OFFSET       (0x3c)  /* rw, 32, 0x42992198 */
#define CPM_USBRDT_OFFSET       (0x40)  /* rw, 32, 0x00000096 */
#define CPM_USBVBFIL_OFFSET     (0x44)  /* rw, 32, 0x00000080 */
#define CPM_USBPCR1_OFFSET      (0x48)  /* rw, 32, 0x42992198 */
#define CPM_USBCDR_OFFSET       (0x50)  /* rw, 32, 0x00000000 */
#define CPM_I2SCDR_OFFSET       (0x60)  /* rw, 32, 0x00000000 */
#define CPM_LPCDR_OFFSET        (0x64)  /* rw, 32, 0x00000000 */
#define CPM_MSC0CDR_OFFSET       (0x68)  /* rw, 32, 0x00000000 */
#define CPM_MSC1CDR_OFFSET       (0xA4)  /* rw, 32, 0x00000000 */
#define CPM_MSC2CDR_OFFSET       (0xA8)  /* rw, 32, 0x00000000 */
#define CPM_BCHCDR_OFFSET       (0xAC)  /* rw, 32, 0x00000000 */
#define CPM_UHCCDR_OFFSET       (0x6c)  /* rw, 32, 0x00000000 */
#define CPM_SSICDR_OFFSET       (0x74)  /* rw, 32, 0x00000000 */
#define CPM_CIMCDR_OFFSET       (0x7c)  /* rw, 32, 0x00000000 */
#define CPM_GPSCDR_OFFSET       (0x80)  /* rw, 32, 0x00000000 */
#define CPM_PCMCDR_OFFSET       (0x84)  /* rw, 32, 0x00000000 */
#define CPM_GPUCDR_OFFSET       (0x88)  /* rw, 32, 0x00000000 */
#define CPM_PSWC0ST_OFFSET      (0x90)  /* rw, 32, 0x00000000 */
#define CPM_PSWC1ST_OFFSET      (0x94)  /* rw, 32, 0x00000000 */
#define CPM_PSWC2ST_OFFSET      (0x98)  /* rw, 32, 0x00000000 */
#define CPM_PSWC3ST_OFFSET      (0x9c)  /* rw, 32, 0x00000000 */
#define CPM_SLBC_OFFSET      	(0xc8)  /* rw, 32, 0x00000000 */
#define CPM_SLPC_OFFSET      	(0xcc)  /* rw, 32, 0x00000000 */

/*
 * CPM registers address definition
 */
#define CPM_CPCCR        (CPM_BASE + CPM_CPCCR_OFFSET)
#define CPM_LCR          (CPM_BASE + CPM_LCR_OFFSET)
#define CPM_RSR          (CPM_BASE + CPM_RSR_OFFSET)
#define CPM_CPPCR0       (CPM_BASE + CPM_CPPCR0_OFFSET)
#define CPM_CPPSR        (CPM_BASE + CPM_CPPSR_OFFSET)
#define CPM_CLKGR0       (CPM_BASE + CPM_CLKGR0_OFFSET)
#define CPM_OPCR         (CPM_BASE + CPM_OPCR_OFFSET)
#define CPM_CLKGR1       (CPM_BASE + CPM_CLKGR1_OFFSET)
#define CPM_CPPCR1       (CPM_BASE + CPM_CPPCR1_OFFSET)
#define CPM_CPSPR        (CPM_BASE + CPM_CPSPR_OFFSET)
#define CPM_CPSPPR       (CPM_BASE + CPM_CPSPPR_OFFSET)
#define CPM_USBPCR       (CPM_BASE + CPM_USBPCR_OFFSET)
#define CPM_USBPCR1       (CPM_BASE + CPM_USBPCR1_OFFSET)
#define CPM_USBRDT       (CPM_BASE + CPM_USBRDT_OFFSET)
#define CPM_USBVBFIL     (CPM_BASE + CPM_USBVBFIL_OFFSET)
#define CPM_USBCDR       (CPM_BASE + CPM_USBCDR_OFFSET)
#define CPM_I2SCDR       (CPM_BASE + CPM_I2SCDR_OFFSET)
#define CPM_LPCDR        (CPM_BASE + CPM_LPCDR_OFFSET)
#define CPM_MSC0CDR       (CPM_BASE + CPM_MSC0CDR_OFFSET)
#define CPM_MSC1CDR       (CPM_BASE + CPM_MSC1CDR_OFFSET)
#define CPM_MSC2CDR       (CPM_BASE + CPM_MSC2CDR_OFFSET)
#define CPM_BCHCDR       (CPM_BASE + CPM_BCHCDR_OFFSET)
#define CPM_UHCCDR       (CPM_BASE + CPM_UHCCDR_OFFSET)
#define CPM_SSICDR       (CPM_BASE + CPM_SSICDR_OFFSET)
#define CPM_CIMCDR       (CPM_BASE + CPM_CIMCDR_OFFSET)
#define CPM_GPSCDR       (CPM_BASE + CPM_GPSCDR_OFFSET)
#define CPM_PCMCDR       (CPM_BASE + CPM_PCMCDR_OFFSET)
#define CPM_GPUCDR       (CPM_BASE + CPM_GPUCDR_OFFSET)
#define CPM_PSWC0ST      (CPM_BASE + CPM_PSWC0ST_OFFSET)
#define CPM_PSWC1ST      (CPM_BASE + CPM_PSWC1ST_OFFSET)
#define CPM_PSWC2ST      (CPM_BASE + CPM_PSWC2ST_OFFSET)
#define CPM_PSWC3ST      (CPM_BASE + CPM_PSWC3ST_OFFSET)
#define CPM_SLBC	 (CPM_BASE + CPM_SLBC_OFFSET)
#define CPM_SLPC	 (CPM_BASE + CPM_SLPC_OFFSET)


/*
 * CPM registers common define
 */

/* Clock control register(CPCCR) */
#define CPCCR_ECS               BIT31
#define CPCCR_MEM               BIT30
#define CPCCR_CE                BIT22
#define CPCCR_PCS               BIT21

#define CPCCR_H1DIV_LSB		24
#define CPCCR_H1DIV_MASK		(0x0f << CPCCR_H1DIV_LSB)

#define CPCCR_H2DIV_LSB         16
#define CPCCR_H2DIV_MASK        BITS_H2L(19, CPCCR_H2DIV_LSB)

#define CPCCR_C1DIV_LSB		12
#define CPCCR_C1DIV_MASK	(0x0f << CPCCR_C1DIV_LSB)

#define CPCCR_PDIV_LSB		8
#define CPCCR_PDIV_MASK         BITS_H2L(11, CPCCR_PDIV_LSB)

#define CPCCR_H0DIV_LSB		4
#define CPCCR_H0DIV_MASK	(0x0f << CPCCR_H0DIV_LSB)

#define CPCCR_CDIV_LSB		0
#define CPCCR_CDIV_MASK         BITS_H2L(3, CPCCR_CDIV_LSB)

/* Low power control register(LCR) */
#define LCR_PDAHB1              BIT30
#define LCR_VBATIR              BIT29
#define LCR_PDGPS               BIT28
#define LCR_PDAHB1S             BIT26
#define LCR_PDGPSS              BIT24
#define LCR_DOZE                BIT2

#define LCR_PST_LSB             8
#define LCR_PST_MASK            BITS_H2L(19, LCR_PST_LSB)

#define LCR_DUTY_LSB            3
#define LCR_DUTY_MASK           BITS_H2L(7, LCR_DUTY_LSB)

#define LCR_LPM_LSB             0
#define LCR_LPM_MASK            BITS_H2L(1, LCR_LPM_LSB)
#define LCR_LPM_IDLE            (0x0 << LCR_LPM_LSB)
#define LCR_LPM_SLEEP           (0x1 << LCR_LPM_LSB)

/* Reset status register(RSR) */
#define RSR_P0R         BIT2
#define RSR_WR          BIT1
#define RSR_PR          BIT0

/* PLL control register 0(CPPCR0) */
#define CPPCR0_LOCK             BIT15   /* LOCK0 bit */
#define CPPCR0_ENLOCK           BIT14
#define CPPCR0_PLLS             BIT10
#define CPPCR0_PLLBP            BIT9
#define CPPCR0_PLLEN            BIT8

#define CPPCR0_PLLM_LSB         24
#define CPPCR0_PLLM_MASK        BITS_H2L(30, CPPCR0_PLLM_LSB)

#define CPPCR0_PLLN_LSB         18
#define CPPCR0_PLLN_MASK        BITS_H2L(22, CPPCR0_PLLN_LSB)

#define CPPCR0_PLLOD_LSB        16
#define CPPCR0_PLLOD_MASK       BITS_H2L(17, CPPCR0_PLLOD_LSB)

#define CPPCR0_PLLST_LSB        0
#define CPPCR0_PLLST_MASK       BITS_H2L(7, CPPCR0_PLLST_LSB)

/* PLL switch and status register(CPPSR) */
#define CPPSR_PLLOFF            BIT31
#define CPPSR_PLLBP             BIT30
#define CPPSR_PLLON             BIT29
#define CPPSR_PS                BIT28
#define CPPSR_FS                BIT27
#define CPPSR_CS                BIT26
#define CPPSR_SM                BIT2
#define CPPSR_PM                BIT1
#define CPPSR_FM                BIT0

/* Clock gate register 0(CGR0) */
#define CLKGR0_AHB_MON          BIT31
#define CLKGR0_DDR              BIT30
#define CLKGR0_IPU              BIT29
#define CLKGR0_LCD              BIT28
#define CLKGR0_TVE              BIT27
#define CLKGR0_CIM              BIT26
//#define CLKGR0_MDMA             BIT25
#define CLKGR0_I2C2		BIT25	//byzli
#define CLKGR0_UHC              BIT24
#define CLKGR0_MAC              BIT23
#define CLKGR0_GPS              BIT22
#define CLKGR0_DMAC             BIT21
#define CLKGR0_SSI2             BIT20
#define CLKGR0_SSI1             BIT19
#define CLKGR0_UART3            BIT18
#define CLKGR0_UART2            BIT17
#define CLKGR0_UART1            BIT16
#define CLKGR0_UART0            BIT15
#define CLKGR0_SADC             BIT14
#define CLKGR0_KBC              BIT13
#define CLKGR0_MSC2             BIT12
#define CLKGR0_MSC1             BIT11
#define CLKGR0_OWI              BIT10
#define CLKGR0_TSSI             BIT9
#define CLKGR0_AIC              BIT8
#define CLKGR0_SCC              BIT7
#define CLKGR0_I2C1             BIT6
#define CLKGR0_I2C0             BIT5
#define CLKGR0_SSI0             BIT4
#define CLKGR0_MSC0             BIT3
#define CLKGR0_OTG              BIT2
#define CLKGR0_BCH              BIT1
#define CLKGR0_NEMC             BIT0

/* Oscillator and power control register(OPCR) */
#define OPCR_OTGPHY_ENABLE      BIT7    /* SPENDN bit */
#define OPCR_GPSEN              BIT6
#define OPCR_UHCPHY_DISABLE     BIT5    /* SPENDH bit */
#define OPCR_O1SE               BIT4
#define OPCR_PD                 BIT3
#define OPCR_ERCS               BIT2

#define OPCR_O1ST_LSB           8
#define OPCR_O1ST_MASK          BITS_H2L(15, OPCR_O1ST_LSB)

/* Clock gate register 1(CGR1) */
//#define CLKGR1_I2C2             BIT15 //by zli
#define CLKGR1_AUX              BIT14
#define CLKGR1_I2S2             BIT13
//#define CLKGR1_OSD              BIT12		//by zli
#define CLKGR1_I2C4		BIT12
#define CLKGR1_UART4            BIT11
#define CLKGR1_PCM1             BIT10
#define CLKGR1_GPU              BIT9
#define CLKGR1_PCM0             BIT8
#define CLKGR1_VPU              BIT7
#define CLKGR1_CABAC            BIT6
#define CLKGR1_SRAM             BIT5
#define CLKGR1_DCT              BIT4
#define CLKGR1_ME               BIT3
#define CLKGR1_DBLK             BIT2
#define CLKGR1_MC               BIT1
//#define CLKGR1_BDMA             BIT0		//by zli
#define CLKGR1_I2C3		BIT0

/* PLL control register 1(CPPCR1) */
#define CPPCR1_P1SCS            BIT15
#define CPPCR1_PLL1EN           BIT7
#define CPPCR1_PLL1S            BIT6
#define CPPCR1_LOCK             BIT2    /* LOCK1 bit */
#define CPPCR1_PLL1OFF          BIT1
#define CPPCR1_PLL1ON           BIT0

#define CPPCR1_PLL1M_LSB        24
#define CPPCR1_PLL1M_MASK       BITS_H2L(30, CPPCR1_PLL1M_LSB)

#define CPPCR1_PLL1N_LSB        18
#define CPPCR1_PLL1N_MASK       BITS_H2L(21, CPPCR1_PLL1N_LSB)

#define CPPCR1_PLL1OD_LSB       16
#define CPPCR1_PLL1OD_MASK      BITS_H2L(17, CPPCR1_PLL1OD_LSB)

#define CPPCR1_P1SDIV_LSB       9
#define CPPCR1_P1SDIV_MASK      BITS_H2L(14, CPPCR1_P1SDIV_LSB)

/* CPM scratch pad protected register(CPSPPR) */
#define CPSPPR_CPSPR_WRITABLE   (0x00005a5a)

/* OTG parameter control register(USBPCR) */
#define USBPCR_USB_MODE         BIT31
#define USBPCR_AVLD_REG         BIT30
#define USBPCR_INCRM            BIT27   /* INCR_MASK bit */
#define USBPCR_CLK12_EN         BIT26
#define USBPCR_COMMONONN        BIT25
#define USBPCR_VBUSVLDEXT       BIT24
#define USBPCR_VBUSVLDEXTSEL    BIT23
#define USBPCR_POR              BIT22
#define USBPCR_SIDDQ            BIT21
#define USBPCR_OTG_DISABLE      BIT20
#define USBPCR_TXPREEMPHTUNE    BIT6

#define USBPCR_IDPULLUP_LSB             28   /* IDPULLUP_MASK bit */
#define USBPCR_IDPULLUP_MASK            BITS_H2L(29, USBPCR_IDPULLUP_LSB)

#define USBPCR_COMPDISTUNE_LSB          17
#define USBPCR_COMPDISTUNE_MASK         BITS_H2L(19, USBPCR_COMPDISTUNE_LSB)

#define USBPCR_OTGTUNE_LSB              14
#define USBPCR_OTGTUNE_MASK             BITS_H2L(16, USBPCR_OTGTUNE_LSB)

#define USBPCR_SQRXTUNE_LSB             11
#define USBPCR_SQRXTUNE_MASK            BITS_H2L(13, USBPCR_SQRXTUNE_LSB)

#define USBPCR_TXFSLSTUNE_LSB           7
#define USBPCR_TXFSLSTUNE_MASK          BITS_H2L(10, USBPCR_TXFSLSTUNE_LSB)

#define USBPCR_TXRISETUNE_LSB           4
#define USBPCR_TXRISETUNE_MASK          BITS_H2L(5, USBPCR_TXRISETUNE_LSB)

#define USBPCR_TXVREFTUNE_LSB           0
#define USBPCR_TXVREFTUNE_MASK          BITS_H2L(3, USBPCR_TXVREFTUNE_LSB)


#define USBPCR1_USB_SEL         BIT28
#define USBPCR1_WORD_IF0        BIT19 

#define USBPCR1_UHC_POWON		BIT5
/* OTG reset detect timer register(USBRDT) */
#define USBRDT_VBFIL_LD_EN      BIT25
#define USBRDT_IDDIG_EN         BIT24
#define USBRDT_IDDIG_REG        BIT23

#define USBRDT_USBRDT_LSB       0
#define USBRDT_USBRDT_MASK      BITS_H2L(22, USBRDT_USBRDT_LSB)

/* OTG PHY clock divider register(USBCDR) */
#define USBCDR_UCS              BIT31
#define USBCDR_UPCS             BIT30

#define USBCDR_OTGDIV_LSB       0       /* USBCDR bit */
#define USBCDR_OTGDIV_MASK      BITS_H2L(5, USBCDR_OTGDIV_LSB)

/* I2S device clock divider register(I2SCDR) */
#define I2SCDR_I2CS             BIT31
#define I2SCDR_I2PCS            BIT30

#define I2SCDR_I2SDIV_LSB       0       /* I2SCDR bit */
#define I2SCDR_I2SDIV_MASK      BITS_H2L(8, I2SCDR_I2SDIV_LSB)

/* LCD pix clock divider register(LPCDR) */
#define LPCDR_LTCS              BIT30
#define LPCDR_LPCS              BIT29

#define LPCDR_PIXDIV_LSB        0       /* LPCDR bit */
#define LPCDR_PIXDIV_MASK       BITS_H2L(10, LPCDR_PIXDIV_LSB)

/* MSC clock divider register(MSCCDR) */
#define MSCCDR_MCS              BIT31
#define MSCCDR_MPCS		BIT30

#define MSCCDR_MSCDIV_LSB       0       /* MSCCDR bit */
#define MSCCDR_MSCDIV_MASK      BITS_H2L(6, MSCCDR_MSCDIV_LSB)

/* UHC device clock divider register(UHCCDR) */
#define UHCCDR_UHPCS            BIT29
#define UHCCDR_UHCS		BIT30 	/* pll1 as default */

#define UHCCDR_UHCDIV_LSB       0       /* UHCCDR bit */
#define UHCCDR_UHCDIV_MASK      BITS_H2L(3, UHCCDR_UHCDIV_LSB)

/* SSI clock divider register(SSICDR) */
#define SSICDR_SCS              BIT31

#define SSICDR_SSIDIV_LSB       0       /* SSICDR bit */
#define SSICDR_SSIDIV_MASK      BITS_H2L(5, SSICDR_SSIDIV_LSB)

/* CIM mclk clock divider register(CIMCDR) */
#define CIMCDR_CIMDIV_LSB       0       /* CIMCDR bit */
#define CIMCDR_CIMDIV_MASK      BITS_H2L(7, CIMCDR_CIMDIV_LSB)

/* GPS clock divider register(GPSCDR) */
#define GPSCDR_GPCS             BIT31

#define GPSCDR_GPSDIV_LSB       0       /* GPSCDR bit */
#define GSPCDR_GPSDIV_MASK      BITS_H2L(3, GPSCDR_GPSDIV_LSB)

/* PCM device clock divider register(PCMCDR) */
#define PCMCDR_PCMS             BIT31
#define PCMCDR_PCMPCS           BIT30

#define PCMCDR_PCMDIV_LSB       0       /* PCMCDR bit */
#define PCMCDR_PCMDIV_MASK      BITS_H2L(8, PCMCDR_PCMDIV_LSB)

/* GPU clock divider register */
#define GPUCDR_GPCS             BIT31
#define GPUCDR_GPUDIV_LSB       0       /* GPUCDR bit */
#define GPUCDR_GPUDIV_MASK      BITS_H2L(2, GPUCDR_GPUDIV_LSB)

/* BCH clock divider register */
#define CPM_BCHCDR_BPCS			(1 << 31)
#define CPM_BCHCDR_BCHM			(1 << 30) //0: hardware as default, 1: software(must change)
#define CPM_BCHCDR_BCHDIV_BIT		0
#define CPM_BCHCDR_BCHDIV_MASK		(0x7 << CPM_BCHCDR_BCHDIV_BIT)

#ifndef __MIPS_ASSEMBLER

#define REG_CPM_CPCCR           REG32(CPM_CPCCR)
#define REG_CPM_RSR             REG32(CPM_RSR)
#define REG_CPM_CPPCR0          REG32(CPM_CPPCR0)
#define REG_CPM_CPPSR           REG32(CPM_CPPSR)
#define REG_CPM_CPPCR1          REG32(CPM_CPPCR1)
#define REG_CPM_CPSPR           REG32(CPM_CPSPR)
#define REG_CPM_CPSPPR          REG32(CPM_CPSPPR)
#define REG_CPM_USBPCR          REG32(CPM_USBPCR)
#define REG_CPM_USBPCR1         REG32(CPM_USBPCR1)
#define REG_CPM_USBRDT          REG32(CPM_USBRDT)
#define REG_CPM_USBVBFIL        REG32(CPM_USBVBFIL)
#define REG_CPM_USBCDR          REG32(CPM_USBCDR)
#define REG_CPM_I2SCDR          REG32(CPM_I2SCDR)
#define REG_CPM_LPCDR           REG32(CPM_LPCDR)
#define REG_CPM_MSCCDR          REG32(CPM_MSCCDR)
#define REG_CPM_BCHCDR          REG32(CPM_BCHCDR)
#define REG_CPM_UHCCDR          REG32(CPM_UHCCDR)
#define REG_CPM_SSICDR          REG32(CPM_SSICDR)
#define REG_CPM_CIMCDR          REG32(CPM_CIMCDR)
#define REG_CPM_GPSCDR          REG32(CPM_GPSCDR)
#define REG_CPM_PCMCDR          REG32(CPM_PCMCDR)
#define REG_CPM_GPUCDR          REG32(CPM_GPUCDR)

#define REG_CPM_PSWC0ST         REG32(CPM_PSWC0ST)
#define REG_CPM_PSWC1ST         REG32(CPM_PSWC1ST)
#define REG_CPM_PSWC2ST         REG32(CPM_PSWC2ST)
#define REG_CPM_PSWC3ST         REG32(CPM_PSWC3ST)

#define REG_CPM_LCR             REG32(CPM_LCR)
#define REG_CPM_CLKGR0          REG32(CPM_CLKGR0)
#define REG_CPM_OPCR            REG32(CPM_OPCR)
#define REG_CPM_CLKGR1          REG32(CPM_CLKGR1)
#define REG_CPM_CLKGR           REG32(CPM_CLKGR0)

#define __cpm_suspend_otg_phy()		(REG_CPM_OPCR &= ~OPCR_OTGPHY_ENABLE)
#define __cpm_enable_otg_phy()		(REG_CPM_OPCR |=  OPCR_OTGPHY_ENABLE)

#endif
