// This code is released under the MIT License

#include <vfs.h>

vfs_node_t rootdir = null;

static void empty_func() {}

static struct vfs_callback vfs_empty_callback;

static vfs_callback_t fs_callbacks[256] = {
    [0] = &vfs_empty_callback,
};
static int fs_nextid = 1;

#define callbackof(node, _name_) (fs_callbacks[(node)->info->fsid]->_name_)

static vfs_node_t vfs_node_alloc(vfs_node_t parent, cstr name) {
  vfs_node_t node = malloc(sizeof(struct vfs_node));
  if (node == null)
    return null;
  memset(node, 0, sizeof(struct vfs_node));
  node->parent = parent;
  node->name = name ? strdup(name) : null;
  node->symlink_path = null;
  node->info = malloc(sizeof(*(node->info)));
  memset(node->info, 0, sizeof(*(node->info)));
  node->info->type = file_none;
  node->info->fsid = parent ? parent->info->fsid : 0;
  node->info->root = parent ? parent->info->root : node;
  if (node->info->type == file_dir) {
    list_prepend(node->info->childs, &(node->child));
  }
  if (parent == null)
    return node;
  list_foreach(parent->info->childs, data) {
    vfs_node_t child = (vfs_node_t)data->data;
    list_prepend(child->child, node);
  }
  return node;
}

static void _vfs_free(vfs_node_t vfs) {
  if (vfs == null)
    return;
  list_free_with(vfs->child, (free_t)_vfs_free);
  vfs_close(vfs);
  free(vfs->name);
  free(vfs);
}
static void vfs_free(vfs_node_t vfs);
static void vfs_free_with_lists(list_t list) {
  list_foreach(list, data) {
    list_t *vfs = (list_t *)data->data;
    list_free_with(*vfs, (free_t)vfs_free);
  }
  list_free(list);
}

static void vfs_free(vfs_node_t vfs) {
  if (vfs == null)
    return;
  if (vfs->symlink_path == null) {
    if (vfs->info->type == file_dir)
      vfs_free_with_lists(vfs->info->childs); // 此时,vfs->child就已经被释放了
    vfs_close(vfs);
    free(vfs->name);
    free(vfs->info);
    free(vfs);
  } else {
    if (vfs_open(vfs->symlink_path) == null)
      return; // 说明info肯定不合法
    if (vfs->info->type == file_dir) {
      list_delete(vfs->info->childs, &(vfs->child));
      list_free_with(vfs->child, (free_t)_vfs_free);
    }
    vfs_close(vfs);
    free(vfs->name);
    free(vfs);
  }
}
static void vfs_free_child(vfs_node_t vfs) {
  if (vfs == null)
    return;
  list_free_with(vfs->child, (free_t)vfs_free);
}

// 从字符串中提取路径
finline char *pathtok(char **_rest sp) {
  char *s = *sp, *e = *sp;
  if (*s == '\0')
    return null;
  for (; *e != '\0' && *e != '/'; e++) {
  }
  *sp = e + (*e != '\0' ? 1 : 0);
  *e = '\0';
  return s;
}

finline __nnull(1) void do_open(vfs_node_t file) {
  if (file->info->handle != null) {
    callbackof(file, stat)(file->info->handle, file);
  } else {
    callbackof(file, open)(file->parent->info->handle, file->name, file);
  }
}

finline void do_update(vfs_node_t file) {
  assert(file != null);
  assert(file->info->fsid != 0 || file->info->type != file_none);
  if (file->info->type == file_none || file->info->handle == null ||
      file->info->type == file_dir)
    do_open(file);
  assert(file->info->type != file_none);
}

vfs_node_t vfs_child_append(vfs_node_t parent, cstr name, void *handle) {
  vfs_node_t node = vfs_node_alloc(parent, name);
  if (node == null)
    return null;
  node->info->handle = handle;
  return node;
}

static vfs_node_t vfs_child_find(vfs_node_t parent, cstr name) {
  return list_first(parent->child, data, streq(name, ((vfs_node_t)data)->name));
}

