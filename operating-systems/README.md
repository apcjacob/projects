# p3

This repository contains the implementation of a basic file system emulator with the following four functionalities: diskinfo, disklist, diskget, and diskput. All features have been implemented as described in the rubric, and they work as expected.

# Compilation

The repository contains a Makefile that compiles all programs without errors.
Running make produces the following executables in the root directory:

    • diskinfo
    • disklist
    • diskget
    • diskput
You can compile the programs by running:

    make

# Functionalities:

## diskinfo
The diskinfo program provides information about the disk image, including block size, FAT table details, and the root directory.

    Implementation Features:
    - Outputs disk information in the exact format specified in the rubric.
    - Validated against:
    - Provided test image.
    - Additional test image.
    - Handles errors gracefully and ensures consistent formatting.

Sample Command

    ./diskinfo test.img

Sample Output
    
    Super block information:
    Block size: 512
    Block count: 6400
    FAT starts: 1
    FAT blocks: 50
    Root directory start: 51
    Root directory blocks: 8

    FAT information:
    Free Blocks: 6334
    Reserved Blocks: 49
    Allocated Blocks: 10

# disklist
The disklist program lists the contents of the specified directory in the disk image.

#### Implementation Features:
    - Lists files and subdirectories in the specified path.
    - Traverses nested directories, allowing paths like /, /    sub_dirA, and /sub_dirA/sub_dirB.
    - Outputs the exact format specified in the rubric.
    - Handles errors such as invalid paths or non-existent directories.

#### Sample Commands

    ./disklist test.img /
    ./disklist test.img /sub_dirA
    ./disklist test.img /sub_dirA/sub_dirB

#### Sample Output

    D          0                        sub_dirB 2024/11/27 14:32:00
    F       1024                   example.txt 2024/11/27 14:31:15

# diskget
The diskget program retrieves a file from the disk image and copies it to the current directory in the host operating system.

#### Implementation Features:
    • Outputs File not found. if the file does not exist in the specified directory.
    • Copies the specified file to the current directory in the host OS.
    • Ensures the copied file is identical to the original in the disk image (verified with cmp).

#### Sample Commands
    ./diskget test.img /example.txt
    ./diskget test.img /sub_dirA/sub_dirB/example.bin

#### Error Handling if the file does not exist:
    File not found.

# diskput
The diskput program copies a file from the host operating system into the specified directory in the disk image.

#### Implementation Features

    • Outputs File not found. if the file does not exist in the host OS.
    • Copies the file to the specified directory in the disk image.
    • Ensures the copied file can be retrieved using diskget and remains identical to the original file.
    • Automatically creates non-existent directories when copying to nested paths (e.g., /sub_dir/bar.txt).

#### Sample Commands
    ./diskput test.img foo.txt /sub_dir/bar.txt
    ./diskput test.img cat.jpg /images/cat.jpg

#### Error Handling if the file does not exist in the host OS:
    File not found.
