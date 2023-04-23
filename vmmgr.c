#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define PAGE_SIZE 256
#define PAGE_TABLE_SIZE 256
#define FRAME_SIZE 256
#define NUM_FRAMES 256
#define MEMORY_SIZE (NUM_FRAMES * FRAME_SIZE)
#define TLB_SIZE 16
#define TLB_MISS -1

int page_table[PAGE_TABLE_SIZE];
int page_table_last_access[PAGE_TABLE_SIZE];
int tlb[TLB_SIZE][2];
int tlb_last_access[TLB_SIZE];
int free_frame_list[NUM_FRAMES];
int page_faults = 0;
int tlb_hits = 0;
int num_free_frames = NUM_FRAMES;

char physical_memory[MEMORY_SIZE];
FILE *backing_store;
FILE *addresses;

int extract_page_number(int address) {
    return (address >> 8) & 0xFF;
}

int extract_offset(int address) {
    return address & 0xFF;
}

void handle_page_fault(int page_number) {
    // Seek to the correct page in the backing store
    if (fseek(backing_store, page_number * PAGE_SIZE, SEEK_SET) != 0) {
        fprintf(stderr, "Error seeking to page %d in backing store.\n", page_number);
        exit(EXIT_FAILURE);
    }

    // Read the page from the backing store into physical memory
    if (fread(physical_memory + num_free_frames * FRAME_SIZE, sizeof(char), PAGE_SIZE, backing_store) != PAGE_SIZE) {
        fprintf(stderr, "Error reading page %d from backing store.\n", page_number);
        exit(EXIT_FAILURE);
    }

    // Update the page table and TLB
    page_table[page_number] = num_free_frames;
    page_table_last_access[page_number] = ++tlb_hits;
    int oldest_index = 0;
    int oldest_access = tlb_last_access[0];
    for (int i = 1; i < TLB_SIZE; i++) {
        if (tlb_last_access[i] < oldest_access) {
            oldest_index = i;
            oldest_access = tlb_last_access[i];
        }
    }
    tlb[oldest_index][0] = page_number;
    tlb[oldest_index][1] = num_free_frames;
    tlb_last_access[oldest_index] = tlb_hits;

    // Update bookkeeping variables
    num_free_frames--;
    page_faults++;
}

int translate_address(int logical_address) {
    int page_number = extract_page_number(logical_address);
    int offset = extract_offset(logical_address);

    // Check the TLB first
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i][0] == page_number) {
            tlb_hits++;
            tlb_last_access[i] = tlb_hits;
            int frame_number = tlb[i][1];
            return (frame_number * PAGE_SIZE) + offset;
        }
    }

    // TLB miss - consult the page table
    if (page_table[page_number] == TLB_MISS) {
        // Page fault - read page from backing store
        handle_page_fault(page_number);
    }

    // Update TLB
    int oldest_index = 0;
    int oldest_access = tlb_last_access[0];
    for (int i = 1; i < TLB_SIZE; i++) {
        if (tlb_last_access[i] < oldest_access) {
            oldest_index = i;
            oldest_access = tlb_last_access[i];
        }
    }
    tlb[oldest_index][0] = page_number;
    tlb[oldest_index][1] = page_table[page_number];
    tlb_last_access[oldest_index] = ++tlb_hits;

    // Update page table
    page_table_last_access[page_number] = tlb_hits;

    int frame_number = page_table[page_number];
    return (frame_number * PAGE_SIZE) + offset;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s BACKING_STORE.bin addresses.txt\n", argv[0]);
        return EXIT_FAILURE;
    }

    backing_store = fopen(argv[1], "rb");
    if (backing_store == NULL) {
        fprintf(stderr, "Could not open backing store file: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    addresses = fopen(argv[2], "r");
    if (addresses == NULL) {
        fprintf(stderr, "Could not open addresses file: %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    // Initialize page table to all TLB_MISS
    for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
        page_table[i] = TLB_MISS;
    }

    // Initialize TLB and free frame list
    for (int i = 0; i < TLB_SIZE; i++) {
        tlb[i][0] = TLB_MISS;
        free_frame_list[i] = i;
    }

    // Translate each address and print out the physical address and value
    int logical_address, physical_address, value;
    while (fscanf(addresses, "%d", &logical_address) != EOF) {
        physical_address = translate_address(logical_address);
        value = physical_memory[physical_address];
        printf("Logical address: %d ; Physical address: %d ; Signed Byte Value: %d\n", logical_address, physical_address, value);
    }

    // Print statistics
    printf("Number of page faults: %d\n", page_faults);
    printf("Number of TLB hits: %d\n", tlb_hits);

    // Clean up
    fclose(backing_store);
    fclose(addresses);
    return 0;
}