int vfs_mkdir(cstr name) {
  if (name[0] != '/')
    return -1;
  char *path = strdup(name + 1);
  char *save_ptr = path;
  vfs_node_t current = rootdir;
  for (cstr buf = pathtok(&save_ptr); buf; buf = pathtok(&save_ptr)) {
    const vfs_node_t father = current;
    if (streq(buf, "."))
      continue;
    if (streq(buf, "..")) {
      if (current->parent && current->info->type == file_dir) {
        current = current->parent;
        goto upd;
      } else {
        goto err;
      }
    }
    current = vfs_child_find(current, buf);

  upd:
    if (current == null) {
      // TODO: childs
      current = vfs_node_alloc(father, buf);
      current->info->type = file_dir;
      callbackof(father, mkdir)(father->info->handle, buf, current);
    } else {
      do_update(current);
      if (current->info->type != file_dir)
        goto err;
    }
  }

  free(path);
  return 0;

err:
  free(path);
  return -1;
}

int vfs_mkfile(cstr name) {
  if (name[0] != '/')
    return -1;
  char *path = strdup(name + 1);
  char *save_ptr = path;
  vfs_node_t current = rootdir;
  char *filename = path + strlen(path);

  while (*--filename != '/' && filename != path) {
  }
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
    if (streq(buf, "."))
      continue;
    if (streq(buf, "..")) {
      if (!current->parent || current->info->type != file_dir)
        goto err;
      current = current->parent;
      continue;
    }
    current = vfs_child_find(current, buf);
    if (current == null || current->info->type != file_dir)
      goto err;
  }

create:
  vfs_node_t node = vfs_child_append(current, filename, null);
  node->info->type = file_block;
  callbackof(current, mkfile)(current->info->handle, filename, node);

  free(path);
  return 0;

err:
  free(path);
  return -1;
}

int vfs_regist(cstr name, vfs_callback_t callback) {
  if (callback == null)
    return -1;
  for (size_t i = 0; i < sizeof(struct vfs_callback) / sizeof(void *); i++) {
    if (((void **)callback)[i] == null)
      return -1;
  }
  int id = fs_nextid++;
  fs_callbacks[id] = callback;
  return id;
}

static vfs_node_t vfs_do_search(vfs_node_t dir, cstr name) {
  return list_first(dir->child, data, streq(name, ((vfs_node_t)data)->name));
}

static vfs_node_t __vfs_open(cstr _path) {
  if (_path == null)
    return null;
  if (_path[1] == '\0')
    return rootdir;
  char *path = strdup(_path + 1);
  if (path == null)
    return null;

  char *save_ptr = path;
  vfs_node_t current = rootdir;
  for (const char *buf = pathtok(&save_ptr); buf; buf = pathtok(&save_ptr)) {
    if (streq(buf, "."))
      continue;
    if (streq(buf, "..")) {
      if (current->parent == null)
        goto err;
      assert(current->parent->info->type == file_dir, "parent is not dir");
      current = current->parent;
      do_update(current);
      continue;
    }
    current = vfs_child_find(current, buf);
    if (current == null)
      goto err;
    do_update(current);
  }

  free(path);
  return current;

err:
  free(path);
  return null;
}

static vfs_node_t _vfs_open(cstr path) {
  vfs_node_t r = __vfs_open(path);
  // 接下来是检查环节
  if (r->symlink_path == null)
    return r;
  // 如果是软链接，则需要重新打开
  char *sympath = r->symlink_path;
  // 如此当源文件被删除时就会返回null
  while (sympath) {
    vfs_node_t sym = _vfs_open(sympath);
    if (sym == null)
      return null;
    // 继续打开下一个软链接
    sympath = sym->symlink_path;
  }
  return r;
}
#define CHECK_AND_UPDATE                                                       \
  do {                                                                         \
    if (current->symlink_path != null) {                                       \
      if (_vfs_open(current->symlink_path) == null) {                          \
        goto err;                                                              \
      }                                                                        \
    }                                                                          \
    do_update(current);                                                        \
  } while (0)
