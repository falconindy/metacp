# metacp

metacp copies file metadata without copying file contents.

Currently, it understands:

* ownership (uid and gid)
* file times (mtime and atime)
* file mode
* ACLs (with filesystem support)
* xattrs (with filesystem support)
* capabilities (with kernel support)

## TODO

* add recursive copies (will likely need an --ignore-missing flag, or so)
* fine grained control over what gets copied (--preserve=acl,nomode,mtime etc)
* add a verbose mode
* allow compilation without xattr support
* implement xattr support on raw syscalls
* implement (de)serialization formats (e.g. for transfer across hosts)
* write a manpage
