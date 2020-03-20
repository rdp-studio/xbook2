#include "page.h"
#include "pmem.h"
#include "instruction.h"
#include <xbook/debug.h>
#include <xbook/math.h>
#include <xbook/memops.h>
#include <xbook/assert.h>

extern mem_node_t *mem_node_table;
extern unsigned int mem_node_count;
extern unsigned int mem_node_base;

unsigned long __alloc_pages(unsigned long count)
{
    mem_node_t *node = get_free_mem_node();
    if (node == NULL)
        return 0;
    
    int index = node - mem_node_table;

    /* 如果分配的页数超过范围 */
    if (index + count > mem_node_count)
        return 0; 

    /* 第一次分配的时候设置引用为1 */
    node->reference = 1;
    node->count = (unsigned int)count;
    node->flags = 0;
    node->cache = NULL;
    node->group = NULL;

    return (long)__mem_node2page(node);
}

int __free_pages(unsigned long page)
{
    mem_node_t *node = __page2mem_node((unsigned long)page);

    if (node == NULL)
        return -1;
    
    __atomic_dec(&node->reference);

	if (node->reference == 0) {
        node->count = 0;
		node->flags = 0;
        return 0;
	}
    return -1;
}
/*
 * __page_link - 物理地址和虚拟地址链接起来
 * @va: 虚拟地址
 * @pa: 物理地址
 * @prot: 页的保护
 * 
 * 把虚拟地址和物理地址连接起来，这样就能访问物理地址了
 */
void __page_link(void *va, void *pa, unsigned long prot)
{
    unsigned long vaddr = (unsigned long )va;
    unsigned long paddr = (unsigned long )pa;
    /* get page dir and page table */
	pde_t *pde = get_pde_ptr(vaddr);
	pte_t *pte = get_pte_ptr(vaddr);

    /* if page table exist. */
	if (*pde & PG_P_1) {
        /* phy page must not exist! */
        ASSERT(!(*pte & PG_P_1)); 
        
        /* make sure phy page not exist! */
        if (!(*pte & PG_P_1)) {
            *pte = (paddr | prot | PG_P_1);
        } else {
            /* phy page exist! */
            panic("pte %x has exist!\n", *pte);
            *pte = (paddr | prot | PG_P_1);
        }
	} else { /* no page table, need create a new one. */
        unsigned long page_table = __alloc_pages(1);
        if (!page_table) {
            /* no page left */
            panic("kernel no page left!\n");
            /* we can goto free some pages and try it again,
             but now we just stop! */
        }
        printk("info: page_link -> new page table %x\n", page_table);

        /* add page table to page dir */
        *pde = (page_table | prot | PG_P_1);

        /* clear page to avoid dirty data */
        memset((void *)((unsigned long)pte & PAGE_ADDR_MASK), 0, PAGE_SIZE);

        /* make sure phy page not exist! */
        ASSERT(!(*pte & PG_P_1));

        *pte = (paddr | prot | PG_P_1);
    }
}

/*
 * __page_unlink - 取消虚拟地址对应的物理链接
 * @vaddr: 虚拟地址
 * 
 * 取消虚拟地址和物理地址链接。
 * 在这里我们不删除页表项映射，只删除物理页，这样在以后映射这个地址的时候，
 * 可以加速运行。但是弊端在于，内存可能会牺牲一些。
 * 也消耗不了多少，4G内存才4MB。
 */
void __page_unlink(unsigned long vaddr)
{
	pte_t *pte = get_pte_ptr(vaddr);

	// 如果页表项存在物理页
	if (*pte & PG_P_1) {
		printk("unlink vaddr:%x pte:%x\n", vaddr, *pte);
		
        // 清除页表项的存在位，相当于删除物理页
		*pte &= ~PG_P_1;

		/* flush vaddr tbl cache */
        flush_tbl(vaddr);        
    }
}

/**
 * __map_pages - 映射页面
 * 
 * 非连续内存使用，可以提高分配效率
 */
