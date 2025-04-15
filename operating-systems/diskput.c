#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h> 

#define SUPER_BLOCK_SIZE 512
#define DIRECTORY_ENTRY_SIZE 64

// Structure for directory entries
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
void add_file_to_directory(FILE *fp, const char *file_path, const char *dest_path, 
                           uint32_t root_start_block, uint32_t root_block_count, 
                           uint16_t block_size, uint32_t fat_start, uint32_t fat_blocks);
void add_file_entry(FILE *fp, const char *file_path, const char *filename, 
                    uint32_t dir_start_block, uint32_t dir_block_count, 
                    uint16_t block_size, uint32_t fat_start, uint32_t fat_blocks);


int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <disk image> <input file> <destination path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *fp = fopen(argv[1], "r+b");
    if (!fp) {
        perror("Error opening disk image");
        return EXIT_FAILURE;
    }

    // Read superblock
    uint16_t block_size;
    uint32_t root_start_block, root_block_count, fat_start, fat_blocks;
    read_superblock(fp, &block_size, &root_start_block, &root_block_count, &fat_start, &fat_blocks);

    // Add file to directory
    add_file_to_directory(fp, argv[2], argv[3], root_start_block, root_block_count, block_size, fat_start, fat_blocks);

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

// Function to add file entry
void add_file_entry(FILE *fp, const char *file_path, const char *filename, 
                    uint32_t dir_start_block, uint32_t dir_block_count, 
                    uint16_t block_size, uint32_t fat_start, uint32_t fat_blocks) {
    
    FILE *input_fp = fopen(file_path, "r+b");
    if (!input_fp) {
        fprintf(stderr, "File not found.\n");
        return;
    }

    fseek(input_fp, 0, SEEK_END);
    uint32_t file_size = ftell(input_fp);
    fseek(input_fp, 0, SEEK_SET);

    // Read FAT
    uint8_t *fat = malloc(fat_blocks * block_size);
    fseek(fp, fat_start * block_size, SEEK_SET);
    fread(fat, fat_blocks * block_size, 1, fp);

    uint32_t *fat_entries = (uint32_t *)fat;
    uint32_t blocks_needed = (file_size + block_size - 1) / block_size;
    uint32_t first_block = 0, previous_block = 0;

    for (uint32_t i = 0; i < fat_blocks * block_size / sizeof(uint32_t); i++) {
        if (ntohl(fat_entries[i]) == 0x00000000) { // Free block
            if (first_block == 0) {
                first_block = i; // First block of the file
            } else {
                fat_entries[previous_block] = htonl(i); // Link previous block to current
            }
            previous_block = i;
            blocks_needed--;

            if (blocks_needed == 0) {
                fat_entries[i] = htonl(0xFFFFFFFF); // Mark the last block as EOF
                break;
            }
        }
    }

    if (blocks_needed > 0) {
        fprintf(stderr, "Error: Not enough free blocks available.\n");
        free(fat);
        fclose(input_fp);
        return;
    }

    // Write FAT back
    fseek(fp, fat_start * block_size, SEEK_SET);
    fwrite(fat, fat_blocks * block_size, 1, fp);
    free(fat);

    // Add file entry to the directory
    uint8_t *directory = malloc(block_size * dir_block_count);
    fseek(fp, dir_start_block * block_size, SEEK_SET);
    fread(directory, block_size, dir_block_count, fp);

    struct dir_entry_t new_file = {0};
    new_file.status = 0x03; // File status
    new_file.starting_block = htonl(first_block);
    new_file.block_count = htonl((file_size + block_size - 1) / block_size);
    new_file.file_size = htonl(file_size);

    // Set time stamp
    time_t now = time(NULL);
    struct tm *current_time = localtime(&now);

    new_file.create_year = htons(current_time->tm_year + 1900);
    new_file.create_month = current_time->tm_mon + 1;
    new_file.create_day = current_time->tm_mday;
    new_file.create_hour = current_time->tm_hour;
    new_file.create_minute = current_time->tm_min;
    new_file.create_second = current_time->tm_sec;

    new_file.modify_year = new_file.create_year;
    new_file.modify_month = new_file.create_month;
    new_file.modify_day = new_file.create_day;
    new_file.modify_hour = new_file.create_hour;
    new_file.modify_minute = new_file.create_minute;
    new_file.modify_second = new_file.create_second;

    strncpy(new_file.filename, filename, 30);
    new_file.filename[30] = '\0';

    for (size_t i = 0; i < block_size * dir_block_count; i += DIRECTORY_ENTRY_SIZE) {
        struct dir_entry_t *entry = (struct dir_entry_t *)(directory + i);
        if (entry->status == 0x00 || entry->status == 0xFF) {
            memcpy(entry, &new_file, sizeof(struct dir_entry_t));
            break;
        }
    }

    // Write updated directory back to disk
    fseek(fp, dir_start_block * block_size, SEEK_SET);
    fwrite(directory, block_size * dir_block_count, 1, fp);
    free(directory);

    // Write file data to allocated blocks
    uint8_t *buffer = malloc(block_size);
    uint32_t remaining_size = file_size;
    uint32_t current_block = first_block;

    while (remaining_size > 0) {
        size_t to_write = (remaining_size < block_size) ? remaining_size : block_size;
        fread(buffer, 1, to_write, input_fp);
        fseek(fp, current_block * block_size, SEEK_SET);
        fwrite(buffer, 1, to_write, fp);
        remaining_size -= to_write;

        if (remaining_size > 0) {
            current_block = ntohl(fat_entries[current_block]);
        }
    }

    free(buffer);
    fclose(input_fp);

}

