// This code is released under the MIT License

#include <vfs.h>

vfs_node_t rootdir = null;

static void empty_func() {}

static struct vfs_callback vfs_empty_callback;

static vfs_callback_t fs_callbacks[256] = {
    [0] = &vfs_empty_callback,
};
static int fs_nextid = 1;

#define callbackof(node, _name_) (fs_callbacks[(node)->fsid]->_name_)

static vfs_node_t vfs_node_alloc(vfs_node_t parent, cstr name) {
  vfs_node_t node = malloc(sizeof(struct vfs_node));
  if (node == null) return null;
  memset(node, 0, sizeof(struct vfs_node));
  node->parent = parent;
  node->name   = name ? strdup(name) : null;
  node->type   = file_none;
  node->fsid   = parent ? parent->fsid : 0;
  node->root   = parent ? parent->root : node;
  if (parent) list_prepend(parent->child, node);
  return node;
}

static void vfs_free(vfs_node_t vfs) {
  if (vfs == null) return;
  list_free_with(vfs->child, (free_t)vfs_free);
  vfs_close(vfs);
  free(vfs->name);
  free(vfs);
}

static void vfs_free_child(vfs_node_t vfs) {
  if (vfs == null) return;
  list_free_with(vfs->child, (free_t)vfs_free);
}

// 从字符串中提取路径
finline char *pathtok(char **_rest sp) {
  char *s = *sp, *e = *sp;
  if (*s == '\0') return null;
  for (; *e != '\0' && *e != '/'; e++) {}
  *sp = e + (*e != '\0' ? 1 : 0);
  *e  = '\0';
  return s;
}

finline __nnull(1) void do_open(vfs_node_t file) {
  if (file->handle != null) {
    callbackof(file, stat)(file->handle, file);
  } else {
    callbackof(file, open)(file->parent->handle, file->name, file);
  }
}

finline void do_update(vfs_node_t file) {
  assert(file != null);
  assert(file->fsid != 0 || file->type != file_none);
  if (file->type == file_none || file->handle == null) do_open(file);
  assert(file->type != file_none);
}

vfs_node_t vfs_child_append(vfs_node_t parent, cstr name, void *handle) {
  vfs_node_t node = vfs_node_alloc(parent, name);
  if (node == null) return null;
  node->handle = handle;
  return node;
}

static vfs_node_t vfs_child_find(vfs_node_t parent, cstr name) {
  return list_first(parent->child, data, streq(name, ((vfs_node_t)data)->name));
}

int vfs_mkdir(cstr name) {
  if (name[0] != '/') return -1;
  char      *path     = strdup(name + 1);
  char      *save_ptr = path;
  vfs_node_t current  = rootdir;
  for (cstr buf = pathtok(&save_ptr); buf; buf = pathtok(&save_ptr)) {
    const vfs_node_t father = current;
    if (streq(buf, ".")) continue;
    if (streq(buf, "..")) {
      if (current->parent && current->type == file_dir) {
        current = current->parent;
        goto upd;
      } else {
        goto err;
      }
    }
    current = vfs_child_find(current, buf);

  upd:
    if (current == null) {
      current       = vfs_node_alloc(father, buf);
      current->type = file_dir;
      callbackof(father, mkdir)(father->handle, buf, current);
    } else {
      do_update(current);
      if (current->type != file_dir) goto err;
    }
  }

  free(path);
  return 0;

err:
  free(path);
  return -1;
}

int vfs_mkfile(cstr name) {
  if (name[0] != '/') return -1;
  char      *path     = strdup(name + 1);
  char      *save_ptr = path;
  vfs_node_t current  = rootdir;
  char      *filename = path + strlen(path);

  while (*--filename != '/' && filename != path) {}
  if (filename != path) {
    *filename++ = '\0';
  } else {
    goto create;
  }

  if (strlen(path) == 0) {
    free(path);
    return -1;
  }
  for (cstr buf = pathtok(&save_ptr); buf; buf = pathtok(&save_ptr)) {
    if (streq(buf, ".")) continue;
    if (streq(buf, "..")) {
      if (!current->parent || current->type != file_dir) goto err;
      current = current->parent;
      continue;
    }
    current = vfs_child_find(current, buf);
    if (current == null || current->type != file_dir) goto err;
  }

create:
  vfs_node_t node = vfs_child_append(current, filename, null);
  node->type      = file_block;
  callbackof(current, mkfile)(current->handle, filename, node);

  free(path);
  return 0;

err:
  free(path);
  return -1;
}