int __map_pages(unsigned long start, unsigned long len, unsigned long prot)
{
    unsigned long first = start;
    // 长度和页对齐
    len = PAGE_ALIGN(len);

	/* 判断长度是否超过剩余内存大小 */
    //printk("len %d pages %d\n", len, len / PAGE_SIZE);

	/* 分配物理页 */
	unsigned long pages = __alloc_pages(len / PAGE_SIZE);

	if (!pages) {
        printk("waring: map_pages -> map without free pages!\n");
        return -1;
    }
	unsigned long end = first + len;
	printk("info: map_pages -> start%x->%x len %x\n", first, pages, len);
    while (first < end)
	{
        // 对单个页进行链接
		__page_link((void *)first, (void *)pages, prot);
		first += PAGE_SIZE;
        pages += PAGE_SIZE;
	}
	return 0;
}

/**
 * __addr_v2p - 虚拟地址转换为物理地址
 * 
 * 用于非连续地址转换。 
 */
unsigned long __addr_v2p(unsigned long vaddr)
{
	pte_t* pte = get_pte_ptr(vaddr);
	/* 
	(*pte)的值是页表所在的物理页框地址,
	去掉其低12位的页表项属性+虚拟地址vaddr的低12位
	*/
	return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

/**
 * __unmap_pages - 取消页映射
 * @vaddr: 虚拟地址
 * @len: 内存长度
 * 
 * 和map_pages配合使用，同使用于非连续内存
 */
int __unmap_pages(void *vaddr, unsigned long len)
{
	if (!len)
		return -1;
	
	len = PAGE_ALIGN(len);
	
	/* 判断长度是否超过剩余内存大小 */
	unsigned long paddr, start = (unsigned long)vaddr;

    unsigned long end = start + len;
	
    /* 不能通过__v2p来获取，需要从页表映射中解析获取 */
	paddr = __addr_v2p(start);
    
	//printk("unmap pages:%x->%x len %x\n", vaddr, paddr, len);
	while (start < end)
	{
		__page_unlink(start);
		start += PAGE_SIZE;
	}
	
	// 释放物理页
	__free_pages(paddr);
    
	return 0;
}

/**
 * __map_pages_safe - 安全地映射页
 * 
 * 如果页已经映射，就不覆盖，直接跳过
 * 每次映射单个页，效率较低，不过比较安全
 * 用于进程地址空间
 */
int __map_pages_safe(void *start, unsigned long len, unsigned long prot)
{
    unsigned long vaddr = (unsigned long )start;
    len = PAGE_ALIGN(len);
    unsigned long pages = DIV_ROUND_UP(len, PAGE_SIZE);
    unsigned long page_idx = 0;
    unsigned long page_addr;
    while (page_idx < pages) {
        /* get page dir and page table */
        pde_t *pde = get_pde_ptr(vaddr);
        pte_t *pte = get_pte_ptr(vaddr);

        if (!(*pde & PG_P_1) || !(*pte & PG_P_1)) {
            page_addr = __alloc_pages(1);
            if (!page_addr) {
                printk("error: user_map_vaddr -> map pages failed!\n");
                return -1;
            }
            __page_link((void *)vaddr, (void *)page_addr, prot);
            printk("info: map_pages_safe -> start%x->%x\n", vaddr, page_addr);
        }
        vaddr += PAGE_SIZE;
        page_idx++;
    }
    return 0;
}

static int is_page_table_empty(pte_t *page_table)
{
    int i;
    for (i = 0; i < 0; i++) {
        /* if some one exist, not empty */
        if (page_table[i] & PG_P_1)
            return 0;
    }
    return 1;   /* empty */
}

/**
 * __unmap_pages_safe - 安全地取消映射页
 * 
 * 如果页已经映射，就不覆盖，直接跳过
 * 每次释放单个页，可以释放所有资源。
 * 用于进程地址释放
 */
int __unmap_pages_safe(void *start, unsigned long len)
{
    unsigned long vaddr = (unsigned long )start;
    len = PAGE_ALIGN(len);
    unsigned long pages = DIV_ROUND_UP(len, PAGE_SIZE);

    unsigned long pte_idx;       /* pte -> physic page */
    
    pde_t *pde;
    pte_t *pte;
    
    while (pages > 0) {
        pde = get_pde_ptr(vaddr); /* get pde from vaddr  */
        if ((*pde & PG_P_1)) {  /* page table exist */
            /* when page tabel entry nr < 1024, continue free */
            while ((pte_idx = PTE_IDX(vaddr)) < 1024) {
                printk("info: unmap_pages_safe -> pte idx %d\n", pte_idx);
                pte = get_pte_ptr(vaddr); /* get pte from vaddr  */
                if (*pte & PG_P_1) {
                    printk("info: unmap_pages_safe -> start%x->%x\n", vaddr, *pte & PAGE_ADDR_MASK);
                    /* free physic page */
                    __free_pages(*pte & PAGE_ADDR_MASK);

                    /* del page entry */
                    *pte &= ~PG_P_1;
                }
                vaddr += PAGE_SIZE;
                pages--;
                if (!pages) { /* no page left  */
                    /* check page table */
                    if (is_page_table_empty((pte_t *)((unsigned long)pte & PAGE_ADDR_MASK))) {
                        printk("info: unmap_pages_safe -> del page table %x\n", *pde & PAGE_ADDR_MASK);

                        /* free page table */
                        __free_pages(*pde & PAGE_ADDR_MASK);            
                    }

                    goto end;
                }
                /* if at last one, break out */
                if (pte_idx == 1023)
                    break;
            }

            printk("info: unmap_pages_safe -> del page table %x\n", *pde & PAGE_ADDR_MASK);

            /* free page table */
            __free_pages(*pde & PAGE_ADDR_MASK);

            /* del page entry */
            *pde &= ~PG_P_1;
        } else {
            vaddr += PAGE_SIZE;
            pages--;
            if (!pages) { /* no page left  */
                goto end;
            }
        }
    }
end:
    return 0;
}






/*
 * mem_self_mapping - 内存自映射
 * @start: 开始物理地址
 * @end: 结束物理地址
 * 
 * 把物理和内核虚拟地址进行映射
 */
int mem_self_mapping(unsigned int start, unsigned int end)
{
	/* ----映射静态内存---- */
	//把页目录表设置
	unsigned int *pdt = (unsigned int *)PAGE_DIR_VIR_ADDR;

	// 求出目录项数量
	unsigned int pde_nr = (end - start) / (1024 * PAGE_SIZE);
    
	// 先获取的页数
	unsigned int pte_nr = (end-start)/PAGE_SIZE;

	// 获取页数中余下的数量，就是页表项剩余数
	pte_nr = pte_nr % 1024;
	
	//跳过页表区域中的第一个页表
	unsigned int *pte_addr = (unsigned int *) (PAGE_TABLE_PHY_ADDR + 
			PAGE_SIZE * PAGE_TABLE_PHY_NR);

	int i, j;
	// 根据页目录项来填写
	for (i = 0; i < pde_nr; i++) {
		//填写页目录表项
		//把页表地址和属性放进去
		pdt[512 + PAGE_TABLE_PHY_NR + i] = (unsigned int)pte_addr | PG_P_1 | PG_RW_W | PG_US_S;
		
		for (j = 0; j < PAGE_ENTRY_NR; j++) {
			//填写页页表项

			//把物理地址和属性放进去
			pte_addr[j] = start | PG_P_1 | PG_RW_W | PG_US_S;
			//指向下一个物理页
			start += PAGE_SIZE;
		}
		//指向下一个页表
		pte_addr += PAGE_SIZE;
	}
	// 根据剩余的页表项填写

	//有剩余的我们才填写
	if (pte_nr > 0) {
		// i 和 pte_addr 的值在上面的循环中已经设置
		pdt[512 + PAGE_TABLE_PHY_NR + i] = (unsigned int)pte_addr | PG_P_1 | PG_RW_W | PG_US_S;
		
		// 填写剩余的页表项数量
		for (j = 0; j < pte_nr; j++) {
			//填写页页表项
			//把物理地址和属性放进去
			pte_addr[j] = start | PG_P_1 | PG_RW_W | PG_US_S;
			
			//指向下一个物理页
			start += PAGE_SIZE;
		}
	}

	/* 在开启分页模式之后，我们的内核就运行在高端内存，
	那么，现在我们不能通过低端内存访问内核，所以，我们在loader
	中初始化的0~8MB低端内存的映射要取消掉才行。
	我们把用户内存空间的页目录项都清空 */ 
	
	for (i = 0; i < 512; i++) {
		pdt[i] = 0;
	}

	/* ----映射完毕---- */
	return 0;
}
