#include <vfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MEMFS_ID 0x01  // Filesystem ID for memfs
// Using PAGE_SIZE from header files

// Structure to represent an in-memory file
typedef struct memfs_file {
    void *data;          // File contents
    size_t allocated;    // Allocated size
    size_t size;         // Actual file size
    int type;            // File type (directory or regular file)
    char *name;          // File name
} memfs_file_t;

// Global root directory for memfs
static memfs_file_t *memfs_root = NULL;

// Utility function to create a new memfs file or directory
static memfs_file_t *memfs_create_file(const char *name, int type) {
    printf("Creating %s: %s\n", 
           type == file_dir ? "directory" : "file", 
           name ? name : "(null)");
           
    memfs_file_t *file = (memfs_file_t *)malloc(sizeof(memfs_file_t));
    if (!file) {
        printf("Error: Failed to allocate memory for file structure\n");
        return NULL;
    }

    file->name = strdup(name);
    if (!file->name) {
        printf("Error: Failed to duplicate filename\n");
        free(file);
        return NULL;
    }
    
    file->type = type;
    file->size = 0;
    
    if (type == file_dir) {
        file->data = NULL;  // Will be initialized as a list when needed
        file->allocated = 0;
        printf("Created directory '%s', handle: %p\n", file->name, file);
    } else {
        file->allocated = PAGE_SIZE;  // Start with one page
        file->data = malloc(file->allocated);
        if (!file->data) {
            printf("Error: Failed to allocate memory for file data\n");
            free(file->name);
            free(file);
            return NULL;
        }
        memset(file->data, 0, file->allocated);
        printf("Created file '%s', handle: %p, data: %p\n", file->name, file, file->data);
    }
    
    return file;
}

// Utility function to free a memfs file
static void memfs_free_file(memfs_file_t *file) {
    if (!file) return;
    
    if (file->data) free(file->data);
    if (file->name) free(file->name);
    free(file);
}

// Utility function to find a file in a directory by name
static memfs_file_t *memfs_find_in_dir(memfs_file_t *dir, const char *name) {
    if (!dir || dir->type != file_dir) {
        printf("find_in_dir: Invalid directory or not a directory type\n");
        return NULL;
    }
    
    if (!name) {
        printf("find_in_dir: Name is NULL\n");
        return NULL;
    }
    
    printf("find_in_dir: Looking for '%s' in directory '%s'\n", 
           name, dir->name ? dir->name : "(null)");
    
    list_t list = dir->data;
    if (!list) {
        printf("find_in_dir: Directory has no children (empty list)\n");
        return NULL;
    }
    
    int count = 0;
    list_foreach(list, node) {
        count++;
        if (!node) {
            printf("find_in_dir: Found NULL node in list\n");
            continue;
        }
        
        memfs_file_t *file = (memfs_file_t *)node->data;
        if (!file) {
            printf("find_in_dir: Found node with NULL data\n");
            continue;
        }
        
        if (!file->name) {
            printf("find_in_dir: Found file with NULL name\n");
            continue;
        }
        
        printf("find_in_dir: Checking file '%s'\n", file->name);
        if (strcmp(file->name, name) == 0) {
            printf("find_in_dir: Found matching file '%s'\n", name);
            return file;
        }
    }
    
    printf("find_in_dir: Searched %d files, '%s' not found\n", count, name);
    return NULL;
}

// VFS mount callback
static int memfs_mount(cstr src, vfs_node_t node) {
    printf("memfs_mount called with src: %s, node: %p\n", src ? src : "(null)", node);
    
    // Create root directory if it doesn't exist
    if (!memfs_root) {
        printf("Creating memfs root directory\n");
        memfs_root = memfs_create_file("/", file_dir);
        if (!memfs_root) {
            printf("Failed to create memfs root directory\n");
            return -1;
        }
        printf("memfs root directory created successfully\n");
    }
    
    // Setup the node
    node->handle = memfs_root;
    node->type = file_dir;
    node->size = 0;
    node->createtime = node->readtime = node->writetime = time(NULL);
    node->permissions = 0755;  // rwxr-xr-x
    
    printf("memfs_mount completed successfully, node->handle: %p\n", node->handle);
    return 0;
}

// VFS unmount callback
static void memfs_unmount(void *root) {
    memfs_file_t *file = (memfs_file_t *)root;
    
    // Free all files recursively
    if (file && file->type == file_dir && file->data) {
        list_t list = file->data;
        if (list) {
            list_foreach(list, node) {
                if (node && node->data) {
                    memfs_file_t *child = (memfs_file_t *)node->data;
                    if (child->type == file_dir && child->data) {
                        memfs_unmount(child);
                    }
                    memfs_free_file(child);
                }
            }
            list_free(list);
        }
    }
    
    // We don't free memfs_root here because we might reuse it
}

