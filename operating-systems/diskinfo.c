#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>

#define SUPER_BLOCK_SIZE 512

// Function prototypes
void read_superblock(FILE *fp, uint16_t *block_size, uint32_t *block_count, 
                     uint32_t *fat_start, uint32_t *fat_blocks, 
                     uint32_t *root_start, uint32_t *root_blocks);
void read_fat(FILE *fp, uint32_t fat_start, uint32_t fat_blocks, 
              uint32_t block_size, uint32_t *free_blocks, 
              uint32_t *reserved_blocks, uint32_t *allocated_blocks);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <disk image>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    uint16_t block_size;
    uint32_t block_count, fat_start, fat_blocks, root_start, root_blocks;

    // Read superblock information
    read_superblock(fp, &block_size, &block_count, &fat_start, &fat_blocks, &root_start, &root_blocks);

    // Print superblock information
    printf("Super block information:\n");
    printf("Block size: %u\n", block_size);
    printf("Block count: %u\n", block_count);
    printf("FAT starts: %u\n", fat_start);
    printf("FAT blocks: %u\n", fat_blocks);
    printf("Root directory start: %u\n", root_start);
    printf("Root directory blocks: %u\n", root_blocks);

    // Variables for FAT information
    uint32_t free_blocks = 0, reserved_blocks = 0, allocated_blocks = 0;

    // Read FAT information
    read_fat(fp, fat_start, fat_blocks, block_size, &free_blocks, &reserved_blocks, &allocated_blocks);

    // Print FAT information
    printf("\nFAT information:\n");
    printf("Free Blocks: %u\n", free_blocks);
    printf("Reserved Blocks: %u\n", reserved_blocks);
    printf("Allocated Blocks: %u\n", allocated_blocks);

    fclose(fp);
    return EXIT_SUCCESS;
}

// Function to read the superblock
void read_superblock(FILE *fp, uint16_t *block_size, uint32_t *block_count, 
                     uint32_t *fat_start, uint32_t *fat_blocks, 
                     uint32_t *root_start, uint32_t *root_blocks) {
    uint8_t buffer[SUPER_BLOCK_SIZE];

    fread(buffer, 1, SUPER_BLOCK_SIZE, fp);

    memcpy(block_size, buffer + 8, sizeof(uint16_t));
    *block_size = ntohs(*block_size);

    memcpy(block_count, buffer + 10, sizeof(uint32_t));
    *block_count = ntohl(*block_count);

    memcpy(fat_start, buffer + 14, sizeof(uint32_t));
    *fat_start = ntohl(*fat_start); 

    memcpy(fat_blocks, buffer + 18, sizeof(uint32_t));
    *fat_blocks = ntohl(*fat_blocks);

    memcpy(root_start, buffer + 22, sizeof(uint32_t));
    *root_start = ntohl(*root_start); 

    memcpy(root_blocks, buffer + 26, sizeof(uint32_t));
    *root_blocks = ntohl(*root_blocks);
}

// Function to read the FAT
void read_fat(FILE *fp, uint32_t fat_start, uint32_t fat_blocks, 
              uint32_t block_size, uint32_t *free_blocks, 
              uint32_t *reserved_blocks, uint32_t *allocated_blocks) {
    uint32_t fat_size = fat_blocks * block_size;
    uint8_t *fat = malloc(fat_size);

    if (!fat) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    // Seek to FAT start and read FAT into memory
    fseek(fp, fat_start * block_size, SEEK_SET);
    fread(fat, fat_size, 1, fp);

    // Parse FAT entries
    for (uint32_t i = 0; i < fat_size / 4; i++) {
        uint32_t entry;
        memcpy(&entry, fat + i * 4, sizeof(uint32_t));

        entry = ntohl(entry); // Convert to host byte order

        if (entry == 0x00000000) {
           (*free_blocks)++;
       } else if (entry == 0x00000001) {
           (*reserved_blocks)++;
       } else if (entry >= 0x00000002 && entry <= 0xFFFFFFFF) {
            (*allocated_blocks)++;
        }
    }

    free(fat);
}
