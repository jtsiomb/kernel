#include <stdio.h>
#include "mboot.h"
#include "vid.h"
#include "term.h"
#include "asmops.h"
#include "segm.h"
#include "intr.h"
#include "ata.h"
#include "rtc.h"
#include "timer.h"
#include "mem.h"
#include "vm.h"
#include "proc.h"


void kmain(struct mboot_info *mbinf)
{
	clear_scr();

	/* pointless verbal diarrhea */
	if(mbinf->flags & MB_LDRNAME) {
		printf("loaded by: %s\n", mbinf->boot_loader_name);
	}
	if(mbinf->flags & MB_CMDLINE) {
		printf("kernel command line: %s\n", mbinf->cmdline);
	}

	puts("kernel starting up");

	init_segm();
	init_intr();


	/* initialize the physical memory manager */
	init_mem(mbinf);
	/* initialize paging and the virtual memory manager */
	init_vm();

	/* initialize ATA disks */
	init_ata();

	/* initialize the timer and RTC */
	init_timer();
	init_rtc();

	/* create the first process and switch to it */
	/*init_proc();*/

	/* XXX unreachable */

	for(;;) {
		halt_cpu();
	}
}
