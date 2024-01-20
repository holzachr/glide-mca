/*
 * /dev/3dfx device for Linux kernels with MTRR settings enabled.
 *
 * Original by Daryll Straus.
 * Port to 2.1 kernel by Jon Taylor.
 * setmtrr_3dfx() added by Jens Axboe.
 * Combining 2.0 and 2.1/2.2 kernels into one rpm,
 *   resetmtrr_3df(), and using correct pci calls for
 *   2.1/2.2 kernels by Carlo Wood.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 */

/*
 * Include this first as it defines things that affect the kernel headers.
 */

#include <linux/version.h>

#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
#include <generated/autoconf.h>
#else
#include <linux/autoconf.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,1,115)
/*
 * It might work with smaller kernels, but I never tested that.
 */
#error "Upgrade your kernel"
#endif

#ifdef MODULE
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#if defined(CONFIG_MODVERSIONS) && !defined(MODVERSIONS)
#define MODVERSIONS
#endif
#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif
#define mod_inc_use_count MOD_INC_USE_COUNT
#define mod_dec_use_count MOD_DEC_USE_COUNT
#else
#define mod_inc_use_count
#define mod_dec_use_count
#endif
#include <linux/module.h>
#else
#define mod_inc_use_count
#define mod_dec_use_count
#endif

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/mca.h>
#include <linux/pci.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,46) && \
    LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,17) || \
    defined(DEVFS_SUPPORT)
#include <linux/devfs_fs_kernel.h>
#define HAVE_DEVFS 1
#else
#define HAVE_DEVFS 0
#endif
#include <asm/segment.h>
#include <asm/ioctl.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/processor.h>

// In Micro Channel world, a K6-2 CPU or higher is very unlikely.
//TODO port and test on PC 750 w/ K6-III CPU.
#ifdef CONFIG_MTRR
#undef CONFIG_MTRR
#endif
#include <asm/uaccess.h>
#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif

/* The Micro Channel Adapter ID */
#define ADAPTER_MCA_ID                  0x8F63
#define ADAPTER_NAME                   "Adaptec ASIC-9060R MCA/PCI bridge with 3dfx Voodoo graphics"

/* The ASIC puts the daughter card at an offset of
   0x1000 above the ASICs own MCA IO space */
#define ASIC_IO_OFFSET                  0x00001000

/* The ASIC's register to write the PCI_CONFIG_CMD
   to to access the daughter card's PCI config space */
#define ASIC_PCI_CONFIG_CMD_REGISTER    0x04

/* Magic bit to access the daughter card's PCI
   configuration space */
#define ASIC_PCI_CONFIG_CMD             0x01

/* The ASIC's daughter card IO tunnel address
   register to write the daughter card's register address to */
#define ASIC_IO_ADDRESS_REGISTER        0x08

/* The ASIC's daughter card IO tunnel data
   register to read/write the daughter card's selected register */
#define ASIC_IO_DATA_REGISTER           0x0C

/* The ASIC's interrupt enable/disable register */
#define ASIC_IRQ_ENABLE_REGISTER        0x1C

/* The ASIC's index register for accessing
    the VPD data, byte by byte */
#define ASIC_VPD_INDEX_REGISTER         0x24

/* The ASIC's <unknown> register, related to
   the VPD data. "VPD EEPROM operation ok" indicator? */
#define ASIC_VPD_STATUS_REGISTER        0x25

/* The ASIC's data register for reading
   the VPD data, byte by byte or short by short */
#define ASIC_VPD_DATA_REGISTER          0x26

#define ASIC_IO_SIZE					0x400

#define MAJOR_3DFX 107
#define DEVICE_VOODOO 0

#define PCI_VENDOR_ID_LINUX 0x0
#define PCI_DEVICE_ID_LINUX 0x2
#define PCI_COMMAND_LINUX 0x4
#define PCI_REVISION_ID_LINUX 0x8
#define PCI_BASE_ADDRESS_0_LINUX 0x10
#define PCI_BASE_ADDRESS_1_LINUX 0x14
#define PCI_BASE_ADDRESS_2_LINUX 0x18
#define SST1_PCI_SPECIAL1_LINUX 0x40
#define SST1_PCI_SPECIAL2_LINUX 0x44
#define SST1_PCI_SPECIAL3_LINUX 0x48
#define SST1_PCI_SPECIAL4_LINUX 0x54

#define VGA_INPUT_STATUS_1C 0x3DA
#define VGA_MISC_OUTPUT_READ 0x3cc
#define VGA_MISC_OUTPUT_WRITE 0x3c2
#define SC_INDEX 0x3c4
#define SC_DATA  0x3c5

