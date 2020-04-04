**Design and Implementation of a Virtual Memory Unit (MMU)**

Memory management unit for translating virtual addresses to physical addresses. Uses a TLB for caching. LRU is the policy for page replacement.

to run with no page replacement:
change define statement for PAGE_TABLE_SIZE to 256

to run with page replacement:
change define statement for PAGE_TABLE_SIZE to any value less than the PHYSICAL_MEMORY_SIZE
This will enable LRU page replacement

To Change the physical memory size:
change define statement for PHYSICAL_MEMORY_SIZE
