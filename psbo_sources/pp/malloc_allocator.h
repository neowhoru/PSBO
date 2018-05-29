#pragma once

#include <stddef.h>
#include <malloc.h>

template <class T>
class malloc_allocator
{
public:
	typedef T value_type;
	typedef T* pointer;
	typedef T& reference;
	typedef const T* const_pointer;
	typedef const T& const_reference;
	typedef std::size_t size_type;
	typedef std::ptrdiff_t difference_type;

	constexpr malloc_allocator()  NOEXCEPT {}
	constexpr malloc_allocator(malloc_allocator const &)  NOEXCEPT {}
	template <class U>
	constexpr malloc_allocator(malloc_allocator<U> const &)  NOEXCEPT {}

	~malloc_allocator()  NOEXCEPT {}

	constexpr static inline pointer address(reference x)
	{
		return &x;
	}

	constexpr static inline const_pointer address(const_reference x)
	{
		return &x;
	}

	constexpr static inline size_type max_size() NOEXCEPT
	{
		return ~0UL / (unsigned)sizeof(T);
	}

	static inline pointer allocate(size_type n, void* hint = nullptr)
	{
		pointer newdata = (T*)realloc(hint, n * sizeof(T));
		if (newdata == nullptr && n > 0)
			throw std::bad_alloc();
		return newdata;
	}

	static inline void deallocate(pointer p, size_type)
	{
		free(p);
	}

	constexpr static inline void construct(pointer p, const_reference val)
	{
		new ((void*)p) T(val);
	}

	constexpr static inline void destroy(pointer p)
	{
		p->~T();
	}
};

