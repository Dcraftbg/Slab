// If you want to quickly move around and/or replace parts of your code
// In your IDE of choice you can just search for:
//  @STAT   things that keep track of stats
//  @DEBUG  debug utilities for logging 
//  @STATUS replace with your own status system


#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define PAGE_ALIGN(x) ((((x)+4095)/4096) * 4096)
// NOTE: Some definition for allocations that I use throughout my code.
// Can be replaced with whever you like honestly
void* page_alloc(size_t pages) {
    return malloc(pages * 4096);
}
void page_dealloc(void* data, size_t pages) {
    free(data);
}
void* kernel_malloc(size_t memory) {
    return page_alloc((memory+4095)/4096);
}
void kernel_free(void* data, size_t size) {
    return page_dealloc(data, (size+4095)/4096);
}

// NOTE: This implementation is a slightly modified version of that of the linux kernel itself.
// https://github.com/torvalds/linux/blob/b1bc554e009e3aeed7e4cfd2e717c7a34a98c683/tools/firewire/list.h
//
// Things like list_insert, list_append are all literally the exact same, with the exception 
// of list_remove which also closes in the array element removed since the linux version didn't clean it up
struct list {
    struct list *next, *prev;
};
static void list_init(struct list* list) {
    list->next = list;
    list->prev = list;
}
static struct list* list_last(struct list* list) {
    return list->prev != list ? list->prev : NULL;
}

static struct list* list_next(struct list* list) {
    return list->next != list ? list->next : NULL;
}
static inline void list_insert(struct list *new, struct list *link) {
    new->prev = link->prev;
    new->next = link;
    new->prev->next = new;
    new->next->prev = new;
}
static void list_append(struct list *new, struct list *into) {
    list_insert(new, into);
}
static void list_remove(struct list* list) {
    list->prev->next = list->next;
    list->next->prev = list->prev;
    list->next = list;
    list->prev = list;
}


struct Slab;
// TODO: Give caches a name field like what linux has for @STAT purposes
typedef struct {
    // Linked list to either:
    // - fully filled blocks     => blocks with no available memory
    // - partially filled blocks => blocks with partially available memory
    // - free blocks             => empty blocks with fully available memory
    struct list full, partial, free;
    // @STAT
    // Total amount of objects
    size_t totalobjs;
    // @STAT
    // Amount of objects allocated currently
    size_t inuse;
    // Size of a single object
    size_t objsize;
    // Objects per slab
    size_t objs_per_slab;
} Cache;
typedef struct Slab { 
    struct list list;
    Cache* cache; // Cache it belongs to
    void* memory;
    // Maintain a stack of free objects (indexes to objects within the slab). Top of the stack is at free
    // We push objects and increment the free pointer when we want to allocate something,
    // and we decrement the pointer and write the free address at that point
    size_t free; 
} Slab;
// A cache for caches. Pretty funny
Cache* cache_cache=NULL;

// Skip the slabs' data (+1, equivilent to +sizeof(Slab))
// and right after it you'll find an array of bufctl
// aka the stack of free objects
#define slab_bufctl(slab) ((uint32_t*)(slab+1))



static size_t slab_mem(Cache* cache) {
    return cache->objs_per_slab * (cache->objsize + sizeof(uint32_t)) + sizeof(Slab);
}
// TODO: Flags for on/off slab
// https://github.com/torvalds/linux/blob/b1cb0982bdd6f57fed690f796659733350bb2cae/mm/slab.c#L2575
intptr_t cache_grow(Cache* cache) {
    void *addr = kernel_malloc(slab_mem(cache));
    if(!addr) return -1; // @STATUS 
    Slab* slab = addr;
    slab->cache = cache;
    // Memory follows immediately after the slab 
    // (we have the memory field because in the future
    //  you might decide to move the memory to its own page)
    slab->memory = ((uint32_t*)(addr+sizeof(*slab)))+cache->objs_per_slab;
    slab->free = 0;
    cache->totalobjs+=cache->objs_per_slab;
    for(size_t i = 0; i < cache->objs_per_slab-1; ++i) {
        slab_bufctl(slab)[i] = i;
    }
    list_append((struct list*)slab, &cache->free);
    return 0;
}
// Selects a cache in which you can allocate your objects
static Slab* cache_select(Cache* cache) {
    Slab* s = NULL;
    if((s = (Slab*)list_next(&cache->partial))) return s;
    if((s = (Slab*)list_next(&cache->free))) return s;
    if(cache_grow(cache) != 0) return NULL; // @STATUS
    s = (Slab*)list_next(&cache->free);
    return s;
}
void* cache_alloc(Cache* cache) {
    Slab* slab = cache_select(cache);
    if(!slab) return NULL;
    size_t index = slab_bufctl(slab)[slab->free];
    void* ptr = slab->memory + cache->objsize * index;
    // The Slab was free before, lets move it to partial since we allocated something
    if(slab->free++ == 0) {
        list_remove(&slab->list);
        list_append(&cache->partial, &slab->list);
    }
    // The Slab was paritally filled, but now is full, lets move it to the full linked list
    if(slab->free == cache->objs_per_slab) {
        list_remove(&slab->list);
        list_append(&cache->full, &slab->list);
    }
    cache->inuse++;
    return ptr;
}
static size_t slab_ptr_to_index(Cache* cache, Slab* slab, void* p) {
    return (size_t)(p - slab->memory) / cache->objsize;
}

