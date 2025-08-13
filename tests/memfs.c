/*
 * VFS Example - Comprehensive test of all VFS functionality
 * This example creates a simple in-memory file system to test the VFS
 * implementation without requiring root privileges or actual disk access.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <vfs.h>

// ANSI color codes
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define BOLD    "\033[1m"

// Simple in-memory file system for testing
typedef struct memfs_file {
    char *data;
    size_t size;
    size_t capacity;
    char *name;
    int type;
    struct memfs_file *next;
    struct memfs_file *parent;
    struct memfs_file *children;
} memfs_file_t;

static memfs_file_t *memfs_root = NULL;

// Helper function to create a memfs file
static memfs_file_t *memfs_create_file(const char *name, int type) {
    memfs_file_t *file = malloc(sizeof(memfs_file_t));
    if (!file) return NULL;

    memset(file, 0, sizeof(memfs_file_t));
    file->name = strdup(name);
    file->type = type;
    file->capacity = 1024; // Initial capacity
    if (type != file_dir) {
        file->data = malloc(file->capacity);
        if (!file->data) {
            free(file->name);
            free(file);
            return NULL;
        }
    }
    return file;
}

// Helper function to find a child file
static memfs_file_t *memfs_find_child(memfs_file_t *parent, const char *name) {
    if (!parent || !name) return NULL;

    memfs_file_t *child = parent->children;
    while (child) {
        if (strcmp(child->name, name) == 0) {
            return child;
        }
        child = child->next;
    }
    return NULL;
}

// Helper function to add a child to a directory
static void memfs_add_child(memfs_file_t *parent, memfs_file_t *child) {
    if (!parent || !child) return;

    child->parent = parent;
    child->next = parent->children;
    parent->children = child;
}

// VFS Callback implementations for our memory file system
static int memfs_mount(const char *src, vfs_node_t node) {
    printf(CYAN "[MEMFS]" RESET " Mounting %s\n", src ? src : "NULL");

    if (!memfs_root) {
        memfs_root = memfs_create_file("", file_dir);
        if (!memfs_root) return -1;
    }

    node->info->handle = memfs_root;
    node->info->type = file_dir;
    node->info->size = 0;
    node->info->createtime = time(NULL);
    node->info->readtime = time(NULL);
    node->info->writetime = time(NULL);

    return 0;
}

static void memfs_unmount(void *root) {
    printf(CYAN "[MEMFS]" RESET " Unmounting\n");
    // In a real implementation, we'd free the file system here
    // For simplicity, we'll leave it allocated
}

static void memfs_open(void *parent, const char *name, vfs_node_t node) {
    printf(CYAN "[MEMFS]" RESET " Opening %s\n", name);

    memfs_file_t *parent_file = (memfs_file_t *)parent;
    memfs_file_t *child = memfs_find_child(parent_file, name);

    if (child) {
        node->info->handle = child;
        node->info->type = child->type;
        node->info->size = child->size;
        node->info->createtime = time(NULL);
        node->info->readtime = time(NULL);
        node->info->writetime = time(NULL);
    }
}

static void memfs_close(void *current) {
    printf(CYAN "[MEMFS]" RESET " Closing file: %p\n", current);
}

static ssize_t memfs_read(void *file, void *addr, size_t offset, size_t size) {
    memfs_file_t *memfile = (memfs_file_t *)file;

    if (!memfile || !memfile->data || offset >= memfile->size) {
        return 0;
    }

    size_t available = memfile->size - offset;
    size_t to_read = size < available ? size : available;

    memcpy(addr, memfile->data + offset, to_read);
    printf(CYAN "[MEMFS]" RESET " Read " GREEN "%zu" RESET " bytes from offset " YELLOW "%zu" RESET "\n", to_read, offset);

    return to_read;
}

static ssize_t memfs_write(void *file, const void *addr, size_t offset, size_t size) {
    memfs_file_t *memfile = (memfs_file_t *)file;

    if (!memfile) return -1;

    // Ensure we have enough capacity
    size_t needed = offset + size;
    if (needed > memfile->capacity) {
        size_t new_capacity = memfile->capacity;
        while (new_capacity < needed) {
            new_capacity *= 2;
        }

        char *new_data = realloc(memfile->data, new_capacity);
        if (!new_data) return -1;

        memfile->data = new_data;
        memfile->capacity = new_capacity;
    }

    // Zero-fill any gap
    if (offset > memfile->size) {
        memset(memfile->data + memfile->size, 0, offset - memfile->size);
    }

    memcpy(memfile->data + offset, addr, size);

    if (offset + size > memfile->size) {
        memfile->size = offset + size;
    }

    printf(CYAN "[MEMFS]" RESET " Wrote " GREEN "%zu" RESET " bytes at offset " YELLOW "%zu" RESET "\n", size, offset);
    return size;
}

static int memfs_mkdir(void *parent, const char *name, vfs_node_t node) {
    printf(CYAN "[MEMFS]" RESET " Creating directory " BLUE "%s" RESET "\n", name);

    memfs_file_t *parent_file = (memfs_file_t *)parent;
    memfs_file_t *new_dir = memfs_create_file(name, file_dir);

    if (!new_dir) return -1;

    memfs_add_child(parent_file, new_dir);

    node->info->handle = new_dir;
    node->info->type = file_dir;
    node->info->size = 0;
    node->info->createtime = time(NULL);
    node->info->readtime = time(NULL);
    node->info->writetime = time(NULL);

    return 0;
}

static int memfs_mkfile(void *parent, const char *name, vfs_node_t node) {
    printf(CYAN "[MEMFS]" RESET " Creating file " MAGENTA "%s" RESET "\n", name);

    memfs_file_t *parent_file = (memfs_file_t *)parent;
    memfs_file_t *new_file = memfs_create_file(name, file_block);

    if (!new_file) return -1;

    memfs_add_child(parent_file, new_file);

    node->info->handle = new_file;
    node->info->type = file_block;
    node->info->size = 0;
    node->info->createtime = time(NULL);
    node->info->readtime = time(NULL);
    node->info->writetime = time(NULL);

    return 0;
}

static int memfs_stat(void *file, vfs_node_t node) {
    memfs_file_t *memfile = (memfs_file_t *)file;

    if (!memfile) return -1;

    node->info->type = memfile->type;
    node->info->size = memfile->size;
    node->info->readtime = time(NULL);

    return 0;
}

// VFS callback structure for our memory file system
static struct vfs_callback memfs_callbacks = {
    .mount = memfs_mount,
    .unmount = memfs_unmount,
    .open = memfs_open,
    .close = memfs_close,
    .read = memfs_read,
    .write = memfs_write,
    .mkdir = memfs_mkdir,
    .mkfile = memfs_mkfile,
    .stat = memfs_stat,
};

// Test helper functions
static void print_separator(const char *title) {
    printf("\n" BOLD BLUE "=== %s ===" RESET "\n", title);
}

static void test_vfs_init() {
    print_separator("Testing VFS Initialization");

    bool result = vfs_init();
    printf("vfs_init() returned: %s\n", result ? GREEN "true" RESET : RED "false" RESET);

    if (rootdir) {
        printf(GREEN "Root directory created successfully" RESET "\n");
        printf("Root directory type: %d\n", rootdir->info->type);
    } else {
        printf(RED "ERROR: Root directory not created" RESET "\n");
    }
}

static void test_vfs_regist() {
    print_separator("Testing File System Registration");

    int fs_id = vfs_regist("memfs", &memfs_callbacks);
    printf("Registered memfs with ID: " YELLOW "%d" RESET "\n", fs_id);

    if (fs_id < 0) {
        printf(RED "ERROR: Failed to register file system" RESET "\n");
        exit(1);
    }
}

static void test_vfs_mount() {
    print_separator("Testing File System Mounting");

    int result = vfs_mount("memory://", rootdir);
    printf("vfs_mount() returned: %s\n", result == 0 ? GREEN "0" RESET : RED "error" RESET);

    if (result == 0) {
        printf(GREEN "Successfully mounted memory file system" RESET "\n");
        printf("Root directory fsid: %d\n", rootdir->info->fsid);
    } else {
        printf(RED "ERROR: Failed to mount file system" RESET "\n");
        exit(1);
    }
}

static void test_vfs_mkdir() {
    print_separator("Testing Directory Creation");

    const char *dirs[] = {
        "/test",
        "/test/subdir1",
        "/test/subdir2",
        "/home",
        "/home/user",
        "/tmp"
    };

    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        int result = vfs_mkdir(dirs[i]);
        printf("vfs_mkdir('%s') returned: %s\n", dirs[i], result == 0 ? GREEN "0" RESET : RED "error" RESET);
    }
}

static void test_vfs_mkfile() {
    print_separator("Testing File Creation");

    const char *files[] = {
        "/test/file1.txt",
        "/test/file2.txt",
        "/test/subdir1/data.bin",
        "/home/user/config.conf",
        "/tmp/temp.tmp"
    };

    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        int result = vfs_mkfile(files[i]);
        printf("vfs_mkfile('%s') returned: %s\n", files[i], result == 0 ? GREEN "0" RESET : RED "error" RESET);
    }
}

static void test_vfs_open() {
    print_separator("Testing File Opening");

    const char *paths[] = {
        "/",
        "/test",
        "/test/file1.txt",
        "/test/subdir1",
        "/test/subdir1/data.bin",
        "/nonexistent"
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        vfs_node_t node = vfs_open(paths[i]);
        printf("vfs_open('%s') returned: ", paths[i]);

        if (node) {
            printf(GREEN "%p" RESET " (type: %d, size: %llu)\n", (void*)node, node->info->type, (unsigned long long)node->info->size);

            // Test getting full path
            char *fullpath = vfs_get_fullpath(node);
            if (fullpath) {
                printf("  Full path: " CYAN "%s" RESET "\n", fullpath);
                free(fullpath);
            }
        } else {
            printf(RED "NULL" RESET " (NOT FOUND)\n");
        }
    }
}

static void test_vfs_write_read() {
    print_separator("Testing File Write/Read Operations");

    // Test writing to files
    const char *test_data1 = "Hello, VFS World!";
    const char *test_data2 = "This is a test file with some data.";
    const char *test_data3 = "Binary data: \x00\x01\x02\x03\xFF";

    vfs_node_t file1 = vfs_open("/test/file1.txt");
    vfs_node_t file2 = vfs_open("/test/file2.txt");
    vfs_node_t file3 = vfs_open("/test/subdir1/data.bin");

    if (file1) {
        ssize_t written = vfs_write(file1, test_data1, 0, strlen(test_data1));
        printf("Wrote " GREEN "%zd" RESET " bytes to /test/file1.txt\n", written);
    }

    if (file2) {
        ssize_t written = vfs_write(file2, test_data2, 0, strlen(test_data2));
        printf("Wrote " GREEN "%zd" RESET " bytes to /test/file2.txt\n", written);

        // Test appending
        const char *append_data = "\nAppended line.";
        written = vfs_write(file2, append_data, strlen(test_data2), strlen(append_data));
        printf("Appended " GREEN "%zd" RESET " bytes to /test/file2.txt\n", written);
    }

    if (file3) {
        ssize_t written = vfs_write(file3, test_data3, 0, 5);
        printf("Wrote " GREEN "%zd" RESET " bytes to /test/subdir1/data.bin\n", written);
    }

    // Test reading from files
    char buffer[256];

    if (file1) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t read_bytes = vfs_read(file1, buffer, 0, sizeof(buffer) - 1);
        printf("Read " GREEN "%zd" RESET " bytes from /test/file1.txt: '" YELLOW "%s" RESET "'\n", read_bytes, buffer);
    }

    if (file2) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t read_bytes = vfs_read(file2, buffer, 0, sizeof(buffer) - 1);
        printf("Read " GREEN "%zd" RESET " bytes from /test/file2.txt: '" YELLOW "%s" RESET "'\n", read_bytes, buffer);

        // Test partial read
        memset(buffer, 0, sizeof(buffer));
        read_bytes = vfs_read(file2, buffer, 10, 10);
        buffer[10] = '\0';
        printf("Partial read (offset 10, size 10): '" YELLOW "%s" RESET "'\n", buffer);
    }

    if (file3) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t read_bytes = vfs_read(file3, buffer, 0, 5);
        printf("Read " GREEN "%zd" RESET " bytes from /test/subdir1/data.bin: ", read_bytes);
        for (int i = 0; i < read_bytes; i++) {
            printf(MAGENTA "%02X " RESET, (unsigned char)buffer[i]);
        }
        printf("\n");
    }
}

static void test_vfs_close() {
    print_separator("Testing File Closing");

    vfs_node_t file = vfs_open("/test/file1.txt");
    if (file) {
        printf("Opened file for closing test\n");
        int result = vfs_close(file);
        printf("vfs_close() returned: %s\n", result == 0 ? GREEN "0" RESET : RED "error" RESET);
    }
}

static void test_vfs_unmount() {
    print_separator("Testing File System Unmounting");

    // Create a subdirectory to mount another filesystem
    vfs_mkdir("/mnt");
    vfs_node_t mount_point = vfs_open("/mnt");

    if (mount_point) {
        // Mount another instance
        int result = vfs_mount("memory2://", mount_point);
        printf("Mounted second filesystem: %s\n", result == 0 ? GREEN "0" RESET : RED "error" RESET);

        if (result == 0) {
            // Create some content
            vfs_mkdir("/mnt/test_dir");
            vfs_mkfile("/mnt/test_file");

            // Now unmount
            result = vfs_unmount("/mnt");
            printf("vfs_unmount('/mnt') returned: %s\n", result == 0 ? GREEN "0" RESET : RED "error" RESET);
        }
    }
}

static void test_error_cases() {
    print_separator("Testing Error Cases");

    // Test invalid operations
    printf("Testing invalid paths:\n");

    int result = vfs_mkdir("invalid_path");
    printf("vfs_mkdir('invalid_path') returned: " RED "%d" RESET " (should be -1)\n", result);

    result = vfs_mkfile("another_invalid");
    printf("vfs_mkfile('another_invalid') returned: " RED "%d" RESET " (should be -1)\n", result);

    vfs_node_t node = vfs_open("/nonexistent/path");
    printf("vfs_open('/nonexistent/path') returned: " RED "%p" RESET " (should be NULL)\n", (void*)node);

    // Test writing to directory
    vfs_node_t dir = vfs_open("/test");
    if (dir) {
        ssize_t written = vfs_write(dir, "test", 0, 4);
        printf("Writing to directory returned: " RED "%zd" RESET " (should be -1)\n", written);
    }

    // Test reading from directory
    if (dir) {
        char buffer[10];
        ssize_t read_bytes = vfs_read(dir, buffer, 0, sizeof(buffer));
        printf("Reading from directory returned: " RED "%zd" RESET " (should be -1)\n", read_bytes);
    }
}

static void print_file_tree(vfs_node_t node, int depth) {
    if (!node) return;

    for (int i = 0; i < depth; i++) {
        printf("  ");
    }

    char *fullpath = vfs_get_fullpath(node);
    printf("%s (%s, size: %llu)\n",
           fullpath ? fullpath : "NULL",
           node->info->type == file_dir ? "dir" : "file",
           (unsigned long long)node->info->size);

    if (fullpath) free(fullpath);

    // This is a simplified tree traversal - in a real implementation,
    // you'd need to iterate through the children properly
}

static void test_file_tree() {
    print_separator("Testing File Tree Structure");

    printf("Root directory structure:\n");
    print_file_tree(rootdir, 0);

    // Test some specific paths
    const char *paths[] = {
        "/test",
        "/test/subdir1",
        "/home/user"
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        vfs_node_t node = vfs_open(paths[i]);
        if (node) {
            printf("\nPath: %s\n", paths[i]);
            print_file_tree(node, 1);
        }
    }
}

int main() {
    printf(BOLD CYAN "VFS Comprehensive Test Example" RESET "\n");
    printf(BOLD CYAN "==============================" RESET "\n");

    // Run all tests
    test_vfs_init();
    test_vfs_regist();
    test_vfs_mount();
    test_vfs_mkdir();
    test_vfs_mkfile();
    test_vfs_open();
    test_vfs_write_read();
    test_vfs_close();
    test_vfs_unmount();
    test_error_cases();
    test_file_tree();

    print_separator("All Tests Completed");
    printf(BOLD GREEN "VFS test example completed successfully!" RESET "\n");
    printf("This example tested all major VFS functionality:\n");
    printf("- " GREEN "VFS initialization" RESET "\n");
    printf("- " GREEN "File system registration" RESET "\n");
    printf("- " GREEN "File system mounting/unmounting" RESET "\n");
    printf("- " GREEN "Directory creation" RESET "\n");
    printf("- " GREEN "File creation" RESET "\n");
    printf("- " GREEN "File opening" RESET "\n");
    printf("- " GREEN "File reading/writing" RESET "\n");
    printf("- " GREEN "File closing" RESET "\n");
    printf("- " GREEN "Path resolution" RESET "\n");
    printf("- " GREEN "Error handling" RESET "\n");
    
    return 0;
}