void add_file_to_directory(FILE *fp, const char *file_path, const char *dest_path, 
                           uint32_t root_start_block, uint32_t root_block_count, 
                           uint16_t block_size, uint32_t fat_start, uint32_t fat_blocks) {
    char *path_copy = strdup(dest_path);
    char *token = strtok(path_copy, "/");
    uint32_t current_start_block = root_start_block;
    uint32_t current_block_count = root_block_count;

    while (token) {
        char *next_token = strtok(NULL, "/");
        if (!next_token) {
            
            // No more subdirectories; add the file here
            add_file_entry(fp, file_path, token, current_start_block, current_block_count, 
                           block_size, fat_start, fat_blocks);

            free(path_copy);
            return;
        }

        // Traverse or create the subdirectory
        uint8_t *directory = malloc(block_size * current_block_count);
        fseek(fp, current_start_block * block_size, SEEK_SET);
        fread(directory, block_size, current_block_count, fp);

        int found = 0;
        for (size_t i = 0; i < block_size * current_block_count; i += DIRECTORY_ENTRY_SIZE) {
            struct dir_entry_t *entry = (struct dir_entry_t *)(directory + i);

            if (entry->status == 0x05 && strcmp(entry->filename, token) == 0) {
                current_start_block = ntohl(entry->starting_block);
                current_block_count = ntohl(entry->block_count);
                found = 1;
                break;
            }
        }

        if (!found) {
            // Create a new subdirectory
            // read FAT table
            uint8_t *fat = malloc(fat_blocks * block_size);
            fseek(fp, fat_start * block_size, SEEK_SET);
            fread(fat, fat_blocks * block_size, 1, fp);

            // Find a free block
            uint32_t *fat_entries = (uint32_t *)fat;
            uint32_t new_block = 0;
            for (uint32_t i = 0; i < fat_blocks * block_size / sizeof(uint32_t); i++) {
                if (ntohl(fat_entries[i]) == 0x00000000) {
                    new_block = i;
                    fat_entries[i] = htonl(0xFFFFFFFF); // Mark as allocated
                    break;
                }
            }

            // write back into disk
            fseek(fp, fat_start * block_size, SEEK_SET);
            fwrite(fat, fat_blocks * block_size, 1, fp);
            free(fat);

            // initialize directory entry
            struct dir_entry_t new_dir = {0};
            new_dir.status = 0x05; // Directory status
            new_dir.starting_block = htonl(new_block);
            new_dir.block_count = htonl(1);
            strncpy(new_dir.filename, token, 30);
            new_dir.filename[30] = '\0';


            // Set timestamps
            time_t now = time(NULL);
            struct tm *current_time = localtime(&now);

            new_dir.create_year = htons(current_time->tm_year + 1900);
            new_dir.create_month = current_time->tm_mon + 1;
            new_dir.create_day = current_time->tm_mday;
            new_dir.create_hour = current_time->tm_hour;
            new_dir.create_minute = current_time->tm_min;
            new_dir.create_second = current_time->tm_sec;

            new_dir.modify_year = new_dir.create_year;
            new_dir.modify_month = new_dir.create_month;
            new_dir.modify_day = new_dir.create_day;
            new_dir.modify_hour = new_dir.create_hour;
            new_dir.modify_minute = new_dir.create_minute;
            new_dir.modify_second = new_dir.create_second;

            // add entry into parent directory
            for (size_t i = 0; i < block_size * current_block_count; i += DIRECTORY_ENTRY_SIZE) {
                struct dir_entry_t *entry = (struct dir_entry_t *)(directory + i);
                if (entry->status == 0x00 || entry->status == 0xFF) {
                    memcpy(entry, &new_dir, sizeof(struct dir_entry_t));
                    break;
                }
            }

            // update disk
            fseek(fp, current_start_block * block_size, SEEK_SET);
            fwrite(directory, block_size * current_block_count, 1, fp);

            // go into new directory
            current_start_block = new_block;
            current_block_count = 1;
        }

        free(directory);
        token = next_token;
    }

    free(path_copy);
}