#ifndef PCI_VENDOR_ID_3DFX
#define PCI_VENDOR_ID_3DFX 0x121a
#endif

#ifndef PCI_VENDOR_ID_ALLIANCE
#define PCI_VENDOR_ID_ALLIANCE 0x1142
#endif

#ifndef PCI_DEVICE_ID_3DFX_VOODOO
#define PCI_DEVICE_ID_3DFX_VOODOO 1
#endif

#ifndef PCI_VENDOR_ID_MACRONIX
#define PCI_VENDOR_ID_MACRONIX 0x10d9
#endif

#ifndef PCI_DEVICE_ID_3DFX_VOODOO2
#define PCI_DEVICE_ID_3DFX_VOODOO2 2
#endif

#ifndef PCI_DEVICE_ID_ALLIANCE_AT3D
#define PCI_DEVICE_ID_ALLIANCE_AT3D 0x643d
#endif

#ifndef PCI_DEVICE_ID_MACRONIX_MX86251
#define PCI_DEVICE_ID_MACRONIX_MX86251 0x8626
#endif

#ifndef PCI_DEVICE_ID_3DFX_BANSHEE
#define PCI_DEVICE_ID_3DFX_BANSHEE 3
#endif

#ifndef PCI_DEVICE_ID_3DFX_VOODOO3
#define PCI_DEVICE_ID_3DFX_VOODOO3 5
#endif

#ifndef PCI_DEVICE_ID_3DFX_VOODOO4
#define PCI_DEVICE_ID_3DFX_VOODOO4 9
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,73)
#define pci_get_device pci_find_device
#define pci_dev_put(dev)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static char name_3dfx[] = "3dfx";

static struct pci_device_id id_table_3dfx[] = {
	{PCI_VENDOR_ID_3DFX,		PCI_DEVICE_ID_3DFX_VOODOO,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_3DFX,		PCI_DEVICE_ID_3DFX_VOODOO2,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ALLIANCE,	PCI_DEVICE_ID_ALLIANCE_AT3D,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_MACRONIX,	PCI_DEVICE_ID_MACRONIX_MX86251,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_3DFX,		PCI_DEVICE_ID_3DFX_BANSHEE,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_3DFX,		PCI_DEVICE_ID_3DFX_VOODOO3,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_3DFX,		PCI_DEVICE_ID_3DFX_VOODOO4,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ 0, }
};
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) */
static struct pci_card {
	unsigned short vendor;
	unsigned short device;
} pci_card_list[] = {
	{PCI_VENDOR_ID_3DFX,		PCI_DEVICE_ID_3DFX_VOODOO},
	{PCI_VENDOR_ID_3DFX,		PCI_DEVICE_ID_3DFX_VOODOO2},
	{PCI_VENDOR_ID_ALLIANCE,	PCI_DEVICE_ID_ALLIANCE_AT3D},
	{PCI_VENDOR_ID_MACRONIX,	PCI_DEVICE_ID_MACRONIX_MX86251},
	{PCI_VENDOR_ID_3DFX,		PCI_DEVICE_ID_3DFX_BANSHEE},
	{PCI_VENDOR_ID_3DFX,		PCI_DEVICE_ID_3DFX_VOODOO3},
	{PCI_VENDOR_ID_3DFX,		PCI_DEVICE_ID_3DFX_VOODOO4}
};
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) */

//#ifdef DEBUG
#define DEBUGMSG(x) printk x
//#else
//TODO #define DEBUGMSG(x)
//#endif