// VFS open callback
static void memfs_open(void *parent, cstr name, vfs_node_t node) {
    printf("memfs_open called with parent: %p, name: %s, node: %p\n", 
           parent, name ? name : "(null)", node);
    
    memfs_file_t *dir = (memfs_file_t *)parent;
    if (!dir || dir->type != file_dir) {
        printf("Invalid parent directory\n");
        return;
    }
    
    printf("Parent directory is valid, type: %d, name: %s\n", 
           dir->type, dir->name ? dir->name : "(null)");
    
    if (dir->data == NULL) {
        printf("Warning: Parent directory has NULL data field (no children)\n");
    }
    
    memfs_file_t *file = memfs_find_in_dir(dir, name);
    if (!file) {
        printf("File not found: %s\n", name);
        return;
    }
    
    printf("Found file: %s (type: %d, size: %zu)\n", name, file->type, file->size);
    
    // Set the node's handle to point to our file structure
    node->handle = file;
    node->type = file->type;
    node->size = file->size;
    node->readtime = time(NULL);
    
    printf("memfs_open set node->handle to %p\n", node->handle);
    printf("memfs_open completed successfully\n");
}

// VFS close callback
static void memfs_close(void *current) {
    // We don't actually close or free the file here
    // We just return since the file data will remain in memory
    printf("memfs_close called with handle: %p\n", current);
}

// VFS read callback
static ssize_t memfs_read(void *file, void *addr, size_t offset, size_t size) {
    printf("memfs_read called with handle: %p, addr: %p, offset: %zu, size: %zu\n", 
           file, addr, offset, size);
    
    memfs_file_t *f = (memfs_file_t *)file;
    if (!f) {
        printf("Error: memfs_read received NULL file handle\n");
        return -1;
    }
    
    if (f->type == file_dir) {
        printf("Error: memfs_read attempted on directory\n");
        return -1;
    }
    
    // Check if read is beyond file size
    if (offset >= f->size) return 0;
    
    // Adjust size if needed
    if (offset + size > f->size) {
        size = f->size - offset;
    }
    
    // Copy data to buffer
    memcpy(addr, (char *)f->data + offset, size);
    
    printf("memfs_read completed successfully, returning %zu\n", size);
    return (ssize_t)size;  // Explicit cast to ensure proper return type
}

