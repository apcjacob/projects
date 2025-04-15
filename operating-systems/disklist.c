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
void read_directory(FILE *fp, uint32_t start_block, uint32_t block_count, uint16_t block_size, uint32_t total_blocks);
int find_subdirectory(FILE *fp, uint32_t start_block, uint32_t block_count, uint16_t block_size,
                      const char *sub_dir, uint32_t *sub_start_block, uint32_t *sub_block_count, uint32_t total_blocks);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <disk image> <directory path>\n", argv[0]);
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

    uint32_t dir_start_block = root_start_block;
    uint32_t dir_block_count = root_block_count;

    // Check if a subdirectory path is provided
    if (strcmp(argv[2], "/") != 0) {
        const char *sub_dir = argv[2] + 1; // Skip the leading '/'
        if (!find_subdirectory(fp, root_start_block, root_block_count, block_size, sub_dir,
                               &dir_start_block, &dir_block_count, root_block_count + fat_blocks)) {
            fprintf(stderr, "Error: Subdirectory %s not found.\n", argv[2]);
            fclose(fp);
            return EXIT_FAILURE;
        }
    }

    // Read and display the directory contents
    read_directory(fp, dir_start_block, dir_block_count, block_size, root_block_count + fat_blocks);

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

int find_subdirectory(FILE *fp, uint32_t start_block, uint32_t block_count, uint16_t block_size,
                      const char *path, uint32_t *sub_start_block, uint32_t *sub_block_count, uint32_t total_blocks) {
    char *path_copy = strdup(path); // Make a copy of the path
    char *token = strtok(path_copy, "/"); // Tokenize the path into directory names

    uint32_t current_start_block = start_block;
    uint32_t current_block_count = block_count;

    while (token) {
        uint8_t buffer[block_size * current_block_count];

        // Read the current directory into memory
        fseek(fp, current_start_block * block_size, SEEK_SET);
        fread(buffer, block_size, current_block_count, fp);

        int found = 0;
        for (size_t i = 0; i < block_size * current_block_count; i += DIRECTORY_ENTRY_SIZE) {
            struct dir_entry_t entry;
            memcpy(&entry, buffer + i, DIRECTORY_ENTRY_SIZE);

            // Skip unused or invalid entries
            if (entry.status == 0x00 || entry.status == 0xFF) {
                continue;
            }

            // Ensure filename is null-terminated
            entry.filename[30] = '\0';

            // Correctly interpret starting_block and block_count
            entry.starting_block = ntohl(entry.starting_block);
            entry.block_count = ntohl(entry.block_count);

            // Check if the current entry matches the directory name
            if (entry.status == 0x05 && strcmp(entry.filename, token) == 0) {
                current_start_block = entry.starting_block;
                current_block_count = entry.block_count;
                found = 1;
                break;
            }
        }

        if (!found) {
            free(path_copy);
            return 0; // Subdirectory not found
        }

        token = strtok(NULL, "/"); // Move to the next level of the path
    }

    *sub_start_block = current_start_block;
    *sub_block_count = current_block_count;
    free(path_copy);
    return 1; // Successfully found the target subdirectory
}

// Function to read and display directory contents
void read_directory(FILE *fp, uint32_t start_block, uint32_t block_count, uint16_t block_size, uint32_t total_blocks) {
    uint8_t buffer[block_size * block_count];

    // Seek to the start of the directory
    fseek(fp, start_block * block_size, SEEK_SET);

    // Read the directory into memory
    size_t bytes_read = fread(buffer, block_size, block_count, fp);
    if (bytes_read != block_count) {
        fprintf(stderr, "ERROR: Failed to read directory data. Expected %u blocks, read %zu blocks.\n",
                block_count, bytes_read);
        return;
    }

    // Parse each directory entry
    for (size_t i = 0; i < block_size * block_count; i += DIRECTORY_ENTRY_SIZE) {
        struct dir_entry_t entry;
        memcpy(&entry, buffer + i, DIRECTORY_ENTRY_SIZE);

        // Skip unused or invalid entries
        if (entry.status == 0x00 || entry.status == 0xFF) {
            continue;
        }

        // Determine if the entry is a file or directory
        char type = (entry.status == 0x05) ? 'D' : 'F';

        // Ensure filename is null-terminated
        entry.filename[30] = '\0';

        // Correctly interpret file size
        uint32_t file_size = ntohl(entry.file_size);

        // Print file or directory details
        printf("%c %10u %30s %04u/%02u/%02u %02u:%02u:%02u\n",
               type,
               (type == 'D') ? 0 : file_size,
               entry.filename,
               ntohs(entry.modify_year), // Convert to host byte order
               entry.modify_month,
               entry.modify_day,
               entry.modify_hour,
               entry.modify_minute,
               entry.modify_second);
    }
}