/*
 * This macro is for accessing vma->vm_offset or vma->vm_pgoff depending
 * on kernel version.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,14)
#define VM_OFFSET(vma) (vma->vm_offset)
#define VM_OFFSET_ALIGNED(vma) ((vma->vm_offset) & ~PAGE_MASK)
#else
#define VM_OFFSET(vma) (vma->vm_pgoff << PAGE_SHIFT)
#define VM_OFFSET_ALIGNED(vma) ((vma->vm_pgoff) & ~PAGE_MASK)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)
#define my_remap_page_range(vma, start, ofs, len, prot) \
	remap_pfn_range(vma, start, ofs >> PAGE_SHIFT, len, prot)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define my_remap_page_range(vma, start, ofs, len, prot) \
	remap_page_range(vma, start, ofs, len, prot)
#else
#define my_remap_page_range(vma, start, ofs, len, prot) \
	remap_page_range(start, ofs, len, prot)
#endif

struct pioData_t {
	short port;
	short size;
	int device;
	void *value;
};

struct cardInfo_t {
	int vendor;
	int type;
	int addr0;
	int addr1;
	int addr2;
	int memsize;
	unsigned int ioaddr;
	int slot;
	struct file *curFile;
#ifdef CONFIG_MTRR
	int mtrr_buf;
	int mtrr_ctl;
#endif
};

typedef struct pioData_t pioData;
typedef struct cardInfo_t cardInfo;

#ifdef MODULE
void cleanup_module(void);
#endif

#define MAXCARDS 1

static cardInfo cards[MAXCARDS];
static int numCards = 0;

static int asic_read_pci_config(unsigned long ioaddr, int index)
{
    outb(ASIC_PCI_CONFIG_CMD, ioaddr + ASIC_PCI_CONFIG_CMD_REGISTER);
    outl(index, ioaddr + ASIC_IO_ADDRESS_REGISTER);
    return (inl(ioaddr + ASIC_IO_DATA_REGISTER));
}

static void asic_write_pci_config(unsigned long ioaddr, int index, int val)
{
    outb(ASIC_PCI_CONFIG_CMD, ioaddr + ASIC_PCI_CONFIG_CMD_REGISTER);
    outl(index, ioaddr + ASIC_IO_ADDRESS_REGISTER);
    outl(val, ioaddr + ASIC_IO_DATA_REGISTER);
}

static void asic_enable_interrupt(unsigned long ioaddr)
{
    outb(1, ioaddr + ASIC_IRQ_ENABLE_REGISTER);
}

static void asic_disable_interrupt(unsigned long ioaddr)
{
    outb(0, ioaddr + ASIC_IRQ_ENABLE_REGISTER);
}

static void init_asic(unsigned long ioaddr)
{
    int i, temp;		//TODO remove

    /* This sequence of writes goes out to the ASIC
       and is required to start it up.
       IBM only knows what they mean. */
    outb(0x00,       ioaddr + 0x1D);
    outb(0x4F,       ioaddr + 0x1E);
    outb(0x04,       ioaddr + 0x1F);
    outl(0x00000000, ioaddr + 0x28);
    outw(0x0006,     ioaddr + 0x00);
    outl(0x00000000, ioaddr + 0x10);    //TODO BAR0 base/size? Decode-Mask like 0x3FFFFFFF ?
    //outl(0x3FFFFFFF, ioaddr + 0x10);    //TODO BAR0 base/size? Decode-Mask like 0x3FFFFFFF ?
    //outl(0x40000000, ioaddr + 0x10);    //TODO BAR0 base/size? Decode-Mask like 0x3FFFFFFF ?
    outl(0x00000000, ioaddr + 0x14);    //TODO BAR1 base/size?
    outw(0x0FFF,     ioaddr + 0x1A);
    outb(0x7F,       ioaddr + 0x22);
    outw(0x03FF,     ioaddr + 0x20);    //TODO IO decode mask? No, does not look like it.
    
    //for (i = 0; i < 0x400; i += 4)
    //{
    //    temp = inl(ioaddr + i);
    //    printk("%8.8X ", temp);
    //    
    //    if ((i % 16) == 0)
    //        printk("\n");
    //} 

    //for (i = 0; i < 0x40; i += 4)		//TODO remove
    //{
    //    temp = asic_read_pci_config(ioaddr, i);
    //    DEBUGMSG(("PCI config 0x%2.2X: 0x%8.8X\n", i, temp));
    //}
}

#if HAVE_DEVFS
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,46) && \
    LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) || \
    defined(DEVFS_SUPPORT)
