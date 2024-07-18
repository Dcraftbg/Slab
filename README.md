# Slab

A simple slab allocator implementation written in C

(Project is under MIT, use however you like)

Very much inspired (cough.. cough.. reimplemented) by the linux source code for slab allocators.
A big thank you to:
- [@Tsoding](https://github.com/tsoding) - for the amazing [nob.h library](https://github.com/tsoding/musializer)
- Older commit in the linux source code for the slab allocator can be found [here](https://github.com/torvalds/linux/blob/b1cb0982bdd6f57fed690f796659733350bb2cae/mm/slab.c)
- kernel.org and their [explanation of the Slab allocator](https://www.kernel.org/doc/gorman/html/understand/understand011.html)

**NOTE:** Slightly modified version of nob.h is used to fit the purposes of the project

## Quickstart

```
gcc build.c -o build 
./build                                 # Without the ./ on windows
./slab                                  # Without the ./ on windows
```

## Extra

This implementation still lacks a lot compared to that of Linux, like:
- Buffer Coloring
- Cpu cache alignment
- A good algorithm for calculating amount of objects per allocation

However it is a good start for people who want to understand how Slab allocators work and/or implement it in their own projects
I HIGHLY advise you to checkout all of the links in the big thank you as they provide a lot of context and explain a lot of the core concepts of how a Slab allocator works.
Once again, big thank you to the people on the Linux team and of course the great explanation on kernel.org

