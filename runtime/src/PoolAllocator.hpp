#ifndef POOL_ALLOCATOR_H
#define POOL_ALLOCATOR_H

#include <cstddef> // for std::size_t, std::ptrdiff_t
#include <limits>
#include "MemMapAllocator.h"

namespace MPoolLib {
	template <class T>
	class PoolAllocator {
	  public:
		// type definitions
		typedef T        value_type;
		typedef T*       pointer;
		typedef const T* const_pointer;
		typedef T& reference;
		typedef const T& const_reference;
		typedef std::size_t size_type;
		typedef std::ptrdiff_t difference_type;

		// rebind allocator to type U
		template <class U>
		struct rebind {
			typedef PoolAllocator<U> other;
		};

		// return address of values
		pointer address (reference value) const {
			return &value;
		}
		const_pointer address (const_reference value) const {
			return &value;
		}

		/* Constructors and Destructors do nothing. There is no state. */
		PoolAllocator() throw() {}
		PoolAllocator(const PoolAllocator&) throw() {}
		template <class U>
			PoolAllocator (const PoolAllocator<U>&) throw() {}
		~PoolAllocator() throw() {}

		// return max num of elements that can be allocated
		size_type max_size() const throw() {
			return std::numeric_limits<std::size_t>::max() / sizeof(T);
		}

		// allocate but don't initialize num elements of type T
		pointer allocate (size_type num, const void* = 0) {
			pointer ret = (pointer)MemPoolAllocSmall(num*sizeof(T));
			return ret;
		}

		// init elems of allocated storage p with value value
		void construct(pointer p, const T& value) {
			// init mem with placement new
			new((void*)p)T(value);
		}

		// destroy elems of initialized storage p
		void destroy(pointer p) {
			p->~T();
		}
		
		// dealloc storage p of deleted elements
		void deallocate(pointer p, size_type num) {
			MemPoolFreeSmall(p, num*sizeof(T));
		}
	};

	// return that all specializations of this allocator are interchangeable
	template <class T1, class T2>
	bool operator== (const PoolAllocator<T1>&, const PoolAllocator<T2>&) throw() {
		return true;
	}

	template <class T1, class T2>
	bool operator!= (const PoolAllocator<T1>&, const PoolAllocator<T2>&) throw() {
		return false;
	}
}

#endif // POOL_ALLOCATOR_H
