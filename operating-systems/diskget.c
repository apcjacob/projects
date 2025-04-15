#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h> 

#define DIRECTORY_ENTRY_SIZE 64
#define SUPER_BLOCK_SIZE 512

// Directory entry structure
struct __attribute__((packed)) dir_entry_t {
    uint8_t status;            
    uint32_t starting_block; 
    uint32_t block_count;    
    uint32_t file_size;  
    uint16_t create_year;
    uint8_t create_month;
    uint8_t create_day;
    uint8_t create_hour;
    uint8_t create_minute;
    uint8_t create_second;
    uint16_t modify_year;
    uint8_t modify_month;
    uint8_t modify_day;
    uint8_t modify_hour;
    uint8_t modify_minute;
    uint8_t modify_second;
    char filename[31];   
    uint8_t unused[6]; 
};

// Function prototypes
void read_superblock(FILE *fp, uint16_t *block_size, uint32_t *root_start_block,
                     uint32_t *root_block_count, uint32_t *fat_start, uint32_t *fat_blocks);
int find_file(FILE *fp, uint32_t start_block, uint32_t block_count, uint16_t block_size,
              const char *filepath, struct dir_entry_t *entry);
void copy_file(FILE *fp, const struct dir_entry_t *entry, const char *output_filename,
               uint16_t block_size, uint32_t fat_start);


int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <disk image> <file path> <output file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Open the file system image
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    // Dynamically retrieve file system parameters
    uint16_t block_size;
    uint32_t root_start_block, root_block_count, fat_start, fat_blocks;
    read_superblock(fp, &block_size, &root_start_block, &root_block_count, &fat_start, &fat_blocks);

    // Find the file in the file system
    struct dir_entry_t entry;
    if (!find_file(fp, root_start_block, root_block_count, block_size, argv[2], &entry)) {
        fprintf(stderr, "File not found.\n");
        fclose(fp);
        return EXIT_FAILURE;
    }

    // Copy the file to the host operating system
    copy_file(fp, &entry, argv[3], block_size, fat_start);

    fclose(fp);
    return EXIT_SUCCESS;
}

// Function to read the superblock
void read_superblock(FILE *fp, uint16_t *block_size, uint32_t *root_start_block,
                     uint32_t *root_block_count, uint32_t *fat_start, uint32_t *fat_blocks) {
    uint8_t buffer[SUPER_BLOCK_SIZE];
    fseek(fp, 0, SEEK_SET);
    fread(buffer, 1, SUPER_BLOCK_SIZE, fp);

    memcpy(block_size, buffer + 8, sizeof(uint16_t));
    *block_size = ntohs(*block_size);

    memcpy(fat_start, buffer + 14, sizeof(uint32_t));
    *fat_start = ntohl(*fat_start);

    memcpy(fat_blocks, buffer + 18, sizeof(uint32_t));
    *fat_blocks = ntohl(*fat_blocks);

    memcpy(root_start_block, buffer + 22, sizeof(uint32_t));
    *root_start_block = ntohl(*root_start_block);

    memcpy(root_block_count, buffer + 26, sizeof(uint32_t));
    *root_block_count = ntohl(*root_block_count);
}

// Function to find a file by its path
int find_file(FILE *fp, uint32_t start_block, uint32_t block_count, uint16_t block_size,
              const char *filepath, struct dir_entry_t *entry) {
    uint8_t buffer[block_size * block_count];
    char *path_copy = strdup(filepath);
    char *token = strtok(path_copy, "/");

    while (token) {
        // Read the current directory
        fseek(fp, start_block * block_size, SEEK_SET);
        fread(buffer, block_size, block_count, fp);

        int found = 0;
        for (size_t i = 0; i < block_size * block_count; i += DIRECTORY_ENTRY_SIZE) {
            struct dir_entry_t current_entry;
            memcpy(&current_entry, buffer + i, DIRECTORY_ENTRY_SIZE);

            // Convert fields to host-endian format
            current_entry.starting_block = ntohl(current_entry.starting_block);
            current_entry.block_count = ntohl(current_entry.block_count);
            current_entry.file_size = ntohl(current_entry.file_size);

            // Skip unused or invalid entries
            if (current_entry.status == 0x00 || current_entry.status == 0xFF) {
                continue;
            }

            // Ensure filename is null-terminated
            current_entry.filename[30] = '\0';

            // Compare filenames
            if (strcmp(current_entry.filename, token) == 0) {
                *entry = current_entry; // Update the entry
                start_block = current_entry.starting_block; // Move to the starting block of the directory or file
                block_count = current_entry.block_count;

                // Move to the next token
                token = strtok(NULL, "/");
                if (!token) {
                    free(path_copy);
                    return 1; // File found
                }

                found = 1; // Mark as found and continue to the next token
                break;
            }
        }
        
        if (!found) {
            free(path_copy);
            return 0; // File not found
        }
    }

    free(path_copy);
    return 0; // File not found
}

// Function to copy a file to the host system
void copy_file(FILE *fp, const struct dir_entry_t *entry, const char *output_filename,
               uint16_t block_size, uint32_t fat_start) {
    FILE *out_fp = fopen(output_filename, "wb");
    if (!out_fp) {
        perror("Error creating output file");
        return;
    }

    uint32_t remaining_size = entry->file_size;
    uint32_t current_block = entry->starting_block;
    
    // Allocate a buffer for reading data from the disk
    uint8_t *buffer = malloc(block_size);
    
    // Loop through the file's blocks and copy data until the entire file is read
    while (remaining_size > 0) {
        fseek(fp, current_block * block_size, SEEK_SET);
        size_t to_read = (remaining_size < block_size) ? remaining_size : block_size;
        fread(buffer, 1, to_read, fp);
        fwrite(buffer, 1, to_read, out_fp);

        remaining_size -= to_read;

        // Read next block from FAT
        fseek(fp, fat_start * block_size + current_block * sizeof(uint32_t), SEEK_SET);
        fread(&current_block, sizeof(uint32_t), 1, fp);
        current_block = ntohl(current_block);
        if (current_block == 0xFFFFFFFF) {
            break; // End of file
        }
    }

    free(buffer);
    fclose(out_fp);
}


