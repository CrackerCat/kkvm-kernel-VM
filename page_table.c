#include <err.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "debug.h"
#include "page_table.h"

__always_inline void set_control_registers(struct kvm_sregs *p_sregs, pgd_t pgd_base){
    /*  设置一系列控制寄存器   */

    p_sregs->cr4 = CR4_PAE_BIT;  /*  设置CR4的第五位，打开PAE   */
    p_sregs->cr4 |= 0x600; // CR4_OSFXSR | CR4_OSXMMEXCPT; /* enable SSE instructions */
    p_sregs->cr3 = pgd_base;                    /*  设置cr3寄存器指向 PGD 的基地址  */
    p_sregs->cr0 = CR0_PE|CR0_MP|CR0_ET|CR0_NE|CR0_WP|CR0_AM|CR0_PG;    /*  设置cr0     */
    p_sregs->efer = 0x500;                    
    #ifdef DEBUG_SREG_SET
    printf("sregs->cr4:0x%llx\n",p_sregs->cr4);   
    printf("sregs->cr3:0x%llx\n",p_sregs->cr3);
    printf("sregs->cr0:0x%llx\n",p_sregs->cr0);
    #endif
}
__always_inline void set_segment_registers(struct kvm_sregs *p_sregs){
    /* cs包含可见的segment selector和不可见的Segment descriptors  */
    /* 前者一般指向GDT中的Segment descriptors  */
    /* 之后通过base + offset来合成地址  */
    struct kvm_segment seg = {
    .base = 0,
    .limit = 0xffffffff,
    .selector = 1 << 3,
    .present = 1,   /*  在内存中    */
    .type = 11, /* execute, read, accessed */
    .dpl = 0, /* privilege level 0 */
    .db = 0,
    .s = 1, /*   S位0说明是系统段，为1是数据or代码段    */
    .l = 1, /*  运行在64位模式  */
    .g = 1,
  };
  p_sregs->cs = seg;
  seg.type = 3;
  seg.selector = 2<<3;
  p_sregs->ds = p_sregs->es = p_sregs->fs = p_sregs->gs = p_sregs->ss = seg;

}
int initialize_page_table(void *mem,struct kvm_sregs *p_sregs){

    /*  四级分页   */
    pgd_t pgd_addr = 0x1000;        /*  GPA  */
    pgd_t *pgd = (void *)(pgd_addr+mem);      /*  HVA  */
 
    pud_t pud_addr = 0x2000;
    pud_t *pud = (void *)(mem+pud_addr);

    pmd_t pmd_addr = 0x3000;
    pmd_t *pmd = (void *)(pmd_addr+mem);

    /*  pte中的指向物理地址  */
    pte_t pte_addr = 0x4000;
    pte_t *pte = (void *)(pte_addr+mem);

    pgd[0] = (pmd_addr | _PAGE_PRESENT | _PAGE_RW) ;       /*  已经调入,设置访问位，可mmaped、可写   */     
    //pud[0] = (pmd_addr | _PAGE_PRESENT | _PAGE_RW) ;
    pmd[0] = (pte_addr | _PAGE_PRESENT | _PAGE_RW) ;
    pte[0] =  _PAGE_PSE|_PAGE_PRESENT | _PAGE_RW ;      /*  启用 2M 分页  */
    
    #ifdef DEBUG_PAGE_SET
    //printf("host mem(GPA base) and code:%p\npgd:%p\npud:%p\npmd:%p\npte:%p\n",mem,pgd,pud,pmd,pte);
    printf("pgd[0]:0x%llx\npud[0]:0x%llx\npmd[0]:0x%llx\npte[0]:0x%llx\n",pgd[0],pud[0],pmd[0],pte[0]);
    #endif
    set_control_registers(p_sregs,pgd_addr);
    set_segment_registers(p_sregs);
}

void setup_page_tables(void *mem, struct kvm_sregs *sregs){
  uint64_t pml4_addr = 0x1000;
  uint64_t *pml4 = (void *)(mem + pml4_addr);

  uint64_t pdpt_addr = 0x2000;
  uint64_t *pdpt = (void *)(mem + pdpt_addr);

  uint64_t pd_addr = 0x3000;
  uint64_t *pd = (void *)(mem + pd_addr);

  pml4[0] = 3 | pdpt_addr; // PDE64_PRESENT | PDE64_RW | pdpt_addr
  pdpt[0] = 3 | pd_addr; // PDE64_PRESENT | PDE64_RW | pd_addr
  pd[0] = 3 | 0x80; // PDE64_PRESENT | PDE64_RW | PDE64_PS

  sregs->cr3 = pml4_addr;
  sregs->cr4 = 1 << 5; // CR4_PAE;
  sregs->cr4 |= 0x600; // CR4_OSFXSR | CR4_OSXMMEXCPT; /* enable SSE instructions */
  sregs->cr0 = 0x80050033; // CR0_PE | CR0_MP | CR0_ET | CR0_NE | CR0_WP | CR0_AM | CR0_PG
  sregs->efer = 0x500; // EFER_LME | EFER_LMA
  printf("pml4[0]:0x%llx\npdpt[0]:0x%llx\npd[0]:0x%llx\n",pml4[0],pdpt[0],pd[0]);
}
