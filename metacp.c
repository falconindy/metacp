#define _GNU_SOURCE

#include <attr/libattr.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "metacp.h"

static int copy_permissions(properties_t propmask, const struct file_t *source,
    const struct file_t *dest) {
  uid_t uid;
  gid_t gid;

  uid = (propmask & PROPERTY_UID ? source : dest)->st.st_uid;
  gid = (propmask & PROPERTY_UID ? source : dest)->st.st_gid;

  return fchown(dest->fd, uid, gid);
}

static int copy_filetimes(properties_t propmask, const struct file_t *source,
    const struct file_t *dest) {
  struct timespec times[2] = {
    (propmask & PROPERTY_ATIME ? source : dest)->st.st_atim,
    (propmask & PROPERTY_MTIME ? source : dest)->st.st_mtim,
  };

  return futimens(dest->fd, times);
}

static int copy_mode(_unused_ properties_t propmask,
    const struct file_t *source, const struct file_t *dest) {
  return fchmod(dest->fd, source->st.st_mode);
}

static int copy_acl(_unused_ properties_t propmask,
    const struct file_t *source, const struct file_t *dest) {
  _cleanup_acl_ acl_t source_acl;

  source_acl = acl_get_fd(source->fd);
  if (source_acl == NULL)
    /* silently pass on filesystems which do not support ACLs */
    return errno == ENOTSUP ? 0 : -errno;

  /* TODO: should we ignore ENOTSUP here, or warn? */
  if (acl_set_fd(dest->fd, source_acl))
    return -errno;

  return 0;
}

static int copy_capabilities(_unused_ properties_t propmask,
    const struct file_t *source, const struct file_t *dest) {
  _cleanup_cap_ cap_t source_caps;

  /* capabilities on anything other than a regular file is undefined. */
  if (!S_ISREG(source->st.st_mode) || !S_ISREG(dest->st.st_mode))
    return 0;

  source_caps = cap_get_fd(source->fd);
  if (source_caps == NULL)
    /* ENODATA means there's no caps to copy */
    return errno == ENODATA ? 0 : -errno;

  if (cap_set_fd(dest->fd, source_caps) < 0)
    return -errno;

  return 0;
}

static int copy_xattrs(_unused_ properties_t propmask,
    const struct file_t *source, const struct file_t *dest) {

  if (attr_copy_fd(source->path, source->fd, dest->path, dest->fd,
        NULL, NULL) < 0)
    return -errno;

  return 0;
}

static int file_open(struct file_t *file, const char *path, int flags) {
  file->fd = open(path, flags | O_NOFOLLOW);
  if (file->fd < 0)
    return -errno;

  if (fstat(file->fd, &file->st) < 0)
    return -errno;

  file->path = path;

  return 0;
}

static int copy_properties_by_fileobj(properties_t propmask,
    struct file_t *source, struct file_t *dest) {
  static const struct copier {
    copy_property copy_fn;
    properties_t propmask;
    const char *desc;
  } copiers[] = {
    { copy_permissions, PROPERTY_UID|PROPERTY_GID, "permissions" },
    { copy_filetimes , PROPERTY_MTIME|PROPERTY_ATIME, "filetimes" },
    { copy_mode, PROPERTY_MODE, "mode"},
    { copy_acl, PROPERTY_ACL, "acl" },
    { copy_capabilities, PROPERTY_CAPABILITIES, "capabilities" },
    { copy_xattrs, PROPERTY_XATTRS, "xattrs" },
    { NULL, 0, NULL },
  };
  int r = 0;

  for (const struct copier *copier = copiers; copier->copy_fn; ++copier) {
    int k;

    if (!(propmask & copier->propmask))
      continue;

    k = copier->copy_fn(propmask, source, dest);
    if (k < 0) {
      fprintf(stderr, "error: failed to copy %s to destination: %s\n",
          copier->desc, strerror(-k));
      if (r == 0)
        r = k;
    }
  }

  return r;
}

static int copy_properties_by_path(properties_t propmask, const char *in,
    const char *out) {
  _cleanup_file_ struct file_t source = { .fd = -1 }, dest = { .fd = -1 };
  int r;

  r = file_open(&source, in, O_RDONLY);
  if (r < 0) {
    fprintf(stderr, "error: failed to open source file %s: %s\n",
        in, strerror(-r));
    return -errno;
  }

  r = file_open(&dest, out, O_WRONLY|O_APPEND);
  if (dest.fd < 0) {
    fprintf(stderr, "error: failed to open destination file %s: %s\n",
        out, strerror(-r));
    return -errno;
  }

  return copy_properties_by_fileobj(propmask, &source, &dest);
}

static int usage(FILE *stream) {
  fprintf(stream,
      "metacp v0\n"
      "Usage: %s <source> <dest>\n",
      program_invocation_short_name);

  return stream == stderr;
}

int main(int argc, char *argv[]) {
  if (argc < 3)
    exit(usage(stderr));

  return !!copy_properties_by_path(PROPERTY_ALL, argv[1], argv[2]);
}
