/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <elf.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 *
 * UNSW: If you use ASST3 config as required, then this file forms
 * part of the VM subsystem.
 *
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}
	/*
	 * Initialize as needed.
	 */
	as -> as_heapStart = 0;
	as -> as_heapEnd = 0;
	as -> as_stack = USERSTACK;
	as -> as_regions = NULL;

	as -> as_pte = (paddr_t **)alloc_kpages(1);
	if (as -> as_pte == NULL) {
		kfree(as);
		return NULL;
	}
	// bzero(as -> as_pte, PAGE_SIZE);
	/*
	 * 1024 descriptors in the 1st-level Page Table
	 */
	for (int i = 0; i < N_DESCRIPTORS; i++) as -> as_pte[i] = NULL;
	//panic("addrspace: as_create DONE\n");
	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	/*
	 * Initialize as needed.
	 */
	newas -> as_heapStart = old -> as_heapStart;
	newas -> as_heapEnd = old -> as_heapEnd;
	newas -> as_stack = old -> as_stack;
	newas -> as_regions = NULL;

	/*
	 * Copy regions
	 */
	region *oRegions; // Old regions
	region *nRegions = NULL; // New regions
	for (oRegions = old -> as_regions; oRegions != NULL; oRegions = oRegions -> next) {
		region *temp = kmalloc(sizeof(region));
		if (temp == NULL) {
			as_destroy(newas);
			return ENOMEM; 	// out of memory
		}

		/*
		 * Copy old region to temp
		 */
		temp -> as_vbase = oRegions -> as_vbase;
		temp -> size = oRegions -> size;
		temp -> flags = oRegions -> flags;
		temp -> oldFlags = oRegions -> oldFlags;
		temp -> next = NULL;

		/*
		 * Append to the end of the list
		 */
		if (nRegions != NULL) nRegions -> next = temp;
		else {
			newas -> as_regions = temp;
			nRegions = temp;
		}
	}
	/*
	 * copy the pagetable
	 * from old to new
	 */
	int result = vm_copyPTE(old -> as_pte, newas -> as_pte);
	if (result != 0) {
		as_destroy(newas);
		return result;
	}
	//panic("addrspace: as_copy DONE\n");
	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	if(as == NULL) return;

	/*
	 * Free all regions in as
	 */
	region *oRegions = as -> as_regions;
	region *temp;		// a temp pointer used to kfree current region
	while(oRegions != NULL) {
		temp = oRegions;
		oRegions = oRegions -> next;
		kfree(temp);
	}
	/* Free pages in PTE*/
	vm_freePTE(as -> as_pte);
	kfree(as);
	// panic("addrspace: as_destroy DONE\n");
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	/*
	 * Write this. Copied from dumbvm.c
	 */
	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
	as_activate();
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{
	/*
	 * Write this.
	 */
	if (as == NULL) return EFAULT; // Bad memory reference

	if (vaddr + memsize >= as -> as_stack) return ENOMEM; // Out of memory

	// copy page alignment from dumbvm.c
	/* Align the region. First, the base... */
	memsize += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;

	region *nRegions = kmalloc(sizeof(region));
	if (nRegions == NULL) return ENOMEM; // Out of memory

	nRegions -> as_vbase = vaddr;
	nRegions -> size = memsize;
	nRegions -> flags = 0;
	if (readable) nRegions -> flags |= PF_R;
	if (writeable) nRegions -> flags |= PF_W;
	if (executable) nRegions -> flags |= PF_X;
	nRegions -> oldFlags = nRegions -> flags;
	//KASSERT((nRegions -> flags & writeable) == writeable);
	// nRegions -> oldFlags = nRegions -> flags = writeable;
	// (void) readable;
	// (void) executable;
	// KASSERT(as -> as_regions == NULL);

	nRegions -> next = as -> as_regions;
	as -> as_regions = nRegions;
	
	as -> as_heapEnd = as -> as_heapStart = vaddr + memsize; // Heap after the region
	//panic("addrspace: as_define_region DONE\n");
	return 0;
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */
	if (as == NULL) return EFAULT; // Bad memory reference

	region *oRegions = as -> as_regions;
	while (oRegions != NULL) {
		// make READONLY regions to RW
		if ((oRegions -> flags & PF_W) != PF_W) {
			oRegions -> flags |= PF_W;
		}
		oRegions = oRegions -> next;
	}
	// panic("addrspace: as_prepare_load DONE\n");
	return 0;

}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */
	if (as == NULL) return EFAULT; // Bad memory reference

	region *oRegions = as -> as_regions;
	while (oRegions != NULL) {
		// Not modified
		if (oRegions -> flags == oRegions -> oldFlags) oRegions = oRegions -> next;
		else {
			// set flag back to old flag
			oRegions -> flags = oRegions -> oldFlags;
			//vm_resetPTE(as -> as_pte); // gets Fatal user mode trap 1 sig 11 if i reset PT
			oRegions = oRegions -> next;
		}
	}
	int spl = splhigh();
	for (int i = 0; i<NUM_TLB; i++) tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	splx(spl);
	// panic("addrspace: as_complete_load DONE\n");
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */
	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}