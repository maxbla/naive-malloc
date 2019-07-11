# Naive Malloc

This is the simplest implementation of C's [malloc family](https://linux.die.net/man/3/malloc) of functions I could come up with. It requests a new page of memory from the kernel for each allocation, which I expect to be extreemly inefficient for the case of many calls to malloc() with small size arguments, although reasonably efficient for a few large mallocs (large meaning on the order of the page size, often 4096K). Please do not use this allocator in any program you intend to be remotely stable -- it's goal is to be hackable, not tested.

# Compatibility

naive-malloc has only been tested on Linux, and depends on [procfs](https://en.wikipedia.org/wiki/Procfs) for testing, so it will not run on macOS. It also appears as if it will not work on BSDs due to differences in procfs (BSD appears to [lack](https://man.openbsd.org/FreeBSD-11.1/procfs) a /proc/self/meminfo)

# Requirements

- Linux
- A C compiler (tested with gcc)
- Make (tested with gmake)

# Build

Clone this repo

```
git clone $(REPO)
cd naiive-malloc
```

make debug or make release depending on if you want optimizations on or not

```
make debug
```

# Run

`./naive_malloc.o`