// @DEBUG
static void log_list(struct list* list) {
    struct list* first = list;
    list = list->next; // Always skip the first one in this case, since cache.full/partial/free is not a valid Slab
    while(first != list) {
        printf("- %p\n",list);
        list = list->next;
    }
}
// @DEBUG
static void log_cache(Cache* cache) {
    printf("Cache:\n");
    printf("Objects: %zu/%zu\n",cache->inuse,cache->totalobjs);
    printf("Object Size: %zu\n",cache->objsize);
    printf("Object Per Slab: %zu\n",cache->objs_per_slab);
    printf("Full:\n");
    log_list(&cache->full);
    printf("Partial:\n");
    log_list(&cache->partial);
    printf("Free:\n");
    log_list(&cache->free);
}

static bool cache_dealloc_within(Cache* cache, Slab* slab, void* p) {
    Slab *first = slab;
    while(slab != (Slab*)first->list.prev) {
        if(p >= slab->memory && p < slab->memory+cache->objsize*cache->objs_per_slab) {
            assert(slab->free);
            if(slab->free-- == cache->objs_per_slab) {
                list_remove(&slab->list);
                list_append(&cache->partial, &slab->list);
            }
            if(slab->free == 0) {
                list_remove(&slab->list);
                list_append(&cache->free, &slab->list);
            }
            size_t index = slab_ptr_to_index(cache, slab, p);
            slab_bufctl(slab)[slab->free]=index;
            cache->inuse--;
            return true;
        }
        slab = (Slab*)slab->list.next;
    }
    return false;
}
void cache_dealloc(Cache* cache, void* p) {
    Slab *s = (Slab*)list_next(&cache->full);
    if(s) {
        if(cache_dealloc_within(cache, s, p)) return;
    }
    s = (Slab*)list_next(&cache->partial);
    if(s) {
        if(cache_dealloc_within(cache, s, p)) return;
    }
}
// TODO: Non-gready algorithmn for better optimisations 
// in memory usage AND that takes in consideration the Slab itself
// And of course the bufctl index stack
void init_cache(Cache* c, size_t objsize) {
    memset(c, 0, sizeof(*c)); 
    list_init(&c->partial);
    list_init(&c->full);
    list_init(&c->free);
    c->objsize = objsize;
    // Gready algorithmn
    c->objs_per_slab = PAGE_ALIGN(objsize) / objsize;
}
Cache* create_new_cache(size_t objsize) {
    Cache* c = (Cache*)cache_alloc(cache_cache);
    if(!c) return NULL;
    init_cache(c, objsize);
    return c;
}
typedef struct {
    size_t a;
    size_t b;
} Foo;
// NOTE: Leaks a bit of memory
int main(void) {
    cache_cache = kernel_malloc(sizeof(Cache));
    init_cache(cache_cache, sizeof(Cache));
    printf("cache_cache: %p", cache_cache);
    Cache* foo_cache = NULL;
    void* p = NULL;
    foo_cache = create_new_cache(sizeof(Foo));

    printf("Foo cache: %p\n",foo_cache);
    p = cache_alloc(foo_cache);
    printf("Foo: %p\n",p);
#if 1
    printf("deallocating...\n");
    cache_dealloc(foo_cache, p);
#else
    printf("NOT deallocating...\n");
#endif
    p = cache_alloc(foo_cache);
    printf("Foo: %p\n",p);
    log_cache(foo_cache);
}