vfs_node_t vfs_open(cstr _path) {
  if (_path == null)
    return null;
  if (_path[1] == '\0')
    return rootdir;
  char *path = strdup(_path + 1);
  if (path == null)
    return null;

  char *save_ptr = path;
  vfs_node_t current = rootdir;
  for (const char *buf = pathtok(&save_ptr); buf; buf = pathtok(&save_ptr)) {

    if (streq(buf, "."))
      continue;
    if (streq(buf, "..")) {
      if (current->parent == null)
        goto err;
      assert(current->parent->info->type == file_dir, "parent is not dir");
      current = current->parent;
      CHECK_AND_UPDATE;
      continue;
    }
    current = vfs_child_find(current, buf);
    if (current == null)
      goto err;
    CHECK_AND_UPDATE;
  }

  free(path);
  return current;

err:
  free(path);
  return null;
}
void vfs_update(vfs_node_t node) { do_update(node); }

bool vfs_init() {
  for (size_t i = 0; i < sizeof(struct vfs_callback) / sizeof(void *); i++) {
    ((void **)&vfs_empty_callback)[i] = &empty_func;
  }

  rootdir = vfs_node_alloc(null, null);
  rootdir->info->type = file_dir;
  return true;
}

int vfs_close(vfs_node_t node) {
  if (node == null)
    return -1;
  if (node->info->handle == null)
    return 0;
  callbackof(node, close)(node->info->handle);
  node->info->handle = null;
  return 0;
}

int vfs_mount(cstr src, vfs_node_t node) {
  if (node == null)
    return -1;
  if (node->info->type != file_dir)
    return -1;
  for (int i = 1; i < fs_nextid; i++) {
    if (fs_callbacks[i]->mount(src, node) == 0) {
      node->info->fsid = i;
      node->info->root = node;
      return 0;
    }
  }
  return -1;
}

ssize_t vfs_read(vfs_node_t file, void *addr, size_t offset, size_t size) {
  assert(file != null);
  assert(addr != null);
  do_update(file);
  if (file->info->type == file_dir)
    return -1;
  return callbackof(file, read)(file->info->handle, addr, offset, size);
}

ssize_t vfs_write(vfs_node_t file, const void *addr, size_t offset,
                  size_t size) {
  assert(file != null);
  assert(addr != null);
  do_update(file);
  if (file->info->type == file_dir)
    return -1;
  ssize_t write_bytes =
      callbackof(file, write)(file->info->handle, addr, offset, size);
  if (write_bytes > 0) {
    file->info->size = max(file->info->size, offset + write_bytes);
  }
  return write_bytes;
}

int vfs_unmount(cstr path) {
  vfs_node_t node = vfs_open(path);
  if (node == null)
    return -1;
  if (node->info->type != file_dir)
    return -1;
  if (node->info->fsid == 0)
    return -1;
  if (node->parent) {
    vfs_node_t cur = node;
    node = node->parent;
    if (cur->info->root == cur) {
      vfs_free_child(cur);
      callbackof(cur, unmount)(cur->info->handle);
      cur->info->fsid = node->info->fsid; // 交给上级
      cur->info->root = node->info->root;
      cur->info->handle = null;
      cur->child = null;
      // cur->info->type   = file_none;
      if (cur->info->fsid)
        do_update(cur);
      return 0;
    }
  }
  return -1;
}

// 使用请记得free掉返回的buff
char *vfs_get_fullpath(vfs_node_t node) {
  if (node == null)
    return null;
  int inital = 16;
  vfs_node_t *nodes = (vfs_node_t *)malloc(sizeof(vfs_node_t) * inital);
  int count = 0;
  for (vfs_node_t cur = node; cur; cur = cur->parent) {
    if (count >= inital) {
      inital *= 2;
      nodes = (vfs_node_t *)realloc((void *)nodes, sizeof(vfs_node_t) * inital);
      assert(nodes);
    }
    nodes[count++] = cur;
  }
  // 正常的路径都不应该超过这个数值
  char *buff = (char *)malloc(PATH_MAX);
  strcpy(buff, "/");
  for (int j = count - 1; j >= 0; j--) {
    if (nodes[j] == rootdir)
      continue;
    strcat(buff, nodes[j]->name);
    if (j != 0)
      strcat(buff, "/");
  }
  free(nodes);
  return buff;
}
