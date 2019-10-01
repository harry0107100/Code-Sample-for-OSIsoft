#define M61_DISABLE 1
#include "m61.hh"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cinttypes>
#include <cassert>
#include <utility>
#include <vector>
#include <algorithm>


/// Document your metadata. Explain what additional data structures you used to track debugging and heavy-hitter information.
/// Define this metadata for storaging memory usage.
struct metadata {
    char* file;             // file
    long line;              // line  
    size_t size;            // size of memory in bytes              
    size_t id;              // metadata unique id
    long int unfreed;       // used to check if pointer has been freed
    metadata* next;         // points to the next metadata in the list    
    metadata* last;         // points to the last metadata in the list    
};

/// Initialize metadata struct to work.
metadata base_meta = {nullptr, 0, 0, 0, 0, nullptr, nullptr};

/// Initialize structure to track memory statistics.
m61_statistics global_stats = {0, 0, 0, 0, 0, 0, 0, 0};

/// Unique id number for filtering metadata and trailing data.
size_t id = 1996199501071030;
char idc = 'Z';

/// Instantiate one number to check if freed.
int unfreed_id = 100000;

/// Define hhstruct structure for the hhtest part, combining the file id, line id and size together for future ranking and printiing.
struct hhstruct {
    std::pair<const char*, long int> file_line;     // pair of file and line
    size_t size;                                    // allocation size
};

/// Define vector for holding hhstruct.
typedef std::vector<hhstruct> hh_vec;

/// Initial hh_vec vector.
hh_vec hhvec;

/// Define isOverflowed function to check for multiplication overflow.
bool isOverflowed(size_t a, size_t b){
    size_t prod = a * b;
    return (prod / a != b);
}

/// Define wasFreed funtion to check if a pointer has been freed.
bool wasFreed(metadata* ptr){
    return (ptr->unfreed != unfreed_id);
}

///  Define pointToExistence function to check if a pointer points to an existence range.
bool pointToExistence(void* ptr, metadata* mptr){
    void* endptr = (char*) mptr + sizeof(metadata) + mptr->size;
    return(ptr >= mptr && ptr <= endptr);
}

/// Define lineCompare Comparator to sort by line number.
bool lineCompare(hhstruct h1, hhstruct h2) {
    return (h1.file_line < h2.file_line);
}

///  Define sizeCompare function to sort by allocated size.
bool sizeCompare(hhstruct h1, hhstruct h2) {
    return (h1.size > h2.size);
}

/// m61_malloc(sz, file, line)
///    Return a pointer to `sz` bytes of newly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc must
///    return a unique, newly-allocated pointer value. The allocation
///    request was at location `file`:`line`.

void* m61_malloc(size_t sz, const char* file, long line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings.
    
    // Create metadata struct.
    metadata data = {(char*) file, line, sz, id, unfreed_id, nullptr, nullptr};

    // Check for size, if too big (just like 1024 below the upper limit), reflect in corresponding part.
    if (sz >= (size_t) -1 - global_stats.active_size - sizeof(metadata) - 1024){
        global_stats.nfail++;
        global_stats.fail_size += sz;
        return nullptr; 
    }

    // Create pointer and allocate space for overall allocation, including metadata. Also request more space than the user for following usage.
    metadata* metaptr = (metadata*) base_malloc(sz + sizeof(metadata) + 16); 
    *metaptr = data;

    // Update list structures in the linkedlist.
    metaptr->next = base_meta.next;
    metaptr->last = &base_meta;
    if (base_meta.next != nullptr){
        (base_meta.next)->last = metaptr;
    }
    base_meta.next = metaptr;

    // Create pointer to payload.
    void* ptr = (void*) ((char*) metaptr + sizeof(metadata));

    // Create trailing pointer.
    // assign unique id to trailing pointer for future check need.
    char* trailptr = (char*) ptr + sz;
    *trailptr = idc;

    // Update global statistics.
    global_stats.nactive++;
    global_stats.active_size += sz;
    global_stats.ntotal++;
    global_stats.total_size += sz;
    
    // Update the bondary of the heap for future check.
    if (((uintptr_t) ptr < global_stats.heap_min) || global_stats.heap_min == (uintptr_t) nullptr){
        global_stats.heap_min = (uintptr_t) ptr;
    }
    if (((uintptr_t) ptr + sz > global_stats.heap_max) || global_stats.heap_min == (uintptr_t) nullptr){
        global_stats.heap_max = (uintptr_t) ptr + sz;
    }

    // Update hhtest vectors
    hhstruct hhdata =  {std::pair(file, line), sz};
    hhvec.push_back(hhdata);

    return(ptr);
}


/// m61_free(ptr, file, line)
///    Free the memory space pointed to by `ptr`, which must have been
///    returned by a previous call to m61_malloc. If `ptr == NULL`,
///    does nothing. The free was called at location `file`:`line`.

