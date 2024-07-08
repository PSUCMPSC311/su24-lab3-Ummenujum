#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"

#include "tester.h"



static int mounted = 0;
static int write_permission = 0;

// Function to pack the command and parameters into a 32-bit integer
uint32_t pack_byte(uint32_t DiskID, uint32_t BlockID, uint32_t Command, uint32_t Reserved) {
    uint32_t returnVal = 0x0, tempDiskID, tempBlockID, tempCommand, tempReserved;

    tempCommand = (Command & 0x3F); // First 6 bits (0-5)
    tempDiskID = (DiskID & 0xF) << 6; // Next 4 bits (6-9)
    tempBlockID = (BlockID & 0xFF) << 10; // Next 8 bits (10-17)
    tempReserved = (Reserved & 0x3FFF) << 18; // Last 14 bits (18-31)
    returnVal = tempBlockID | tempCommand | tempDiskID | tempReserved; // OR all the values

    return returnVal;
}

int mdadm_mount(void) {
    if (mounted) {
        return -1; // already mounted
    }
    uint32_t op = pack_byte(0, 0, JBOD_MOUNT, 0); // mount command
    if (jbod_operation(op, NULL) == 0) {
        mounted = 1;
        return 1; // success
    }
    return -1; // failure
}

int mdadm_unmount(void) {
    if (!mounted) {
        return -1; // already unmounted
    }
    uint32_t op = pack_byte(0, 0, JBOD_UNMOUNT, 0); // unmount command
    if (jbod_operation(op, NULL) == 0) {
        mounted = 0;
        return 1; // success
    }
    return -1; // failure
}

int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf) {
    if (!mounted) {
        return -1;
    }

    if (read_len == 0 && read_buf == NULL) {
        return 0; // success for 0-length read
    }

    if (read_len > MAX_IO_SIZE || read_buf == NULL) {
        return -1;
    }

    if (start_addr + read_len > JBOD_NUM_DISKS * JBOD_DISK_SIZE) {
        return -1; // out of bounds read
    }

    uint32_t disk_num, block_num, offset, bytes_to_read, bytes_read = 0;
    uint8_t temp_buf[JBOD_BLOCK_SIZE];
    uint32_t op;

    // Loop to read the needed amount of data
    while (bytes_read < read_len) {
        disk_num = start_addr / JBOD_DISK_SIZE;
        block_num = (start_addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
        offset = start_addr % JBOD_BLOCK_SIZE;
        bytes_to_read = (read_len - bytes_read < JBOD_BLOCK_SIZE - offset) ? read_len - bytes_read : JBOD_BLOCK_SIZE - offset;

        // Seek to the correct disk
        op = pack_byte(disk_num, 0, JBOD_SEEK_TO_DISK, 0);
        if (jbod_operation(op, NULL) != 0) {
            return -1;
        }

        // Seek to the correct block
        op = pack_byte(0, block_num, JBOD_SEEK_TO_BLOCK, 0);
        if (jbod_operation(op, NULL) != 0) {
            return -1;
        }

        // Read the block
        op = pack_byte(0, 0, JBOD_READ_BLOCK, 0);
        if (jbod_operation(op, temp_buf) != 0) {
            return -1;
        }

        // Copy the required bytes from temp_buf to read_buf
        memcpy(read_buf + bytes_read, temp_buf + offset, bytes_to_read);

        // Update counters
        start_addr += bytes_to_read;
        bytes_read += bytes_to_read;
    }

    return bytes_read;
}

int mdadm_write_permission(void) {
    uint32_t op = pack_byte(0, 0, JBOD_WRITE_PERMISSION, 0);
    if (jbod_operation(op, NULL) == 0) {
        write_permission = 1;
        return 1;
    }
    return -1;
}

int mdadm_revoke_write_permission(void) {
    uint32_t op = pack_byte(0, 0, JBOD_REVOKE_WRITE_PERMISSION, 0);
    if (jbod_operation(op, NULL) == 0) {
        write_permission = 0;
        return 1;
    }
    return -1;
}

int mdadm_write(uint32_t start_addr, uint32_t write_len, const uint8_t *write_buf) {
    if (!mounted || !write_permission) {
        return -1;
    }

    if (write_len == 0 && write_buf == NULL) {
        return 0; // success for 0-length write
    }

    if (write_len > MAX_IO_SIZE || write_buf == NULL) {
        return -1;
    }

    if (start_addr + write_len > JBOD_NUM_DISKS * JBOD_DISK_SIZE) {
        return -1; // out of bounds write
    }

    uint32_t disk_num, block_num, offset, bytes_to_write, bytes_written = 0;
    uint8_t temp_buf[JBOD_BLOCK_SIZE];
    uint32_t op;

    while (bytes_written < write_len) {
        disk_num = start_addr / JBOD_DISK_SIZE;
        block_num = (start_addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
        offset = start_addr % JBOD_BLOCK_SIZE;
        bytes_to_write = (write_len - bytes_written < JBOD_BLOCK_SIZE - offset) ? write_len - bytes_written : JBOD_BLOCK_SIZE - offset;

        // Seek to the correct disk
        op = pack_byte(disk_num, 0, JBOD_SEEK_TO_DISK, 0);
        if (jbod_operation(op, NULL) != 0) {
            return -1;
        }

        // Seek to the correct block
        op = pack_byte(0, block_num, JBOD_SEEK_TO_BLOCK, 0);
        if (jbod_operation(op, NULL) != 0) {
            return -1;
        }

        // Read the current block content into temp_buf
        op = pack_byte(0, 0, JBOD_READ_BLOCK, 0);
        if (jbod_operation(op, temp_buf) != 0) {
            return -1;
        }

        // Update the temp_buf with the write data
        memcpy(temp_buf + offset, write_buf + bytes_written, bytes_to_write);

        // Write the updated temp_buf back to the block
        op = pack_byte(0, 0, JBOD_WRITE_BLOCK, 0);
        if (jbod_operation(op, temp_buf) != 0) {
            return -1;
        }

        // Update counters
        start_addr += bytes_to_write;
        bytes_written += bytes_to_write;
    }

    return bytes_written;
}
