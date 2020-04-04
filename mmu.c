#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Constants
#define OUTPUT_FILENAME ("output.csv")
#define PAGE_MASK   (0b1111111100000000)
#define OFFSET_MASK (0b11111111)
#define PAGE_SHIFT (8)
#define INACTIVE_BIT (-1)
#define PAGE_TABLE_SIZE (128)
#define PAGE_SIZE (256)
#define PHYSICAL_MEMORY_SIZE (256)
#define TLB_ENTRIES (16)
#define FRAME_SIZE (256)
#define P_MEMORY (65536)
#define LINE_LENGTH (100)

// Structs
struct node{
    int pagenum;
    struct node *next;
};

// Global Variables
// pagetable[index][PAGE_SIZE] => Contains Page #
char pagetable[PAGE_TABLE_SIZE][PAGE_SIZE + 1];
char tlb[TLB_ENTRIES][PAGE_SIZE + 1];
int page_to_frame[PHYSICAL_MEMORY_SIZE];// [PAGE NUMBER, FRAME NUMBER]
int fault_count = 0;
struct node *pgt_head;

// Prototypes
void clear_pagetable(void);
void insert(struct node **head, int pn);
void delete(struct node **head, int pn);
int is_in_tlb(int pagenum);
char get_tlb_data(int pagenum, int offset);
int is_in_pgt(int pagenum); 
char get_pgt_data(int page_num, int offset);
void clear_tlb(void);
unsigned long get_tlb_pad(int page_num, int offset);
unsigned long get_pgt_pad(int page_num, int offset);
void clear_page_to_frame(void);