static devfs_handle_t devfs_handle;
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
static void findMcaCardType(int vendor, int device)
{
	unsigned int ioaddr = 0;
    unsigned int  irq_line = 0;
    unsigned int  dma = 0;
    int cards_found = 0;
    int slot = MCA_NOTFOUND;
	unsigned int viddid, control, membase, temp, memsize;
    int i;  //TODO remove

    if(ioaddr != 0)
        return ;

    slot = mca_find_unused_adapter(ADAPTER_MCA_ID, 0);
    if (slot != MCA_NOTFOUND)
    {
        /* Upper 6 bits of POS[2] contain the IO base * 0x100) */
        ioaddr = ((mca_read_pos(slot, 2) & 0xFC)) << 8;

        /* Lower 2 bits of POS[5] encode the IRQ */
        switch (mca_read_pos(slot, 5) & 0x03)
        {
            case 0x00: irq_line = 15; break;
            case 0x01: irq_line = 12; break;
            case 0x02: irq_line = 11; break;
            default:   irq_line = 10; break;
        }

        /* Upper 4 bits of POS[3] contain the DMA arbitration level.
           Unused from CPU side, but reserved by POS for the daughter card
           busmaster to use */
        dma = mca_read_pos(slot, 3) >> 4;

        DEBUGMSG(("%s found in MCA slot %d using I/O 0x%4.4lX, IRQ %u, DMA %d\n",
            ADAPTER_NAME, slot, ioaddr, irq_line, dma));

        init_asic(ioaddr);

		viddid = asic_read_pci_config(ioaddr, PCI_VENDOR_ID_LINUX);

		if (  ((viddid & 0xFFFF) == vendor)
			&&((viddid > 16)     == device))
		{
			temp = asic_read_pci_config(ioaddr, PCI_INTERRUPT_LINE);
			temp |= 256;	// Disable interrupts
			asic_write_pci_config(ioaddr, PCI_INTERRUPT_LINE, temp);

			// We don't have a real PCI BIOS to set MEMEN and memory mapping base for us.
			// We are the BIOS now! -> Let's do it manually.

			// No Micro Channel x86 computer known to me has more than 256 MB of RAM.
			// Let's place the memory window at 1 GB, this is where the PCI BIOS
			// of my IBM PC 750 sets it when I put the 3dfx card in a PCI slot.
            // Let's hope it's free.
            membase = 0x40000000;
			asic_write_pci_config(ioaddr, PCI_BASE_ADDRESS_0_LINUX, membase | (1 << 3));    //TODO check if memory is used?
			asic_write_pci_config(ioaddr, PCI_BASE_ADDRESS_1_LINUX, 0x00000000);

            cards[numCards].addr0 = membase;
            cards[numCards].addr1 = 0x00000000;
			
			/* Optional, determining BAR size:
             * From the PCI Specification v2.2:
			 * Implementation Note: Sizing a 32 bit Base Address Register Example */
			temp = asic_read_pci_config(ioaddr, PCI_BASE_ADDRESS_0_LINUX);
			asic_write_pci_config(ioaddr, PCI_BASE_ADDRESS_0_LINUX, 0xFFFFFFF0);
			memsize = asic_read_pci_config(ioaddr, PCI_BASE_ADDRESS_0_LINUX);
			memsize &= 0xFFFFFFF0;		// Ignore encoding information bits
			memsize ^= 0xFFFFFFFF;		// Invert all bits
			memsize += 1;				// This is the memory range decoded by the register!
            DEBUGMSG(("3dfx: BAR size is %xh\n", memsize));
            asic_write_pci_config(ioaddr, PCI_BASE_ADDRESS_0_LINUX, temp);            

			control = asic_read_pci_config(ioaddr, PCI_COMMAND_LINUX);
			control |= (1 << 1); 	// Set MEMEN bit to enable memory address decoding
            //control &= ~(1 << 1); 	// Set MEMEN bit to enable memory address decoding //TODO remove
			asic_write_pci_config(ioaddr, PCI_COMMAND_LINUX, control);

            cards[numCards].memsize = memsize;
			cards[numCards].ioaddr = ioaddr;
			cards[numCards].slot   = slot;
            cards[numCards].addr0 &= ~0xF;
            cards[numCards].addr1 &= ~0xF;
            cards[numCards].vendor = (viddid & 0xFFFF);
            cards[numCards].type = (viddid > 16);
            cards[numCards].curFile = 0;

            DEBUGMSG(("3dfx: board vendor %4.4x type %4.4x located at %xh/%xh\n",
                vendor, device, cards[numCards].addr0, cards[numCards].addr1));
                
            for (i = membase; i < (membase + 0x100); i += 4)
            {
                int * p = (int *)i;                
                printk("%8.8X ", (int)(*(int *)i));
                
                //if (((i + 1) % 16) == 0)
                //    printk("\n");
            } 

            mca_set_adapter_name(slot, ADAPTER_NAME);
            mca_mark_as_used(slot);
            request_region(ioaddr, ASIC_IO_SIZE, ADAPTER_NAME);
            request_mem_region(membase, memsize, ADAPTER_NAME);

            ++numCards;
		}
    }
}

static int findCards(void)
{
	int i;

	numCards = 0;
	for (i = 0; i < (sizeof(pci_card_list)/sizeof(struct pci_card)); i++)
		findMcaCardType(pci_card_list[i].vendor, pci_card_list[i].device);

	return numCards;
}
#else
static struct pci_driver driver_3dfx;

static int findCards(void)
{
	int error;

	error = pci_register_driver(&driver_3dfx);

	if (error)
		return error < 0 ? error : -error;
	else
		return numCards;
}
#endif

static int open_3dfx(struct inode *inode, struct file *file)
{
	DEBUGMSG(("3dfx: Entering open_3dfx\n"));

	if (!numCards) {
		printk("3dfx: No 3Dfx cards found\n");
		return -ENODEV;
	}
	mod_inc_use_count;

	return 0;
}

