#ifndef _METACP_H_
#define _METACP_H_

#include <stdlib.h>
#include <sys/acl.h>
#include <sys/capability.h>
#include <sys/stat.h>
#include <unistd.h>

typedef enum {
  PROPERTY_UID = 0x1 << 0,
  PROPERTY_GID = 0x1 << 1,
  PROPERTY_ATIME = 0x1 << 2,
  PROPERTY_MTIME = 0x1 << 3,
  PROPERTY_MODE = 0x1 << 4,
  PROPERTY_ACL = 0x1 << 5,
  PROPERTY_CAPABILITIES = 0x1 << 6,
  PROPERTY_XATTRS = 0x1 << 7,

  PROPERTY_ALL = (0x1 << 8) - 1,
  /* sentinel */
  _PROPERTY_INVALID = -1,
} properties_t;

struct file_t {
  int fd;
  const char *path;
  struct stat st;
};

typedef int (*copy_property)(properties_t propmask, const struct file_t *source,
    const struct file_t *dest);

#define _noreturn_ __attribute__((noreturn))
#define _unused_ __attribute__((unused))
#define _cleanup_(x) __attribute__((cleanup(x)))

static inline void file_close(struct file_t *file) {
  if (file->fd >= 0)
    close(file->fd);
}
#define _cleanup_file_ _cleanup_(file_close)

static inline void cap_freep(cap_t *cap) {
  cap_free(*cap);
}
#define _cleanup_cap_ _cleanup_(cap_freep)

static inline void acl_freep(acl_t *acl) {
  acl_free(*acl);
}
#define _cleanup_acl_ _cleanup_(acl_freep)

static inline void freep(void *p) {
  free(*(void**)p);
}
#define _cleanup_free_ _cleanup_(freep)

#endif /* _METACP_H_ */