void m61_free(void* ptr, const char* file, long line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings

    // Calculate back to metadata
    metadata* metaptr = (metadata*) ((char*) ptr - sizeof(metadata));

    // Check nullptr
    if (ptr == nullptr){
        return;
    }
    
    // Check if pointer points to heap
    if ((uintptr_t)ptr <  global_stats.heap_min || (uintptr_t)ptr > global_stats.heap_max){
        printf("MEMORY BUG %s:%li: invalid free of pointer %p, not in heap\n", file, line, ptr);
        return;
    }

    // Check if pointer is valid
    if((((uintptr_t) ptr & 7) != 0) || (metaptr->id != id)){
        printf("MEMORY BUG: %s:%li: invalid free of pointer %p, not allocated\n", file, line, ptr);
        metadata* checkptr = base_meta.next;
        while (checkptr != nullptr){
            if (pointToExistence(ptr, checkptr)){
                unsigned long offset = (char*) ptr - ((char*) checkptr + sizeof(metadata));
                printf("  %s:%i: %p is %lu bytes inside a %lu byte region allocated here\n", 
                        file, checkptr->line, ptr, offset, checkptr->size);
            break;
            }
            checkptr = checkptr->next;
        }

    return;
    }
    
    // Check if double free
    bool dbfreed = wasFreed(metaptr);
    if(metaptr->id == id && dbfreed){
        printf("MEMORY BUG: %s:%li: invalid free of pointer %p, double free\n", file, line, ptr);
        return;
    } 

    // Check if illegal free
    if (metaptr->next != nullptr){
        if (metaptr->next->last != metaptr){
            printf("MEMORY BUG: %s:%li: invalid free of pointer %p, not allocated\n", file, line, ptr);
      }
    }

    // Check if free corrupts of trailing data
    char* trailptr = (char*) ptr + metaptr->size;
    if(*trailptr != idc){
        printf("MEMORY BUG: %s:%li: detected wild write during free of pointer %p\n", file, line, ptr);
        return;
    } 

    // Update metadata linked list
    if (metaptr->next != nullptr){
        (metaptr->next)->last = metaptr->last;
    }
    if (metaptr->last != nullptr){
        (metaptr->last)->next = metaptr->next;
    }
    else{
        base_meta.next = metaptr->next;
    }

    // Update from metadata
    global_stats.active_size -= (metaptr->size);
    global_stats.nactive--;
    metaptr->unfreed = 0;
    base_free(metaptr);
    
    return;
}
 

/// m61_calloc(nmemb, sz, file, line)
///    Return a pointer to newly-allocated dynamic memory big enough to
///    hold an array of `nmemb` elements of `sz` bytes each. If `sz == 0`,
///    then must return a unique, newly-allocated pointer value. Returned
///    memory should be initialized to zero. The allocation request was at
///    location `file`:`line`.

void* m61_calloc(size_t nmemb, size_t sz, const char* file, long line) {
    // Your code here (to fix test014).
    if (isOverflowed(nmemb, sz)){
        global_stats.nfail++;
        return nullptr;
    }

    void* ptr = m61_malloc(nmemb * sz, file, line);
    if (ptr != nullptr) {
        memset(ptr, 0, nmemb * sz);
    }
    return ptr;
}


/// m61_getstatistics(stats)
///    Store the current memory statistics in `*stats`.
void m61_get_statistics(m61_statistics* stats) {
    // Stub: set all statistics to enormous numbers
    memset(stats, 255, sizeof(m61_statistics));
    // Your code here.
    *stats = global_stats;
}


/// m61_print_statistics()
///    Print the current memory statistics.

void m61_print_statistics() {
    m61_statistics stats;
    m61_get_statistics(&stats);

    printf("alloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("alloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}


/// m61_print_leak_report()
///    Print a report of all currently-active allocated blocks of dynamic
///    memory.

void m61_print_leak_report() {
    metadata* next = base_meta.next;
    while (next != nullptr){
        char* file  = next->file;
        long int line = next->line;
        long long int sz = next->size;
        printf("LEAK CHECK: %s:%li: allocated object %p with size %lli\n", 
              file, line, (metadata*) ((char*) next + sizeof(metadata)), sz);
        next = next->next; 
    }
    return;
}

/// m61_print_heavy_hitter_report()
///   Print a report of heavily-used allocation locations.

void m61_print_heavy_hitter_report() {
  
    // Sort by line number
    std::sort (hhvec.begin(), hhvec.end(), lineCompare);
    
    // Group by the size of different line by lines.
    hh_vec result;
    result.push_back(hhvec[0]);
    for(unsigned int i = 1; i != hhvec.size(); i++) {
        if(hhvec[i].file_line == result.back().file_line) {
            result.back().size = result.back().size + hhvec[i].size;
        }
        else {
            result.push_back(hhvec[i]);
      }
    }

    // Sort by size.
    std::sort (result.begin(), result.end(), sizeCompare);
    
    ///calculate total size.
    float total_size1 = 0.0;
    for(int i = 0; i != result.size(); i++) {
        total_size1 += result[i].size;
    }

    /// calculate usage in each line, print the line which takes up more than 20 percent of usage.
    for(int i = result.size(); i != 0; i--) {
        float percent = result[i].size / total_size1 * 100;
        if (percent >= 20) {
            printf("HEAVY HITTER: %s:%li: %lu bytes (~%.3f)\n", std::get<const char*>(result[i].file_line), 
                                                                  std::get<long int>(result[i].file_line), 
                                                                  result[i].size, percent);
        }
    }
    
return;
}