// Main Method
int main(int argc, char *argv[])
{

    if (argc < 3)
    {
        printf("Incorrect number of args\n");
        exit(1);
    }
    FILE *address_file;
    address_file = fopen(argv[2],"r");

    FILE *backing_store;
    backing_store = fopen(argv[1],"rb");

    char stored_byte;

    //output.csv
    FILE *out_file;
    out_file = fopen(OUTPUT_FILENAME, "w+");

    // Local Variables
    int current_frame = 0;
    int tlb_counter = 0;
    int pagetable_counter = 0;
    int tlb_hit = 0;
    int total_iterations = 0;
    char *temp;
    char line[LINE_LENGTH];

    // Clear and set up arrays with INVALID_BIT
    clear_pagetable();
    clear_tlb();
    clear_page_to_frame();

    // Iterate through the input address file
    while (fgets(line,LINE_LENGTH,address_file) != NULL)
    {   
        temp = strdup(line);
        int virtual = atoi(temp);

        int page_num = (virtual & PAGE_MASK) >> PAGE_SHIFT;

        // Convert the page number to a char for storage in the tlb and pagetable
        char page_num_char = page_num;
        int offset = virtual & OFFSET_MASK;

        if (is_in_tlb(page_num) == 1)
        {
            // TLB Hit
            tlb_hit++;
            stored_byte = get_tlb_data(page_num, offset);

            int frame = page_to_frame[page_num];
            int unsigned long physical_address = (frame << PAGE_SHIFT) | offset;

            delete(&pgt_head, page_num);
            insert(&pgt_head, page_num);

            // Print Values
            //unsigned long physical_address = get_tlb_pad(page_num, offset);
            fprintf(out_file, "%d,%lu,%d\n", virtual, physical_address, stored_byte);
            //printf("%d\n", stored_byte);
        } else 
        {
            // TLB Miss
            if (is_in_pgt(page_num) == 1)
            {
                // Hit in Pagetable
                // Right Column Value
                stored_byte = get_pgt_data(page_num, offset);

                // Print values
                //printf("%d\n", stored_byte);

                int frame = page_to_frame[page_num];
                int unsigned long physical_address = (frame << PAGE_SHIFT) | offset;

                //int physical_address = get_pgt_pad(page_num, offset);
                fprintf(out_file, "%d,%lu,%d\n", virtual, physical_address, stored_byte);

                // Update TLB
                char data_temp[PAGE_SIZE];

                // Read from backing store at address - offset value
                fseek(backing_store, virtual - offset, SEEK_SET);
                fread(data_temp, PAGE_SIZE, 1, backing_store);

                for (int j = 0; j < PAGE_SIZE; j++)
                {
                    tlb[tlb_counter][j] = data_temp[j];
                }

                // Set new rightmost column page number value
                tlb[tlb_counter][PAGE_SIZE] = page_num_char;
                tlb_counter = (tlb_counter + 1) % 16;
                // END UPDATE TLB

                // Re-insert node in LRU cache
                delete(&pgt_head, page_num);
                insert(&pgt_head, page_num);
            } else 
            {
                // PageTable Miss
                char data_temp[PAGE_SIZE];           
                int pagetable_index;

                // Read from backing store at address - offset value
                fseek(backing_store, virtual - offset, SEEK_SET);
                fread(data_temp, PAGE_SIZE, 1, backing_store);

                for (int j = 0; j < PAGE_SIZE; j++)
                {
                    tlb[tlb_counter][j] = data_temp[j];
                }

                // Set new rightmost column page number value
                tlb[tlb_counter][PAGE_SIZE] = page_num_char;

                // If going through the first time, no need for LRU
                if (pagetable_counter <= PAGE_TABLE_SIZE - 1)
                {
                    // Add page number to LRU cache linked list
                    insert(&pgt_head, page_num);
                    pagetable_index = pagetable_counter;
                } else 
                {
                    // Need to update the LRU and get new index
                    
                    // Page number to be removed: pgt_head->pagenum. Get Char conversion
                    char pgt_head_char = pgt_head->pagenum;
                    
                    for (int i = 0; i < PAGE_TABLE_SIZE; i++)
                    {
                        if (pagetable[i][PAGE_SIZE] == pgt_head_char)
                        {
                            pagetable_index = i;
                        }
                    }

                    //Delete head of LRU linkedlist
                    delete(&pgt_head, pgt_head->pagenum); 
                    insert(&pgt_head, page_num);
                }
                // Translate everything in data_temp to appropriate pagetable
                for (int i = 0; i < PAGE_SIZE; i++)
                {
                    pagetable[pagetable_index][i] = data_temp[i];
                }
                
                // Set new rightmost column page number value
                pagetable[pagetable_index][PAGE_SIZE] = page_num_char;
                
                // Right col value
                stored_byte = get_pgt_data(page_num, offset);
                
                // Physical Address
                int frame;
                if(page_to_frame[page_num] != -1)
                {
                    // Found in page-to-frame array
                    frame = page_to_frame[page_num];
                } else 
                {
                    // Not found in page-to-frame array
                    page_to_frame[page_num] = current_frame;
                    frame = current_frame;
                    current_frame++;
                }
                
                // Calculate physical address
                int unsigned long physical_address = (frame << PAGE_SHIFT) | offset;

                //Print values
                //int physical_address = get_pgt_pad(page_num, offset);
                fprintf(out_file, "%d,%lu,%d\n", virtual, physical_address, stored_byte);
                //printf("%d\n", stored_byte);

                // Circular buffer iteration for FIFO order
                tlb_counter = (tlb_counter + 1) % 16;

                // add to faults count
                fault_count++;
                pagetable_counter++;
            }
        }        
        total_iterations++;
    }

    printf("Page Faults = %d\n", fault_count);
    printf("TLB Hits = %d\n", tlb_hit); 

    printf("Page Fault Rate = %0.2f%%\n", (double) fault_count / total_iterations * 100);
    printf("TLB Hit Rate = %0.2f%%\n", (double) tlb_hit / total_iterations * 100); 
    fclose(address_file);
    fclose(backing_store);
    fclose(out_file);
    return 0;
}

// Clear the page to frame array
void clear_page_to_frame(void)
{
    for (int i = 0; i < PHYSICAL_MEMORY_SIZE; i++)
    {
        page_to_frame[i] = -1;
    }
}

