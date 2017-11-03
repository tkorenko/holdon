### About

**holdon** is an [inotify(7)](http://man7.org/linux/man-pages/man7/inotify.7.html)-based console utility for monitoring file content modifications.

A utility watches a bunch of files, awaits for changes, catches the first
modification, displays the name of the modified file, and exits.  This utility
might come in handy in shell scripts.

### Usage Examples

Lets automate updating of a **_name_.html** file at every change of its
**_name_.md** source.  The following shell script may be used to automatically
rerun [markdown](http://daringfireball.net/projects/markdown/) during editing
session of the source file:

---
    #!/bin/sh
    while true
    do
        markdown README.md > README.htm
        holdon README.md
    done

---

Its also easy to automatically rebuild a project when one of its
files gets changed (kinda "a poor man's IDE-less programming efforts") --
*make* utility handles build process, whereas *holdon* utility postpones
the next run of *make* until changes happen.

---
    #!/bin/sh
    while true
    do
        make
        holdon *.c *.h
    done

---