static int release_3dfx(struct inode *inode, struct file *file)
{
	int i;

	DEBUGMSG(("3dfx: Entering release_3dfx\n"));

	for (i = 0; i < numCards; ++i)
		if (cards[i].curFile == file)
			cards[i].curFile = 0;
	mod_dec_use_count;

	return 0;
}

static int mmap_3dfx(struct file *file, struct vm_area_struct *vma)
{
	size_t len;
	int i;

	DEBUGMSG(("3dfx: Entering mmap_3dfx\n"));

	for (i = 0; i < numCards; ++i) {
		if ((cards[i].addr0 == VM_OFFSET(vma)) ||
		    (cards[i].addr1 == VM_OFFSET(vma)))
			break;
	}
	if (i == numCards) {
		DEBUGMSG(("3dfx: Couldn't match address %lx to a card\n",
			 VM_OFFSET(vma)));
		return -EPERM;
	}
	if (VM_OFFSET_ALIGNED(vma)) {
		DEBUGMSG(("3dfx: Map request not page aligned\n"));
		return -ENXIO;
	}
	len = vma->vm_end - vma->vm_start;
	if ((len < 0) || (len > 0x2000000)) {
		DEBUGMSG(("3dfx: Invalid mapping size requested\n"));
		return -EINVAL;
	}
#if defined(__i386__)
	pgprot_val(vma->vm_page_prot) |= _PAGE_PCD;
#endif
	if (my_remap_page_range(vma, vma->vm_start, VM_OFFSET(vma), len,
			        vma->vm_page_prot)) {
		DEBUGMSG(("3dfx: Page remap failed\n"));
		return -EAGAIN;
	}
	return 0;
}

static int doQueryBoards(void)
{
	return numCards;
}

