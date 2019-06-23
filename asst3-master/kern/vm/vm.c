#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>
#include <proc.h>
#include <current.h>
#include <elf.h>
#include <spl.h>

/*
 * REFERENCE USED FOR 2 LEVEL PAGE TABLE
 * - https://www.clear.rice.edu/comp425/slides/L31.pdf
 */

/* Place your page table functions here */

/* PT init */
int vm_initPT(paddr_t **oldPTE, uint32_t msb)
{
    oldPTE[msb] = kmalloc(sizeof(paddr_t) * N_DESCRIPTORS);
    
    if (oldPTE[msb] == NULL) return ENOMEM;

    for (int i = 0; i < N_DESCRIPTORS; i ++) oldPTE[msb][i] = 0; // Zero filled
    // panic("vm: vm_initPT DONE\n");
    return 0;
}

int vm_addPTE(paddr_t **oldPTE, uint32_t msb, uint32_t lsb, uint32_t dirty)
{
    vaddr_t vbase = alloc_kpages(1);
    if (vbase == 0) return ENOMEM;
    bzero((void *)vbase, PAGE_SIZE);
    paddr_t pbase = KVADDR_TO_PADDR(vbase);

    oldPTE[msb][lsb] = (pbase & PAGE_FRAME) | dirty | TLBLO_VALID;
    // panic("vm: vm_addPTE DONE\n");
    return 0;
}

int vm_copyPTE(paddr_t **oldPTE, paddr_t **newPTE)
{
    uint32_t dirty = 0;

    for (int i = 0; i < N_DESCRIPTORS; i++) {
        if (oldPTE[i] == NULL) continue;

        newPTE[i] = kmalloc(sizeof(paddr_t) * N_DESCRIPTORS);

        for (int j = 0; j < N_DESCRIPTORS; j++) {
            if (oldPTE[i][j] == 0) newPTE[i][j] = 0;
            else {
                vaddr_t newframe = alloc_kpages(1);
                if (newframe == 0) return ENOMEM; // Out of memory
                bzero((void *)newframe,PAGE_SIZE);
                // copy bytes
                if (memmove((void *)newframe, (const void *)PADDR_TO_KVADDR(oldPTE[i][j] & PAGE_FRAME)
                    , PAGE_SIZE) == NULL) { // fail memmove()
                    vm_freePTE(newPTE);
                    return ENOMEM; // Out of memory
                }
                dirty = oldPTE[i][j] & TLBLO_DIRTY;
                newPTE[i][j] = (KVADDR_TO_PADDR(newframe) & PAGE_FRAME) | dirty | TLBLO_VALID;;
            }
        }
    }
    // panic("vm: vm_copyPTE DONE\n");
    return 0;
}

void vm_freePTE(paddr_t **oldPTE)
{
    for (int i = 0; i < N_DESCRIPTORS; i ++) {
        if (oldPTE[i] == NULL) continue;

        for (int j = 0; j < N_DESCRIPTORS; j ++) {
            if (oldPTE[i][j] != 0) free_kpages(PADDR_TO_KVADDR(oldPTE[i][j] & PAGE_FRAME));
        }
        kfree(oldPTE[i]);
    }
    kfree(oldPTE); // Free page table entry
    // panic("vm: vm_freePTE DONE\n");
}