// Get physical address from tlb
unsigned long get_tlb_pad(int pagenum, int offset)
{
    int pad = 0;
    char data;
    char pn_char = pagenum;

    for (int i = 0; i < PAGE_TABLE_SIZE; i++)
    {
        if (pagetable[i][PAGE_SIZE] == pn_char)
        {
            int frame = i;
            pad = (frame << PAGE_SHIFT) | offset;
            
        }
    }
    return pad;
}

// Get physical address from pagetable
unsigned long get_pgt_pad(int pagenum, int offset)
{
    int pad = 0;
    char data;
    char pn_char = pagenum;
    for (int i = 0; i < PAGE_TABLE_SIZE; i++)
    {
        if (pagetable[i][PAGE_SIZE] == pn_char)
        {
            int frame = i;
            pad = (frame << PAGE_SHIFT) | offset;
        }
    }
    return pad;
}

char get_tlb_data(int pagenum, int offset)
{
    char data;
    char pn_char = pagenum;
    for (int i = 0; i < TLB_ENTRIES; i++)
    {
        if (tlb[i][PAGE_SIZE] == pn_char)
        {
            data = tlb[i][offset];
        }
    }
    return data;
}

// 0 => not in tlb, 1 => in tlb
int is_in_tlb(int pagenum)
{
    int result = 0;
    char pn_char = pagenum;
    for (int i = 0; i < TLB_ENTRIES; i++)
    {
        if (tlb[i][PAGE_SIZE] == pn_char && pagetable[i][0] != INACTIVE_BIT)
        {
            result = 1;
        }
    }
    return result;
}

// 0 => not in pgt, 1 => in pgt
int is_in_pgt(int pagenum)
{
    int result = 0;
    char pn_char = pagenum;

    for (int i = 0; i < PAGE_TABLE_SIZE; i++)
    {
        if (pagetable[i][PAGE_SIZE] == pn_char && pagetable[i][0] != INACTIVE_BIT)
        {
            result = 1;
        }
    }
    return result;
}

char get_pgt_data(int pagenum, int offset)
{
    char data;
    char pn_char = pagenum;
    for (int i = 0; i < PAGE_TABLE_SIZE; i++)
    {
        if (pagetable[i][PAGE_SIZE] == pn_char)
        {
            data = pagetable[i][offset];
        }
    }
    return data;
}

void clear_pagetable(void)
{
    for (int i = 0; i < PAGE_TABLE_SIZE; i++)
    {
        pagetable[i][0] = INACTIVE_BIT;
        pagetable[i][PAGE_SIZE] = INACTIVE_BIT;
    }
}

void clear_tlb(void)
{
    for (int i = 0; i < TLB_ENTRIES; i++)
    {
        tlb[i][0] = INACTIVE_BIT;
        tlb[i][PAGE_SIZE] = INACTIVE_BIT;
    }
}

// Insert Page number into list
void insert(struct node **head, int pn) {
    if (*head == NULL)
	{
		*head = malloc(sizeof(struct node));
		(*head)->pagenum = pn;
		(*head)->next = NULL;
	} else 
	{
        struct node *newNode = malloc(sizeof(struct node));
        struct node *temp = malloc(sizeof(struct node));
        newNode->pagenum = pn;
        newNode->next = NULL;

        temp = *head;
        while (temp->next != NULL)
        {
            temp = temp->next;
        }
        temp->next = newNode;
    }
}

// delete the selected task from the list
void delete(struct node **head, int pn) {
    struct node *temp;
    struct node *prev;

    temp = *head;
    // special case - beginning of list
    if (pn == temp->pagenum) {
        *head = (*head)->next;
    }
    else {
        // interior or last element in the list
        prev = *head;
        temp = temp->next;
        while (pn != temp->pagenum) {
            prev = temp;
            temp = temp->next;
        }
        prev->next = temp->next;
    }
}