static int doQueryFetch(pioData *desc)
{
	char retchar;
	short retword;
	int retlong;
	int i;
	int temp;

	if (desc->device < 0 || desc->device >= numCards)
		return -EINVAL;
	i = desc->device;

	switch (desc->port) {
	case PCI_VENDOR_ID_LINUX:
		if (desc->size != 2)
			return -EINVAL;
		if (copy_to_user(desc->value, &cards[i].vendor, desc->size))
			return -EFAULT;
		return 0;
	case PCI_DEVICE_ID_LINUX:
		if (desc->size != 2)
			return -EINVAL;
		if (copy_to_user(desc->value, &cards[i].type, desc->size))
			return -EFAULT;
		return 0;
	case PCI_BASE_ADDRESS_0_LINUX:
		if (desc->size != 4)
			return -EINVAL;
		if (copy_to_user(desc->value, &cards[i].addr0, desc->size))
			return -EFAULT;
		return 0;
	case PCI_BASE_ADDRESS_1_LINUX:
		if (desc->size != 4)
			return -EINVAL;
		if (copy_to_user(desc->value, &cards[i].addr1, desc->size))
			return -EFAULT;
		return 0;
	case PCI_BASE_ADDRESS_2_LINUX:
		if (desc->size != 4)
			return -EINVAL;
		if (copy_to_user(desc->value, &cards[i].addr2, desc->size))
			return -EFAULT;
		return 0;
	case SST1_PCI_SPECIAL1_LINUX:
		if (desc->size != 4)
			return -EINVAL;
		break;
	case PCI_REVISION_ID_LINUX:
		if (desc->size != 1)
			return -EINVAL;
		break;
	case SST1_PCI_SPECIAL4_LINUX:
		if (desc->size != 4)
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	switch (desc->size) {
	case 1:
		temp = asic_read_pci_config(cards[desc->device].ioaddr, desc->port);
		retchar = temp & 0xFF;
		if (copy_to_user(desc->value, &retchar, 1))
			return -EFAULT;
		break;
	case 2:
		temp = asic_read_pci_config(cards[desc->device].ioaddr, desc->port);
		retword = temp & 0xFFFF;
		if (copy_to_user(desc->value, &retword, 2))
			return -EFAULT;
		break;
	case 4:
		retlong = asic_read_pci_config(cards[desc->device].ioaddr, desc->port);
		if (copy_to_user(desc->value, &retlong, 4))
			return -EFAULT;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int doQueryUpdate(pioData *desc)
{
	int retval;
	int preval;
	int mask;
	char retchar;
	short retword;
	int retlong;

	if (desc->device < 0 || desc->device >= numCards)
		return -EINVAL;

	switch (desc->port) {
	case PCI_COMMAND_LINUX:
		if (desc->size != 2)
			return -EINVAL;
		break;
	case SST1_PCI_SPECIAL1_LINUX:
		if (desc->size != 4)
			return -EINVAL;
		break;
	case SST1_PCI_SPECIAL2_LINUX:
		if (desc->size != 4)
			return -EINVAL;
		break;
	case SST1_PCI_SPECIAL3_LINUX:
		if (desc->size != 4)
			return -EINVAL;
		break;
	case SST1_PCI_SPECIAL4_LINUX:
		if (desc->size != 4)
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	retval = asic_read_pci_config(cards[desc->device].ioaddr, desc->port & ~0x3);
	switch (desc->size) {
	case 1:
		if (copy_from_user(&retchar, desc->value, 1))
			return -EFAULT;
		preval = retchar << (8 * (desc->port & 0x3));
		mask = 0xFF << (8 * (desc->port & 0x3));
		break;
	case 2:
		if (copy_from_user(&retword, desc->value, 2))
			return -EFAULT;
		preval = retword << (8 * (desc->port & 0x3));
		mask = 0xFFFF << (8 * (desc->port & 0x3));
		break;
	case 4:
		if (copy_from_user(&retlong, desc->value, 4))
			return -EFAULT;
		preval = retlong;
		mask = ~0;
		break;
	default:
		return -EINVAL;
	}

	retval = (retval & ~mask) | preval;
	asic_write_pci_config(cards[desc->device].ioaddr, desc->port, retval);

	return 0;
}

static int doQuery(unsigned int cmd, unsigned long arg)
{
	pioData desc;

	if (_IOC_NR(cmd) == 2)
		return doQueryBoards();
	if (copy_from_user(&desc, (void *)arg, sizeof(pioData)))
		return -EFAULT;
	if (_IOC_NR(cmd) == 3)
		return doQueryFetch(&desc);
	if (_IOC_NR(cmd) == 4)
		return doQueryUpdate(&desc);

	return -EINVAL;
}

static int doPIORead(pioData *desc)
{
	int retchar;

	/* full SSTIO aperture is defined by:
	 * unsigned short port = desc->port;
	 * unsigned short base = cards[i].addr2 & ~1;
	 * (base <= port && port <= (base + 0x108 - desc->size))
	 */
	switch (desc->port) {
	case VGA_INPUT_STATUS_1C:
		break;
	case SC_INDEX:
		break;
	case SC_DATA:
		break;
	case VGA_MISC_OUTPUT_READ:
		break;
	default:
		return -EPERM;
	}

	switch (desc->size) {
	case 1:
		retchar = inb(desc->port);
		break;
	case 2:
		retchar = inw(desc->port);
		break;
	case 4:
		retchar = inl(desc->port);
		break;
	default:
		return -EINVAL;
	}

	if (copy_to_user(desc->value, &retchar, desc->size))
		return -EFAULT;

	return 0;
}

static int doPIOWrite(pioData *desc)
{
	int retchar;

	/* full SSTIO aperture is defined by:
	 * unsigned short port = desc->port;
	 * unsigned short base = cards[i].addr2 & ~1;
	 * (base <= port && port <= (base + 0x108 - desc->size))
	 */
	switch (desc->port) {
	case SC_INDEX:
		break;
	case SC_DATA:
		break;
	case VGA_MISC_OUTPUT_WRITE:
		break;
	default:
		return -EPERM;
	}

	if (copy_from_user(&retchar, desc->value, desc->size))
		return -EFAULT;

	switch (desc->size) {
	case 1:
		outb(retchar, desc->port);
		break;
	case 2:
		outw(retchar, desc->port);
		break;
	case 4:
		outl(retchar, desc->port);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int doPIO(unsigned int cmd, unsigned long arg)
{
	pioData desc;

	if (copy_from_user(&desc, (void *)arg, sizeof(pioData)))
		return -EFAULT;
	if (_IOC_DIR(cmd) == _IOC_READ)
		return doPIORead(&desc);
	if (_IOC_DIR(cmd) == _IOC_WRITE)
		return doPIOWrite(&desc);

	return -EINVAL;
}

#ifdef HAVE_UNLOCKED_IOCTL
static long ioctl_3dfx(struct file *file, unsigned int cmd, unsigned long arg)
#else
static int ioctl_3dfx(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
#endif
{
#ifdef HAVE_UNLOCKED_IOCTL
	DEBUGMSG(("3dfx: Entering ioctl_3dfx, file %p cmd %x arg %lx\n",
	          file, cmd, arg));
#else
	DEBUGMSG(("3dfx: Entering ioctl_3dfx, inode %p file %p cmd %x arg %lx\n", inode, file, cmd, arg));
#endif

	switch (_IOC_TYPE(cmd)) {
	case '3':
		return doQuery(cmd, arg);
	case 0:
		return doPIO(cmd, arg);
	default:
		DEBUGMSG(("3dfx: Unknown 3dfx request made\n"));
		return -EINVAL;
	}
}

#ifdef CONFIG_MTRR
static int setmtrr_3dfx(void)
{
	int i = 0, retval = -2;
	unsigned char dlc;

	/*
	 * First do a bios fixup if this system has a 82441FX chipset.
	 */
	struct pci_dev *dev = NULL;
	dev = pci_get_device(PCI_VENDOR_ID_INTEL,
	                     PCI_DEVICE_ID_INTEL_82371SB_0, dev);
	if (dev) {
		pci_read_config_byte(dev, 0x82, &dlc);
		if (!(dlc & 1 << 1)) {
			dlc |= 1 << 1;
			pci_write_config_byte(dev, 0x82, dlc);
			printk("3dfx: PIIX3: Enabling Passive Release\n");
		}
		pci_dev_put(dev);
	}

	/*
	 * Set up the mtrr's.
	 */
	if (numCards == 0)
		return -EIO;
	for (i = 0; i < numCards; i++) {
		if ((cards[i].vendor != PCI_VENDOR_ID_3DFX) ||
		    (cards[i].type > PCI_DEVICE_ID_3DFX_VOODOO3)) {
			/*
			 * Used as flag in resetmtrr_3dfx().
			 */
			cards[i].mtrr_buf = -1;
			continue;
		}
		switch (cards[i].type) {
		case PCI_DEVICE_ID_3DFX_VOODOO:
		case PCI_DEVICE_ID_3DFX_VOODOO2:
			/*
			 * Frame buffer to write combining.
			 */
			retval = cards[i].mtrr_buf = mtrr_add(cards[i].addr0, 0x400000, MTRR_TYPE_WRCOMB, 1);
			if (retval >= 0) {
				retval = cards[i].mtrr_ctl = mtrr_add(cards[i].addr0, 0x1000, MTRR_TYPE_UNCACHABLE, 1);
				if (retval < 0) {
					mtrr_del(cards[i].mtrr_buf, 0, 0);
					cards[i].mtrr_buf = -1;
				}
			}
			if (retval < 0) {
				printk("3dfx: Could not set MTRR for Voodoo card\n");
				/*
				 * Can still run.
				 */
				return 0;
			}
			break;
		case PCI_DEVICE_ID_3DFX_BANSHEE:
		case PCI_DEVICE_ID_3DFX_VOODOO3:
			retval = cards[i].mtrr_buf = mtrr_add(cards[i].addr1, 0x1000000, MTRR_TYPE_WRCOMB, 1);
			if (retval < 0) {
				printk("3dfx: Could not set MTRR for Voodoo card\n");
				/*
				 * Can still run
				 */
				return 0;
			}
			cards[i].mtrr_ctl = -1;
			break;
		default:
			/*
			 * We should never hit this
			 */
			break;
		}
	}
	if (retval == -2) {
		DEBUGMSG(("3dfx: Could not set MTRR for this graphics card\n"));
		/*
		 * Can still run
		 */
		retval = 0;
	}
#ifdef DEBUG
	else if (retval >= 0)
		DEBUGMSG(("3dfx: Successfully set MTRR, mtrr register: %d\n",
			 retval));
#endif

	return retval;
}

static int resetmtrr_3dfx(void)
{
	int i, ret, retval = 0;

	for (i = 0; i < numCards; i++) {
		if (cards[i].mtrr_buf >= 0) {
			ret = mtrr_del(cards[i].mtrr_buf, 0, 0);
			if (ret < 0)
				retval = ret;
			cards[i].mtrr_buf = -1;
			if (cards[i].mtrr_ctl >= 0) {
				ret = mtrr_del(cards[i].mtrr_ctl, 0, 0);
				if (ret < 0)
					retval = ret;
				cards[i].mtrr_ctl = -1;
			}
		}
	}

	return retval;
}

#endif /* CONFIG_MTRR */

static struct file_operations fops_3dfx = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	.owner =	THIS_MODULE,
#endif
#ifdef HAVE_UNLOCKED_IOCTL
	.unlocked_ioctl = ioctl_3dfx,
#else
	.ioctl =	ioctl_3dfx,
#endif
	.mmap =		mmap_3dfx,
	.open =		open_3dfx,
	.release =	release_3dfx,
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static int probe_3dfx(struct pci_dev *dev, const struct pci_device_id *id)
{
	pci_read_config_dword(dev, PCI_BASE_ADDRESS_0, &cards[numCards].addr0);
	pci_read_config_dword(dev, PCI_BASE_ADDRESS_1, &cards[numCards].addr1);
	pci_read_config_dword(dev, PCI_BASE_ADDRESS_2, &cards[numCards].addr2);
	cards[numCards].addr0 &= ~0xF;
	cards[numCards].addr1 &= ~0xF;
	cards[numCards].dev = dev;
	cards[numCards].vendor = dev->vendor;
	cards[numCards].type = dev->device;
	cards[numCards].curFile = 0;


	DEBUGMSG(("3dfx: board vendor %4.4x type %4.4x located at %xh/%xh\n",
		 dev->vendor, dev->device, cards[numCards].addr0,
		 cards[numCards].addr1));
	numCards++;

	return 0;
}

static void remove_3dfx(struct pci_dev *dev)
{

}

MODULE_DEVICE_TABLE(pci, id_table_3dfx);

static struct pci_driver driver_3dfx = {
	.name =		name_3dfx,
	.id_table =	id_table_3dfx,
	.probe =	probe_3dfx,
	.remove =	remove_3dfx,
};
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) */

#ifdef MODULE
int init_module(void)
{
	int ret;

	DEBUGMSG(("3dfx: Entering init_module()\n"));

	if ((ret = register_chrdev(MAJOR_3DFX, "3dfx", &fops_3dfx)) < 0) {
		printk("3dfx: Unable to register character device with major %d\n", MAJOR_3DFX);
		return ret;
	}

#if HAVE_DEVFS
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	devfs_mk_cdev(MKDEV(MAJOR_3DFX, DEVICE_VOODOO),
		      S_IFCHR | S_IROTH | S_IWOTH | S_IRGRP | S_IWGRP,
		      "3dfx");
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,46) || defined(DEVFS_SUPPORT)
	devfs_handle = devfs_register(NULL, "3dfx", DEVFS_FL_NONE,
		MAJOR_3DFX, DEVICE_VOODOO,
		S_IFCHR | S_IROTH | S_IWOTH | S_IRGRP | S_IWGRP,
		&fops_3dfx, NULL);
#endif
#endif

	DEBUGMSG(("3dfx: Successfully registered device 3dfx\n"));
	ret = findCards();
	if (ret < 0) {
		printk("3dfx: findCards() failed, returned %d\n", ret);
		return ret;
	}

#ifdef CONFIG_MTRR
	ret = setmtrr_3dfx();
	if (ret < 0) {
		DEBUGMSG(("setmtrr_3dfx() failed, returned %d\n", ret));
		/*
		unregister_chrdev(MAJOR_3DFX, "3dfx");
		return ret;
		*/
	}
#endif

	return 0;
}

void cleanup_module(void)
{
	DEBUGMSG(("3dfx: Entering cleanup_module\n"));

#ifdef CONFIG_MTRR
	resetmtrr_3dfx();
#endif
#if HAVE_DEVFS
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	devfs_remove("3dfx");
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,46) || defined(DEVFS_SUPPORT)
	devfs_unregister(devfs_handle);
#endif
#endif

	unregister_chrdev(MAJOR_3DFX, "3dfx");
	if (numCards)
	{
		release_region(cards[0].ioaddr, ASIC_IO_SIZE);
        release_mem_region(cards[0].addr0, cards[0].memsize);
		mca_mark_as_unused(cards[0].slot);
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	pci_unregister_driver(&driver_3dfx);
#endif
}
#else /* !MODULE */

long init_3dfx(long mem_start, long mem_end)
{
	int ret;

	if (register_chrdev(MAJOR_3DFX, "3dfx", &fops_3dfx)) {
		DEBUGMSG(("3dfx: Unable to register_chrdev with major %d\n", MAJOR_3DFX));
		return 0;
	}

	ret = findCards();
	if (ret < 0) {
		printk("3dfx: findCards() failed, returned %d\n", ret);
		return 0;
	}

	return mem_start;
}
#endif /* !MODULE */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,21)
MODULE_AUTHOR("Daryll Strauss et al.");
MODULE_DESCRIPTION("3dfx glide kernel device driver");
MODULE_LICENSE("GPL");
#ifdef MODULE_VERSION
MODULE_VERSION("2011.05.27");
#endif
#endif

#if defined(DEBUG)
/*
 * Kludge to get rid of:
 * ./3dfx.o: unresolved symbol inb
 * ./3dfx.o: unresolved symbol inl
 * ./3dfx.o: unresolved symbol inw
 * ./3dfx.o: unresolved symbol outb
 * ./3dfx.o: unresolved symbol outl
 * ./3dfx.o: unresolved symbol outw
 * when compiling this module with -g during debugging.
 */
#undef __KERNEL__
#define extern
#undef _ASM_IO_H
#include <asm/io.h>
#endif