/* Initialization function */
void vm_bootstrap(void)
{
    /* Initialise VM sub-system.  You probably want to initialise your 
       frame table here as well.
    */
    // FT *ft;
    // uint32_t pages = ram_getsize() / PAGE_SIZE;
    // paddr_t pbase = (uint32_t)ram_getsize() - (uint32_t)(N_FRAMES * sizeof(FT));
    // ft = (FT *)PADDR_TO_KVADDR(pbase);

    // FT fte;
    // for (uint32_t i = 0; i < N_FRAMES; i ++) {
        //panic("vm: vm_bootstrap: fte\n");
        // fte.status = FREE_FRAME;
        // memmove(&ft[i], &fte, sizeof(FT));
        //panic("vm: vm_bootstrap: fte DONE\n");
    // }
    /* Find out what memory it has available to manage 
     * right shift 12 to get the phy addr
     */
    // paddr_t firstfree = ram_getfirstfree() >> 12;
    //pbase >>= 12; 

    // for (uint32_t i = 0; i < firstfree + 1; i ++) ft[i].status = USED_FRAME;

    //for (uint32_t i = N_FRAMES - 1; i >= pbase; i --) ft[i].status = USED_FRAME;
    //panic("vm: vm_bootstrap DONE\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
    if (curproc == NULL) return EFAULT; // Bad memory reference
    
    if (faultaddress == 0) return EFAULT; // Bad memory reference

    faultaddress &= PAGE_FRAME;

    int flag = 0; // solve double free
    /*
     * faultype [LECTURE SLIDE ASST3 intro page 22]
     * ROUGH STRUCTURE
     */

    /* if VM_FAULT_READONLY, return EFAULT */
    switch(faulttype) {
        case VM_FAULT_READ:
        case VM_FAULT_WRITE:
            break;
        case VM_FAULT_READONLY:
            return EFAULT;  // Bad memory reference
        default: 
            return EINVAL;  // invalid arg
    }

    struct addrspace *as = proc_getas();
    if (as == NULL) return EFAULT; // no address space

    if (as -> as_regions == NULL) return EFAULT; // no regions

    if (as -> as_pte == NULL) return EFAULT; // no PTE


    // find region where faultaddress locates
    region *curr = as->as_regions;
    while (curr != NULL) {
        if (faultaddress >= curr->as_vbase) {
            if (faultaddress - curr->as_vbase < curr->size) {
                break;
            }
        }
        curr = curr->next;
    }

    if(curr == NULL){
        int stack_size = 16 * PAGE_SIZE;
        // if faultaddress locates in the end of heap and end of stack
        if (faultaddress < as->as_stack && faultaddress > (as->as_stack - stack_size)) {
            // dirty 
        }else {
            return EFAULT;
        }
    }

    paddr_t pbase = KVADDR_TO_PADDR(faultaddress);

    /*
     * 10 MSBs (bits 22..31) of the virtual address (PTN) 
     * are used to index 
     */
    uint32_t msb = pbase >> 22;
    /*
     * 10 LSBs used to index chosen PT
     */
    uint32_t lsb = pbase << 10 >> 22;

    int result;

    uint32_t dirty = 0;

    if (as -> as_pte[msb] == NULL) {
        result = vm_initPT(as -> as_pte, msb);
        if (result) return result;
        flag = 1;
    }

    if (as -> as_pte[msb][lsb] == 0) {

        region *oRegions = as -> as_regions;
        while (oRegions != NULL) {
            if (faultaddress >= (oRegions -> as_vbase + oRegions -> size * PAGE_SIZE) 
                && faultaddress < oRegions -> as_vbase) continue;
            // READONLY
            if ((oRegions -> flags & PF_W) != PF_W) dirty = 0;
            else dirty = TLBLO_DIRTY;
            break;

            oRegions = oRegions -> next;
        }
        if (oRegions == NULL) {
            // if (as -> as_pte[msb] != NULL) kfree(as -> as_pte[msb]);
            if (flag) kfree(as -> as_pte[msb]);
            return EFAULT; // Bad memory reference
        }

        result = vm_addPTE(as -> as_pte, msb, lsb, dirty);
        if (result) {
            // if (as -> as_pte[msb] != NULL) kfree(as -> as_pte[msb]);
            if (flag) kfree(as -> as_pte[msb]);
            return result;
        }
    }

    /* Entry high is the virtual address */
    uint32_t entryhi = faultaddress & TLBHI_VPAGE;
    /* Entry low is physical frame, dirty bit, and valid bit. */
    uint32_t entrylo = as -> as_pte[msb][lsb];
    /* Disable interrupts on this CPU while frobbing the TLB. */
    int spl = splhigh();
    /* Randomly add pagetable entry to the TLB. */
    tlb_random(entryhi, entrylo);
    splx(spl);
    // panic("vm: vm_fault DONE\n");
    return 0;
}

/*
 *
 * SMP-specific functions.  Unused in our configuration.
 */

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
    (void)ts;
    panic("vm tried to do tlb shootdown?!\n");
}
