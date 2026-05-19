#pragma once

/* /mod — synthetic FS exposing every Multiboot module as a readable file.
   /mod/0 is the first module (typically the -initrd binary).  Reads return
   the raw file bytes; vnode->size reports the module's length. */
void modfs_init(void);
