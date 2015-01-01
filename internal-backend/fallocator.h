#ifndef CRACK_FALLOCATOR_H_
#define CRACK_FALLOCATOR_H_

// The following headers are required for all allocators.
#include <stddef.h>  // Required for size_t and ptrdiff_t and NULL
#include <new>       // Required for placement new and std::bad_alloc
#include <stdexcept> // Required for std::length_error

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

// The following headers contain stuff that Mallocator uses.
#include <stdlib.h>  // For malloc() and free()
#include <assert.h>  // For malloc() and free()
#include <string.h>  // For malloc() and free()
#include <iostream>  // For std::cout
#include <ostream>   // For std::endl
#include <string>   // For std::endl

template <typename T>
class Fallocator {
 public:
  typedef T* pointer;
  typedef const T* const_pointer;
  typedef T& reference;
  typedef const T& const_reference;
  typedef T value_type;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  std::string fn_;
  int flags_;

  T* address(T& r) const { return &r; }
  const T* address(const T& s) const { return &s; }

  size_t max_size() const {
    // The following has been carefully written to be independent of
    // the definition of size_t and to avoid signed/unsigned warnings.
    return (static_cast<size_t>(0) - static_cast<size_t>(1)) / sizeof(T);
  }

  // The following must be the same for all allocators.
  template <typename U> struct rebind {
    typedef Fallocator<U> other;
  };

  bool operator!=(const Fallocator& other) const {
    return !(*this == other);
  }

  void construct(T* const p, const T& t) const {
    void* const pv = static_cast<void*>(p);
    new (pv) T(t);
  }

  void destroy(T* const p) const; // Defined below.

  // Returns true if and only if storage allocated from *this
  // can be deallocated from other, and vice versa.
  // Always returns true for stateless allocators.
  bool operator==(const Fallocator& other) const {
    return true;
  }


  // Default constructor, copy constructor, rebinding constructor, and destructor.
  // Empty for stateless allocators.
  Fallocator() { }

  Fallocator(std::string fn): Fallocator(fn, false) {}

  Fallocator(std::string fn, bool memory_lock): fn_(fn) {
    #ifdef MAP_LOCKED
      flags_ = memory_lock ? MAP_LOCKED : 0;
    #else
      flags_ = 0;
      if (memory_lock) {
        fprintf(stderr, "Unsupported MAP_LOCKED for %s\n", fn.c_str());
      }
    #endif
    bool is_new = access(fn_.c_str(), F_OK) == -1;
    if (is_new) {
      fprintf(stderr, "File is new %s, writing zeros to the first 8 bytes\n", fn_.c_str());
      FILE *f = fopen(fn_.c_str(), "wb");
      if (!f) {
        fprintf(stderr, "Fail to write to %s\n", fn_.c_str());
        abort();
      }
      int zeros[2] = { 0 };
      fwrite(zeros, sizeof(int), 2, f);
      fclose(f);
    }
  }

  Fallocator(const Fallocator&) { }

  template <typename U> Fallocator(const Fallocator<U>&) { }


  // The following will be different for each allocator.
  T* allocate(size_t n) const {
    // Fallocator prints a diagnostic message to demonstrate
    // what it's doing. Real allocators won't do this.
    std::cout << "Allocating " << n << (n == 1 ? " object" : " objects")
        << " of size " << sizeof(T) << " on file " << fn_ << "." << std::endl;

    // The return value of allocate(0) is unspecified.
    // Fallocator returns NULL in order to avoid depending
    // on malloc(0)'s implementation-defined behavior
    // (the implementation can define malloc(0) to return NULL,
    // in which case the bad_alloc check below would fire).
    // All allocators can return NULL in this case.
    assert(n > 0);

    // All allocators should contain an integer overflow check.
    // The Standardization Committee recommends that std::length_error
    // be thrown in the case of integer overflow.
    if (n > max_size()) {
      assert(!"Fallocator<T>::allocate() - Integer overflow.");
      assert(0);
    }

    bool is_new = access(fn_.c_str(), F_OK) == -1;
    if (is_new) {
      fprintf(stderr, "File %s is missing\n", fn_.c_str());
      abort();
    }

    // Fallocator wraps malloc().
    // Allocators should throw std::bad_alloc in the case of memory allocation failure.
    int fd = open(fn_.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IXUSR);
    if (fd < 0) {
      fprintf(stderr, "couldn't create %s for writing\n", fn_.c_str());
      assert(0);
    }

    // Extend the file if necessary.
    if (lseek(fd, (sizeof(T) * n) - 1, SEEK_SET) == -1) {
      fprintf(stderr, "lseek error on file %s\n", fn_.c_str());
      assert(0);
    }

    char tmp;
    read(fd, &tmp, 1);

    if (lseek(fd, (sizeof(T) * n) - 1, SEEK_SET) == -1) {
      fprintf(stderr, "lseek error on file %s (%lu * %lu)\n", fn_.c_str(), sizeof(T), n);
      assert(0);
    }
    if (write(fd, &tmp, 1) != 1) {
      fprintf(stderr, "write error on file %s (%lu * %lu)\n", fn_.c_str(), sizeof(T), n);
      assert(0);
    }

    void* pv = mmap(0, sizeof(T) * n, PROT_READ | PROT_WRITE, MAP_SHARED | flags_, fd, 0);
    if (pv == MAP_FAILED) {
      if (errno == 11) {
        fprintf(stderr, "MAP_LOCKED failed for %s, giving up MAP_LOCKED\n", fn_.c_str());
        pv = mmap(0, sizeof(T) * n, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      }
    }
    if (pv == MAP_FAILED) {        
      fprintf(stderr, "mmap error for output on file %s, errno = %d\n", fn_.c_str(), errno);
      perror("MMAP FAILED");
      assert(0);
    }
    close(fd);

    if (is_new) {
      fprintf(stderr, "zeroing file %s with %lu bytes\n", fn_.c_str(), sizeof(T) * n);
      memset(pv, 0, sizeof(T) * n);
    }

    return static_cast<T*>(pv);
  }

  void deallocate(T* const p, const size_t n) const {
    // Fallocator prints a diagnostic message to demonstrate
    // what it's doing. Real allocators won't do this.
    std::cout << "Deallocating " << n << (n == 1 ? " object" : " objects")
        << " of size " << sizeof(T) << " on file " << fn_ << "." << std::endl;

    msync(p, sizeof(T) * n, MS_SYNC);
    munmap(p, sizeof(T) * n);
  }

  T* reallocate(T* const p, const size_t old_n, const size_t new_n) const {
    deallocate(p, old_n);
    return allocate(new_n);
    // return static_cast<T*>(mremap(p, old_n, new_n, MREMAP_MAYMOVE));
  }

  // The following will be the same for all allocators that ignore hints.
  template <typename U>
  T* allocate(const size_t n, const U * /* const hint*/) const {
    return allocate(n);
  }


  // Allocators are not required to be assignable, so
  // all allocators should have a private unimplemented
  // assignment operator. Note that this will trigger the
  // off-by-default (enabled under /Wall) warning C4626
  // "assignment operator could not be generated because a
  // base class assignment operator is inaccessible" within
  // the STL headers, but that warning is useless.
 private:
  Fallocator& operator=(const Fallocator&);
};

// The definition of destroy() must be the same for all allocators.
template <typename T>
void Fallocator<T>::destroy(T* const p) const {
  p->~T();
}

#endif