// VFS write callback
static ssize_t memfs_write(void *file, const void *addr, size_t offset, size_t size) {
    printf("memfs_write called with file: %p, addr: %p, offset: %zu, size: %zu\n", 
           file, addr, offset, size);
    
    memfs_file_t *f = (memfs_file_t *)file;
    if (!f) {
        printf("Error: memfs_write received NULL file handle\n");
        return -1;
    }
    
    if (f->type == file_dir) {
        printf("Error: memfs_write attempted on directory\n");
        return -1;
    }
    
    printf("Writing to file '%s', current size: %zu, allocated: %zu\n", 
           f->name, f->size, f->allocated);
    
    // Check if we need to extend the file
    if (offset + size > f->allocated) {
        printf("Extending file from %zu to ", f->allocated);
        // Calculate new size (round up to nearest PAGE_SIZE)
        size_t new_size = ((offset + size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
        printf("%zu bytes\n", new_size);
        
        void *new_data = realloc(f->data, new_size);
        if (!new_data) {
            printf("Error: realloc failed when extending file\n");
            return -1;
        }
        
        // Zero the new memory
        memset((char *)new_data + f->allocated, 0, new_size - f->allocated);
        
        f->data = new_data;
        f->allocated = new_size;
    }
    
    // Copy data from buffer
    printf("Copying %zu bytes to offset %zu\n", size, offset);
    memcpy((char *)f->data + offset, addr, size);
    
    // Update file size if needed
    if (offset + size > f->size) {
        f->size = offset + size;
        printf("Updated file size to %zu\n", f->size);
    }
    
    printf("memfs_write completed successfully, returning %zu\n", size);
    return (ssize_t)size;  // Explicit cast to ensure proper return type
}

// VFS mkdir callback
static int memfs_mkdir(void *parent, cstr name, vfs_node_t node) {
    printf("memfs_mkdir called with parent: %p, name: %s, node: %p\n", 
           parent, name ? name : "(null)", node);
    
    memfs_file_t *dir = (memfs_file_t *)parent;
    if (!dir || dir->type != file_dir) {
        printf("Invalid parent directory\n");
        return -1;
    }
    
    printf("Parent directory is valid, type: %d, name: %s\n", 
           dir->type, dir->name ? dir->name : "(null)");
    
    // Check if directory already exists
    if (memfs_find_in_dir(dir, name)) {
        printf("Directory already exists: %s\n", name);
        return -1;
    }
    
    // Create new directory
    printf("Creating new directory: %s\n", name);
    memfs_file_t *new_dir = memfs_create_file(name, file_dir);
    if (!new_dir) {
        printf("Failed to create directory: %s\n", name);
        return -1;
    }
    
    // Initialize directory list if needed
    if (dir->data == NULL) {
        printf("Initializing empty list for parent directory\n");
    }
    
    // Add to parent directory
    printf("Adding directory '%s' to parent '%s'\n", new_dir->name, dir->name);
    dir->data = list_append(dir->data, new_dir);
    printf("Directory added to parent list, dir->data: %p\n", dir->data);
    
    // Setup node
    node->handle = new_dir;
    node->type = file_dir;
    node->size = 0;
    node->createtime = node->readtime = node->writetime = time(NULL);
    node->permissions = 0755;  // rwxr-xr-x
    printf("Directory node setup complete, handle: %p\n", node->handle);
    
    return 0;
}

// VFS mkfile callback
static int memfs_mkfile(void *parent, cstr name, vfs_node_t node) {
    memfs_file_t *dir = (memfs_file_t *)parent;
    if (!dir || dir->type != file_dir) return -1;
    
    // Check if file already exists
    if (memfs_find_in_dir(dir, name)) return -1;
    
    // Create new file
    memfs_file_t *new_file = memfs_create_file(name, file_block);
    if (!new_file) return -1;
    
    // Add to parent directory
    printf("Adding file '%s' to parent '%s'\n", new_file->name, dir->name);
    // Make sure we initialize the list if it's NULL
    dir->data = list_append(dir->data, new_file);
    printf("File added to parent list\n");
    
    // Setup node
    node->handle = new_file;
    node->type = file_block;
    node->size = 0;
    node->createtime = node->readtime = node->writetime = time(NULL);
    node->permissions = 0644;  // rw-r--r--
    printf("File node setup complete, handle: %p\n", node->handle);
    
    return 0;
}

// VFS stat callback
static int memfs_stat(void *file, vfs_node_t node) {
    memfs_file_t *f = (memfs_file_t *)file;
    if (!f) return -1;
    
    node->size = f->size;
    node->type = f->type;
    
    return 0;
}

// MemFS callback structure
static struct vfs_callback memfs_callbacks = {
    .mount = memfs_mount,
    .unmount = memfs_unmount,
    .open = memfs_open,
    .close = memfs_close,
    .read = memfs_read,
    .write = memfs_write,
    .mkdir = memfs_mkdir,
    .mkfile = memfs_mkfile,
    .stat = memfs_stat
};

// Function to test the memfs
int test_memfs() {
    printf("Starting memfs test...\n");
    
    // Initialize the VFS
    printf("Initializing VFS...\n");
    if (!vfs_init()) {
        printf("Failed to initialize VFS\n");
        return -1;
    }
    printf("VFS initialized successfully\n");
    
    // Register the memfs
    printf("Registering memfs...\n");
    int fs_id = vfs_regist("memfs", &memfs_callbacks);
    if (fs_id < 0) {
        printf("Failed to register memfs\n");
        return -1;
    }
    
    printf("MemFS registered with ID: %d\n", fs_id);
    
    // Mount memfs on root
    printf("Mounting memfs to rootdir...\n");
    int mount_result = vfs_mount("memfs", rootdir);
    printf("vfs_mount result: %d, rootdir->handle: %p\n", mount_result, rootdir->handle);
    if (mount_result != 0) {
        printf("Failed to mount memfs\n");
        return -1;
    }
    
    // Create a test directory
    printf("Creating directory: /test\n");
    int result = vfs_mkdir("/test");
    printf("vfs_mkdir result: %d\n", result);
    if (result != 0) {
        printf("Failed to create directory: /test\n");
        return -1;
    }
    printf("Directory created successfully\n");
    
    // Create a test file
    printf("Creating file: /test/hello.txt\n");
    result = vfs_mkfile("/test/hello.txt");
    printf("vfs_mkfile result: %d\n", result);
    if (result != 0) {
        printf("Failed to create file: /test/hello.txt\n");
        return -1;
    }
    printf("File created successfully\n");
    
    // Dump root directory information
    printf("Root directory information:\n");
    printf("  - type: %d\n", rootdir->type);
    printf("  - fsid: %d\n", rootdir->fsid);
    printf("  - handle: %p\n", rootdir->handle);
    if (rootdir->handle) {
        memfs_file_t *root_file = (memfs_file_t *)rootdir->handle;
        printf("  - memfs_file name: %s\n", root_file->name ? root_file->name : "(null)");
        printf("  - memfs_file type: %d\n", root_file->type);
        printf("  - memfs_file data: %p\n", root_file->data);
    }
    
    // Open test directory
    printf("Opening directory: /test\n");
    vfs_node_t test_dir = vfs_open("/test");
    if (!test_dir) {
        printf("Failed to open directory: /test\n");
        return -1;
    }
    printf("Test directory opened successfully, handle: %p\n", test_dir->handle);
    
    // Open the file
    printf("Opening file: /test/hello.txt\n");
    vfs_node_t file = vfs_open("/test/hello.txt");
    if (!file) {
        printf("Failed to open file: /test/hello.txt\n");
        return -1;
    }
    printf("File opened successfully, file->handle: %p\n", file->handle);
    
    // Force an update to ensure the handle is initialized
    vfs_update(file);
    printf("File updated, file->handle: %p\n", file->handle);
    
    // Write to the file
    const char *data = "Hello, MemFS!";
    size_t data_len = strlen(data);
    printf("Writing to file: \"%s\" (%zu bytes)\n", data, data_len);
    
    // Explicitly check file handle before writing
    if (!file) {
        printf("Error: Invalid file node before writing\n");
        return -1;
    }
    
    if (!file->handle) {
        printf("Error: Invalid file handle before writing, updating node\n");
        vfs_update(file);
        if (!file->handle) {
            printf("Error: Still no valid handle after update\n");
            return -1;
        }
    }
    
    printf("Writing to file with handle: %p\n", file->handle);
    
    ssize_t written = vfs_write(file, data, 0, data_len);
    printf("vfs_write result: %zd bytes\n", written);
    
    if (written < 0) {
        printf("Error: vfs_write returned negative value\n");
        vfs_close(file);
        return -1;
    }
    
    if ((size_t)written != data_len) {
        printf("Failed to write to file: %zd bytes written (expected %zu)\n", 
               written, data_len);
        vfs_close(file);
        return -1;
    }
    
    printf("Successfully wrote %zd bytes to file\n", written);
    
    // Reopen the file to read
    printf("Closing file\n");
    vfs_close(file);
    printf("Reopening file: /test/hello.txt\n");
    file = vfs_open("/test/hello.txt");
    if (!file) {
        printf("Failed to reopen file: /test/hello.txt\n");
        return -1;
    }
    printf("File reopened successfully\n");
    
    // Read from the file
    char buffer[100];
    memset(buffer, 0, sizeof(buffer));
    printf("Reading from file\n");
    // Explicitly check file handle before reading
    if (!file) {
        printf("Error: Invalid file node before reading\n");
        return -1;
    }
    
    if (!file->handle) {
        printf("Error: Invalid file handle before reading, updating node\n");
        vfs_update(file);
        if (!file->handle) {
            printf("Error: Still no valid handle after update\n");
            return -1;
        }
    }
    
    printf("Reading from file with handle: %p\n", file->handle);
    
    ssize_t bytes_read = vfs_read(file, buffer, 0, sizeof(buffer) - 1);
    printf("vfs_read result: %zd bytes\n", bytes_read);
    
    if (bytes_read < 0) {
        printf("Error: vfs_read returned negative value\n");
        vfs_close(file);
        return -1;
    }
    
    if ((size_t)bytes_read != strlen(data)) {
        printf("Failed to read from file: %zd bytes read (expected %zu)\n", 
               bytes_read, strlen(data));
        vfs_close(file);
        return -1;
    }
    
    printf("Read from file: \"%s\"\n", buffer);
    printf("Verification: %s\n", strcmp(buffer, data) == 0 ? "PASSED" : "FAILED");
    
    // Clean up
    printf("Closing file\n");
    vfs_close(file);
    
    printf("MemFS test completed successfully\n");
    return 0;
}

int main() {
    int result = test_memfs();
    printf("MemFS test %s\n", result == 0 ? "passed" : "failed");
    return result;
}