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
  _cleanup_acl_ acl_t acl;

  acl = acl_get_fd(source->fd);
  if (acl == NULL)
    return errno == ENOTSUP ? 0 : -errno;

  if (acl_set_fd(dest->fd, acl) < 0)
    if (errno == ENOTSUP)
      fprintf(stderr, "warning: unable to preserve ACL on %s: %s\n",
          dest->path, strerror(errno));
    return -errno;

  return 0;
}

static int copy_capabilities(_unused_ properties_t propmask,
    const struct file_t *source, const struct file_t *dest) {
  _cleanup_cap_ cap_t caps = NULL;

  caps = cap_get_fd(source->fd);
  if (caps == NULL) {
    /* ENODATA signifies success, but a lack of caps on the file */
    if (errno != ENODATA)
      return -errno;

    caps = cap_init();
    if (caps == NULL)
      return -errno;
  }

  if (cap_set_fd(dest->fd, caps) < 0)
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
    int filetypemask;
    const char *desc;
  } copiers[] = {
    { copy_permissions,  PROPERTY_UID|PROPERTY_GID,     S_IFMT,  "permissions" },
    { copy_filetimes,    PROPERTY_MTIME|PROPERTY_ATIME, S_IFMT,  "filetimes" },
    { copy_mode,         PROPERTY_MODE,                 S_IFMT,  "mode"},
    { copy_acl,          PROPERTY_ACL,                  S_IFMT,  "acl" },
    { copy_capabilities, PROPERTY_CAPABILITIES,         S_IFREG, "capabilities" },
    { copy_xattrs,       PROPERTY_XATTRS,               S_IFMT,  "xattrs" },
    { NULL, 0, 0, NULL },
  };
  int r = 0;

  for (const struct copier *copier = copiers; copier->copy_fn; ++copier) {
    int k;

    if (!(propmask & copier->propmask))
      continue;

    if (!(source->st.st_mode & copier->filetypemask) ||
        !(dest->st.st_mode & copier->filetypemask))
      continue;

    k = copier->copy_fn(propmask, source, dest);
    if (k < 0) {
      fprintf(stderr, "error: failed to copy %s: %s\n",
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
    return r;
  }

  r = file_open(&dest, out, O_WRONLY|O_APPEND);
  if (r < 0) {
    fprintf(stderr, "error: failed to open destination file %s: %s\n",
        out, strerror(-r));
    return r;
  }

  return copy_properties_by_fileobj(propmask, &source, &dest);
}

static int usage(FILE *stream) {
  fprintf(stream,
      "metacp v0\n"
      "Usage: %s <source> <dest>\n\n",
      program_invocation_short_name);

  fputs("  -h, --help              display this help and exit\n",
      stream);

  return stream == stderr;
}

static int arg_parse(int *argc, char **argv[]) {
  int r = 0;
  const char * const shortopts = "h";
  const struct option longopts[] = {
    { "help", no_argument, NULL, 'h' },
  };

  for (;;) {
    int k = getopt_long(*argc, *argv, shortopts, longopts, NULL);
    if (k < 0)
      break;

    switch (k) {
      case 'h':
        exit(usage(stdout));
      default:
        return -EINVAL;
    }
  }

  return r;
}

int main(int argc, char *argv[]) {
  if (arg_parse(&argc, &argv) < 0)
    return 1;

  if (argc < 3) {
    fprintf(stderr, "error: missing source and/or destination\n");
    return 1;
  }

  return !!copy_properties_by_path(PROPERTY_ALL, argv[1], argv[2]);
}