int vfs_regist(cstr name, vfs_callback_t callback) {
  if (callback == null) return -1;
  for (size_t i = 0; i < sizeof(struct vfs_callback) / sizeof(void *); i++) {
    if (((void **)callback)[i] == null) return -1;
  }
  int id           = fs_nextid++;
  fs_callbacks[id] = callback;
  return id;
}

static vfs_node_t vfs_do_search(vfs_node_t dir, cstr name) {
  return list_first(dir->child, data, streq(name, ((vfs_node_t)data)->name));
}

vfs_node_t vfs_open(cstr _path) {
  if (_path == null) return null;
  if (_path[1] == '\0') return rootdir;
  char *path = strdup(_path + 1);
  if (path == null) return null;

  char      *save_ptr = path;
  vfs_node_t current  = rootdir;
  for (const char *buf = pathtok(&save_ptr); buf; buf = pathtok(&save_ptr)) {
    if (streq(buf, ".")) continue;
    if (streq(buf, "..")) {
      if (current->parent == null) goto err;
      assert(current->parent->type == file_dir, "parent is not dir");
      current = current->parent;
      do_update(current);
      continue;
    }
    current = vfs_child_find(current, buf);
    if (current == null) goto err;
    do_update(current);
  }

  free(path);
  return current;

err:
  free(path);
  return null;
}

void vfs_update(vfs_node_t node) {
  do_update(node);
}

bool vfs_init() {
  for (size_t i = 0; i < sizeof(struct vfs_callback) / sizeof(void *); i++) {
    ((void **)&vfs_empty_callback)[i] = &empty_func;
  }

  rootdir       = vfs_node_alloc(null, null);
  rootdir->type = file_dir;
  return true;
}

int vfs_close(vfs_node_t node) {
  if (node == null) return -1;
  if (node->handle == null) return 0;
  callbackof(node, close)(node->handle);
  node->handle = null;
  return 0;
}

int vfs_mount(cstr src, vfs_node_t node) {
  if (node == null) return -1;
  if (node->type != file_dir) return -1;
  for (int i = 1; i < fs_nextid; i++) {
    if (fs_callbacks[i]->mount(src, node) == 0) {
      node->fsid = i;
      node->root = node;
      return 0;
    }
  }
  return -1;
}

ssize_t vfs_read(vfs_node_t file, void *addr, size_t offset, size_t size) {
  assert(file != null);
  assert(addr != null);
  do_update(file);
  if (file->type == file_dir) return -1;
  return callbackof(file, read)(file->handle, addr, offset, size);
}

ssize_t vfs_write(vfs_node_t file, const void *addr, size_t offset, size_t size) {
  assert(file != null);
  assert(addr != null);
  do_update(file);
  if (file->type == file_dir) return -1;
  ssize_t write_bytes = callbackof(file, write)(file->handle, addr, offset, size);
  if (write_bytes > 0) { file->size = max(file->size, offset + write_bytes); }
  return write_bytes;
}

int vfs_unmount(cstr path) {
  vfs_node_t node = vfs_open(path);
  if (node == null) return -1;
  if (node->type != file_dir) return -1;
  if (node->fsid == 0) return -1;
  if (node->parent) {
    vfs_node_t cur = node;
    node           = node->parent;
    if (cur->root == cur) {
      vfs_free_child(cur);
      callbackof(cur, unmount)(cur->handle);
      cur->fsid   = node->fsid; // 交给上级
      cur->root   = node->root;
      cur->handle = null;
      cur->child  = null;
      // cur->type   = file_none;
      if (cur->fsid) do_update(cur);
      return 0;
    }
  }
  return -1;
}

// 使用请记得free掉返回的buff
char *vfs_get_fullpath(vfs_node_t node) {
  if (node == null) return null;
  int         inital = 16;
  vfs_node_t *nodes  = (vfs_node_t *)malloc(sizeof(vfs_node_t) * inital);
  int         count  = 0;
  for (vfs_node_t cur = node; cur; cur = cur->parent) {
    if (count >= inital) {
      inital *= 2;
      nodes   = (vfs_node_t *)realloc((void *)nodes, sizeof(vfs_node_t) * inital);
      assert(nodes);
    }
    nodes[count++] = cur;
  }
  // 正常的路径都不应该超过这个数值
  char *buff = (char *)malloc(PATH_MAX);
  strcpy(buff, "/");
  for (int j = count - 1; j >= 0; j--) {
    if (nodes[j] == rootdir) continue;
    strcat(buff, nodes[j]->name);
    if (j != 0) strcat(buff, "/");
  }
  free(nodes);
  return buff;
}
