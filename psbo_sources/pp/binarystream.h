#pragma once

#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <sstream>

#include <array>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include "nullinit.h"

#ifdef _MSC_VER
#pragma warning(disable:4244)
#pragma warning(disable:4800)
#endif


template <class T>
struct ClassSerializationData
{
	static const unsigned serializable = false;
};


class BinaryReader;
class BinaryWriter;
std::ostream & operator << (std::ostream & o, BinaryReader const & br);

template <typename ... ARGS>
struct sum_size;

template <typename ARG, typename ... ARGS>
struct sum_size<ARG, ARGS...>
	: public std::integral_constant<size_t, sizeof(ARG) + sum_size<ARGS...>::value>
{};

template <>
struct sum_size<>
	: public std::integral_constant<size_t, 0>
{};


template <class T>
class stream_array
{
	static_assert(std::is_pod<T>::value, "T must be POD");
	const char * pos;
public:
	typedef T value_type;
	unsigned size() const
	{
		return *reinterpret_cast<const unsigned*>(pos);
	}
	T const & operator[](unsigned index) const
	{
		return *reinterpret_cast<const T*>(pos + 4 + index * sizeof(T));
	}

	void operator << (BinaryWriter & bw) const;

	void operator >> (BinaryReader & br);

	T const * data() const
	{
		return reinterpret_cast<const T*>(pos + 4);
	}

	T const * begin() const
	{
		return reinterpret_cast<const T*>(pos + 4);
	}
	T const * end() const
	{
		return reinterpret_cast<const T*>(pos + 4 + size() * sizeof(T));
	}
};

template <class T, unsigned count>
class fixed_stream_array
{
	static_assert(std::is_pod<T>::value, "T must be POD");
	const char * pos;
public:
	typedef T value_type;
	unsigned size() const
	{
		return count;
	}
	T const & operator[](unsigned index) const
	{
		return *reinterpret_cast<const T*>(pos + index * sizeof(T));
	}

	void operator << (BinaryWriter & bw) const;

	void operator >> (BinaryReader & br);

	T const * data() const
	{
		return reinterpret_cast<const T*>(pos);
	}

	T const * begin() const
	{
		return reinterpret_cast<const T*>(pos);
	}
	T const * end() const
	{
		return reinterpret_cast<const T*>(pos + size() * sizeof(T));
	}
};

template <class T>
class iterator_pair
{
	static_assert(std::is_pod<typename T::value_type>::value, "iterator::value_type must be POD");
	T b, e;
public:
	typedef const decltype(*b) value_type;
	iterator_pair() {}
	iterator_pair(T const & b, T const & e)
		: b(b), e(e) 
	{}

	void operator << (BinaryWriter & bw) const;

	void operator >> (BinaryReader & br);

	unsigned size() const
	{
		return std::distance(b, e);
	}

	T begin() const
	{
		return b;
	}
	T end() const
	{
		return e;
	}
};

#ifndef __GNUC__

template<typename T>
class has_binary_read
{
	typedef char yes[1];
	typedef char no[2];
	template <typename U, U> struct really_has;

	template <typename C> static yes& test(really_has <BinaryReader &(C::*)(BinaryReader &), &(C::operator >>) >*);
	template <typename C> static yes& test(really_has <void (C::*)(BinaryReader &), &(C::operator >>) >*);
	template<typename C> static no&  test(...);
public:
	static bool const value = sizeof(test<T>(0)) == sizeof(yes);
};

template<typename T>
class has_binary_write
{
	typedef char yes[1];
	typedef char no[2];
	template <typename U, U> struct really_has;

	template <typename C> static yes& test(really_has <BinaryWriter &(C::*)(BinaryWriter &) const, &(C::operator <<) >*);
	template <typename C> static yes& test(really_has <void (C::*)(BinaryWriter &) const, &(C::operator <<) >*);
	template<typename C> static no&  test(...);
public:
	static bool const value = sizeof(test<T>(0)) == sizeof(yes);
};
#else

template<typename T>
class has_binary_read
{
	typedef char yes[1];
	typedef char no[2];

	template <typename C> static yes& test(decltype(&C::operator >>));
	template<typename C> static no&  test(...);
public:
	static bool const value = sizeof(test<T>(0)) == sizeof(yes);
};

template<typename T>
class has_binary_write
{
	typedef char yes[1];
	typedef char no[2];

	template <typename C> static yes& test(decltype(&C::operator <<));
	template<typename C> static no&  test(...);
public:
	static bool const value = sizeof(test<T>(0)) == sizeof(yes);
};

#endif

namespace detail
{
	template <typename ... ARGS>
	struct uc_read_e;

	template <typename ARG, typename ... ARGS>
	struct uc_read_e<ARG, ARGS...>
		: public std::integral_constant<bool, std::is_pod<ARG>::value && !has_binary_read<ARG>::value && uc_read_e<ARGS...>::value>
	{};

	template <>
	struct uc_read_e<>
		: public std::integral_constant<bool, true> 
	{};


	template <class T, bool is_pod = std::is_pod<T>::value && !has_binary_read<T>::value>
	struct stream_size
//		: public std::integral_constant<bool, false>
	{
		constexpr static inline size_t size(T const & value)
		{
			return 0;
		}
		constexpr static inline size_t const_size()
		{
			return 0;
		}
	};

	template <class T>
	struct stream_size<T, true>
//		: public std::integral_constant<bool, true>
	{
		constexpr static inline size_t size(T const & value)
		{
			return sizeof(T);
		}
		constexpr static inline size_t const_size()
		{
			return sizeof(T);
		}
	};

	template <class A, class B>
	struct stream_size<std::pair<A, B>, false>
//		: public std::integral_constant<bool, stream_size<A>::value && stream_size<B>::value>
	{
		static inline size_t size(std::pair<A, B> const & value)
		{
			return stream_size<A>::size(value.first) + stream_size<B>::size(value.second);
		}
		constexpr static inline size_t const_size()
		{
			return stream_size<A>::size() + stream_size<B>::size();
		}
	};

	template <typename ... ARGS>
	struct tuple_stream_size;

	template <typename ARG, typename ... ARGS>
	struct tuple_stream_size<ARG, ARGS...>
	{
		template <unsigned i, class T>
		static inline size_t size(T const & value)
		{
			return stream_size<ARG>::size(std::get<i>(value)) + tuple_stream_size<ARGS...>::size<i + 1>(value);
		}
		constexpr static inline size_t const_size()
		{
			return stream_size<ARG>::size() + tuple_stream_size<ARGS...>::const_size();
		}
	};

	template <>
	struct tuple_stream_size<>
	{
		template <unsigned i, class T>
		constexpr static inline size_t size(T const & value)
		{
			return 0;
		}
		constexpr static inline size_t const_size()
		{
			return 0;
		}
	};

	template <typename ... ARGS>
	struct stream_size<std::tuple<ARGS...>, false>
//		: public std::integral_constant<bool, false> 
	{
		static inline size_t size(std::tuple<ARGS...> const & value)
		{
			return tuple_stream_size<ARGS...>::size<0>(value);
		}
		constexpr static inline size_t const_size()
		{
			return tuple_stream_size<ARGS...>::const_size();
		}
	};

	template <>
	struct stream_size<std::string, false>
//		: public std::integral_constant<bool, true> 
	{
		static inline size_t size(std::string const & value)
		{
			return value.size() + sizeof(unsigned);
		}
		constexpr static inline size_t const_size()
		{
			return sizeof(unsigned);
		}
	};

	template <class T, unsigned len>
	struct stream_size<std::array<T, len>, false>
//		: public std::integral_constant<bool, stream_size<T>::value>
	{
		constexpr static inline size_t size(std::array<T, len> const & value)
		{
			return len * stream_size<T>::const_size();
		}
		constexpr static inline size_t const_size()
		{
			return len * stream_size<T>::const_size();
		}
	};

	template <class T, unsigned len>
	struct stream_size<fixed_stream_array<T, len>, false>
//		: public std::integral_constant<bool, stream_size<T>::value>
	{
		constexpr static inline size_t size(fixed_stream_array<T, len> const & value)
		{
			return len * stream_size<T>::const_size();
		}
		constexpr static inline size_t const_size()
		{
			return len * stream_size<T>::const_size();
		}
	};

	template <class T>
	struct stream_size<std::vector<T>, false>
//		: public std::integral_constant<bool, stream_size<T>::value>
	{
		static inline size_t size(std::vector<T> const & value)
		{
			return value.size() * stream_size<T>::const_size() + sizeof(unsigned);
		}
		constexpr static inline size_t const_size()
		{
			return sizeof(unsigned);
		}
	};

	template <class T>
	struct stream_size<stream_array<T>, false>
//		: public std::integral_constant<bool, stream_size<T>::value>
	{
		static inline size_t size(stream_array<T> const & value)
		{
			return value.size() * stream_size<T>::const_size() + sizeof(unsigned);
		}
		constexpr static inline size_t const_size()
		{
			return sizeof(unsigned);
		}
	};

	template <class T>
	struct stream_size<iterator_pair<T>, false>
	{
		static inline size_t size(iterator_pair<T> const & value)
		{
			return value.size() * stream_size<typename T::value_type>::const_size() + sizeof(unsigned);
		}
		constexpr static inline size_t const_size()
		{
			return sizeof(unsigned);
		}
	};

	template <class T>
	struct stream_size<std::list<T>, false>
//		: public std::integral_constant<bool, stream_size<T>::value>
	{
		static inline size_t size(std::list<T> const & value)
		{
			return value.size() * stream_size<T>::const_size() + sizeof(unsigned);
		}
		constexpr static inline size_t const_size()
		{
			return sizeof(unsigned);
		}
	};

	template <class T>
	struct stream_size<std::set<T>, false>
//		: public std::integral_constant<bool, stream_size<T>::value>
	{
		static inline size_t size(std::set<T> const & value)
		{
			return value.size() * stream_size<T>::const_size() + sizeof(unsigned);
		}
		constexpr static inline size_t const_size()
		{
			return sizeof(unsigned);
		}
	};

	template <class T>
	struct stream_size<std::multiset<T>, false>
//		: public std::integral_constant<bool, stream_size<T>::value>
	{
		static inline size_t size(std::multiset<T> const & value)
		{
			return value.size() * stream_size<T>::const_size() + sizeof(unsigned);
		}
		constexpr static inline size_t const_size()
		{
			return sizeof(unsigned);
		}
	};

	template <class T>
	struct stream_size<std::unordered_set<T>, false>
//		: public std::integral_constant<bool, stream_size<T>::value>
	{
		static inline size_t size(std::unordered_set<T> const & value)
		{
			return value.size() * stream_size<T>::const_size() + sizeof(unsigned);
		}
		constexpr static inline size_t const_size()
		{
			return sizeof(unsigned);
		}
	};

	template <class K, class T>
	struct stream_size<std::map<K, T>, false>
//		: public std::integral_constant<bool, stream_size<K>::value && stream_size<T>::value>
	{
		static inline size_t size(std::map<K, T> const & value)
		{
			return value.size() * (stream_size<K>::const_size() + stream_size<T>::const_size()) + sizeof(unsigned);
		}
		constexpr static inline size_t const_size()
		{
			return sizeof(unsigned);
		}
	};

	template <class K, class T>
	struct stream_size<std::multimap<K, T>, false>
//		: public std::integral_constant<bool, stream_size<K>::value && stream_size<T>::value>
	{
		static inline size_t size(std::multimap<K, T> const & value)
		{
			return value.size() * (stream_size<K>::const_size() + stream_size<T>::const_size()) + sizeof(unsigned);
		}
		constexpr static inline size_t const_size()
		{
			return sizeof(unsigned);
		}
	};

	template <class K, class T>
	struct stream_size<std::unordered_map<K, T>, false>
//		: public std::integral_constant<bool, stream_size<K>::value && stream_size<T>::value>
	{
		static inline size_t size(std::unordered_map<K, T> const & value)
		{
			return value.size() * (stream_size<K>::const_size() + stream_size<T>::const_size()) + sizeof(unsigned);
		}
		constexpr static inline size_t const_size()
		{
			return sizeof(unsigned);
		}
	};
};



class UnexpectedEndOfData
	: public std::runtime_error
{
public:
	UnexpectedEndOfData()
		: std::runtime_error("Unexpected read in binary reader")
	{}
	virtual ~UnexpectedEndOfData()
#ifdef __GNUC__
		noexcept (true)
#endif
	{}
};


class BinaryReader
{
	template <class T>
	friend class stream_array;

	template <class T, unsigned count>
	friend class fixed_stream_array;

	friend class BinaryWriter;
	friend std::ostream & operator << (std::ostream & o, BinaryReader const & br);

	const char * data;
	unsigned data_size;
	unsigned pos;
public:

	BinaryReader(const char * const data, unsigned data_size)
		: data(data), data_size(data_size), pos(0)
	{}

	BinaryReader(std::vector<char> const & data)
		: data(data.data()), data_size(data.size()), pos(0)
	{}

	inline bool eof() const
	{
		return pos >= data_size;
	}

	inline bool eof(unsigned s) const
	{
		return pos + s > data_size;
	}

	void seek(unsigned pos)
	{
		this->pos = pos;
	}

	unsigned tell() const
	{
		return pos;
	}

	void skip(unsigned n)
	{
		this->pos += n;
	}

	template <class T>
	void skip()
	{
		this->pos += sizeof(T);
	}

	unsigned read_length();

private:

	template <class T, bool is_reflectable = false/*::is_reflectable<T>::value*/, bool is_pod = std::is_pod<T>::value, bool has_br = has_binary_read<T>::value>
	struct read_helper
	{
		static _forceinline void read(BinaryReader & br, T & value)
		{
			value.operator >>(br);
		}
	};

	template <class T, bool is_reflectable, bool is_pod>
	struct read_helper < T, is_reflectable, is_pod, true>
	{
		static _forceinline void read(BinaryReader & br, T & value)
		{
			value.operator >>(br);
		}
	};

	template <class T>
	struct read_helper < T, true, false, false>
	{
		static _forceinline void read(BinaryReader & br, T & value)
		{
			read_reflectable<0, T::memberCount, T>::read(br, value);
		}
	};

	template <class T, bool is_reflectable>
	struct read_helper < T, is_reflectable, true, false>
	{
		static _forceinline void read(BinaryReader & br, T & value)
		{
			if (br.data_size - br.pos < sizeof(T))
				throw UnexpectedEndOfData();
			value = *reinterpret_cast<const T*>(&br.data[br.pos]);
			br.pos += sizeof(T);
		}
	};

	template <bool is_reflectable>
	struct read_helper < unsigned long, is_reflectable, true, false>
	{
		static _forceinline void read(BinaryReader & br, unsigned long & value);	//unsigned long check
	};


	template <class T>
	void _forceinline _read(T & value)
	{
		read_helper<T>::read(*this, value);
	}

	void _read(unsigned long & value);//unsigned long check

	template <class T>
	void _forceinline _read_uc(T & value)
	{
		static_assert(std::is_pod<T>::value && !has_binary_read<T>::value, "_read_uc error");
		value = *reinterpret_cast<const T*>(&data[pos]);
		pos += sizeof(T);
	}


	template <unsigned i, unsigned e, class T>
	struct read_reflectable
	{
		_forceinline static void read(BinaryReader & br, T & value)
		{
//			br.read(ref_get<i>(value));
//			read_reflectable<i + 1, e, T>::read(br, value);
		}
	};

	template <unsigned i, class T>
	struct read_reflectable<i, i, T>
	{
		_forceinline static void read(BinaryReader & br, T & value)
		{
		}
	};

	template <unsigned i, unsigned e, typename ... ARGS>
	struct read_tuple
	{
		_forceinline static void read(BinaryReader & br, std::tuple<ARGS...> & value)
		{
			br.read(std::get<i>(value));
			read_tuple<i + 1, e, ARGS...>::read(br, value);
		}
	};

	template <unsigned i, typename ... ARGS>
	struct read_tuple<i, i, ARGS...>
	{
		_forceinline static void read(BinaryReader & br, std::tuple<ARGS...> & value)
		{
		}
	};

	template <typename ... ARGS>
	inline void _read(std::tuple<ARGS...> & value)
	{
		read_tuple<0, sizeof...(ARGS), ARGS...>::read(*this, value);
	}



	template <class T>
	const T* get(unsigned length)
	{
		static_assert(std::is_pod<T>::value, "T must be POD for 'get'");

		length *= sizeof(T);
		if (data_size - pos < length)
			throw UnexpectedEndOfData();

		pos += length;
		return reinterpret_cast<const T*>(&data[pos - length]);
	}


	template <class T, class U>
	inline void _read(std::pair<T, U> & value)
	{
		_read(value.first);
		_read(value.second);
	}

	template <class T, class U>
	inline void _read_uc(std::pair<T, U> & value)
	{
		_read_uc(value.first);
		_read_uc(value.second);
	}

	template <class T>
	inline void _read(std::atomic<T> & value)
	{
		value = read<T>();
	}

	template <class T, bool is_pod = std::is_pod<T>::value && !has_binary_read<T>::value>
	struct array_read_helper
	{
		static inline void read(BinaryReader * br, T * values, unsigned count)
		{
			for (unsigned i = 0; i < count; ++i)
				br->_read(values[i]);
		}
	};

	template <class T>
	struct array_read_helper < T, true >
	{
		static inline void read(BinaryReader * br, T * values, unsigned length)
		{
			const T *in = br->get<T>(length);
			memcpy(values, in, length * sizeof(T));
		}
	};

	template <class T2, class T, bool is_pod = std::is_pod<T>::value && !has_binary_read<T>::value>
	struct array_read_helper2
	{
		static inline void read(BinaryReader * br, T2 & container, unsigned count)
		{
			container.clear();
			container.reserve(count);
			for (unsigned i = 0; i < count; ++i)
				container.emplace_back(br->read<T>());
		}
	};

	template <class T2, class T>
	struct array_read_helper2 < T2, T, true >
	{
		static inline void read(BinaryReader * br, T2 & container, unsigned length)
		{
			if (length > 0)
			{
				const T *in = br->get<T>(length);
				container.resize(length);
				memcpy(&container[0], in, length * sizeof(T));
			}
		}
	};

	template <class T2>
	struct array_read_helper2 < T2, bool, true >
	{
		static inline void read(BinaryReader * br, T2 & container, unsigned length)
		{
			if (length > 0)
			{
				const unsigned char *in = br->get<unsigned char>(length / 8 + (length % 8 ? 1 : 0));
				container.resize(length);

				for (unsigned j = 0, k = 0; j < length; ++k)
				{
					for (unsigned i = 0; i < 8 && j < length; ++i, ++j)
						container[j] = (in[k] >> i) & 1;
				}
			}
		}
	};

	void _read(bool & value)
	{
		value = read<unsigned char>() != 0;
	}

	template <class T>
	void _read(std::unique_ptr<T> & value)
	{
		if (read<unsigned char>() != 0)
		{
            value = new T();
			_read(*value);
        }
	}

	template <class T>
	void _read(std::shared_ptr<T> & value)
	{
		if (read<unsigned char>() != 0)
		{
			value = std::make_shared<T>();
			_read(*value);
		}
	}

	void _read(BinaryWriter & bw);

	void _read(std::string & value)
	{
		unsigned length = read_length();
		const char *in = get<char>(length);
		value.assign(in, length);
	}

	template <class T>
	void _read(std::array<T, 0> & value)
	{
	}

	void _read(std::array<bool, 0> & value)
	{
	}


	template <class T, unsigned long length>	//t1
	void _read(std::array<T, length> & value)
	{
		array_read_helper<T>::read(this, &value[0], length);
	}

	template <unsigned long length>				//t1
	void _read(std::array<bool, length> & value)
	{
		const unsigned char *in = get<unsigned char>(length / 8 + (length % 8 > 0 ? 1 : 0));

		for (unsigned j = 0, k = 0; j < length; ++k)
		{
			for (unsigned i = 0; i < 8 && j < length; ++i, ++j)
				value[j] = ((in[k] >> i) & 1);
		}
	}

	template <class T>
	void _read(std::vector<T> & value)
	{
		unsigned length = read_length();

		array_read_helper2<std::vector<T>, T>::read(this, value, length);
	}

	void _read(std::vector<bool> & value)
	{
		unsigned length = read_length();

		value.reserve(length);
		const unsigned char *in = get<unsigned char>(length / 8 + (length % 8 > 0 ? 1 : 0));

		for (unsigned j = 0, k = 0; j < length; ++k)
		{
			for (unsigned i = 0; i < 8 && j < length; ++i, ++j)
				value.push_back((in[k] >> i) & 1);
		}
	}


	template <class T>
	void _read(std::list<T> & value)
	{
		unsigned length = read_length();

		for (unsigned i = 0; i < length; ++i)
			value.emplace_back(read<T>());
	}

	void _read(std::list<bool> & value)
	{
		unsigned length = read_length();

		const unsigned char *in = get<unsigned char>(length / 8 + (length % 8 > 0 ? 1 : 0));

		for (unsigned j = 0, k = 0; j < length; ++k)
		{
			for (unsigned i = 0; i < 8 && j < length; ++i, ++j)
				value.push_back((in[k] >> i) & 1);
		}
	}

	template <class T>
	void _read(std::set<T> & value)
	{
		unsigned length = read_length();

		for (unsigned i = 0; i < length; ++i)
			value.emplace(read<T>());
	}

	void _read(std::set<bool> & value)
	{
		unsigned length = read_length();
		const unsigned char *in = get<unsigned char>(length / 8 + (length % 8 > 0 ? 1 : 0));

		for (unsigned j = 0, k = 0; j < length; ++k)
		{
			for (unsigned i = 0; i < 8 && j < length; ++i, ++j)
				value.emplace((in[k] >> i) & 1);
		}
	}

	template <class T, class U>
	void _read(std::map<T, U> & value)
	{
		unsigned length = read_length();

		T v1;

		for (unsigned i = 0; i < length; ++i)
		{
			v1 = read<T>();
			value.emplace(std::move(v1), read<U>());
		}
	}

	template <class T>
	void _read(std::unordered_set<T> & value)
	{
		unsigned length = read_length();

		for (unsigned i = 0; i < length; ++i)
			value.emplace(read<T>());
	}

	void _read(std::unordered_set<bool> & value)
	{
		unsigned length = read_length();
		const unsigned char *in = get<unsigned char>(length / 8 + (length % 8 > 0 ? 1 : 0));

		for (unsigned j = 0, k = 0; j < length; ++k)
		{
			for (unsigned i = 0; i < 8 && j < length; ++i, ++j)
				value.emplace((in[k] >> i) & 1);
		}
	}

	template <class T, class U>
	void _read(std::unordered_map<T, U> & value)
	{
		unsigned length = read_length();

		T v1;
		for (unsigned i = 0; i < length; ++i)
		{
			v1 = read<T>();
			value.emplace(std::move(v1), read<U>());
		}
	}
	/*
	template <unsigned i, typename T, typename R, typename ... ARGS>
	struct read_call_helper;


	template <unsigned i, typename T, typename R, typename ARG, typename ... ARGS>
	struct read_call_helper<i, T, R, ARG, ARGS...>
	{
		template <typename ... F_ARGS>
		static _forceinline R call(BinaryReader & br, T f, F_ARGS && ... f_args)
		{
			typedef typename std::remove_const<typename std::remove_reference<ARG>::type>::type ARG_T;
			return read_call_helper<i - 1, T, R, ARGS...>::call(br, f, std::forward<F_ARGS>(f_args)..., br.read<ARG_T>());
		}
	};

	template <typename T, typename R>
	struct read_call_helper < 0, T, R>
	{
		template <typename ... F_ARGS>
		static _forceinline R call(BinaryReader & br, T f, F_ARGS && ... f_args)
		{
			return f(std::forward<F_ARGS>(f_args)...);
		}
	};

	template <typename T>
	struct read_call_helper < 0, T, void>
	{
		template <typename ... F_ARGS>
		static _forceinline void call(BinaryReader & br, T f, F_ARGS && ... f_args)
		{
			f(std::forward<F_ARGS>(f_args)...);
		}
	};

	template <unsigned i, typename O, typename T, typename R, typename ... ARGS>
	struct read_call_helper2;


	template <unsigned i, typename O, typename T, typename R, typename ARG, typename ... ARGS>
	struct read_call_helper2<i, O, T, R, ARG, ARGS...>
	{
		template <typename ... F_ARGS>
		static _forceinline R call(BinaryReader & br, O & o, T f, F_ARGS && ... f_args)
		{
			typedef typename std::remove_const<typename std::remove_reference<ARG>::type>::type ARG_TYPE;
			return read_call_helper2<i - 1, O, T, R, ARGS...>::call(br, o, f, std::forward<F_ARGS>(f_args)..., br.read<ARG_TYPE>());
		}
	};

	template <typename O, typename T>
	struct read_call_helper2 < 0, O, T, void>
	{
		template <typename ... F_ARGS>
		static _forceinline void call(BinaryReader & br, O & o, T f, F_ARGS && ... f_args)
		{
			(o.*f)(std::forward<F_ARGS>(f_args)...);
		}
	};

	template <typename O, typename T, typename R>
	struct read_call_helper2 < 0, O, T, R>
	{
		template <typename ... F_ARGS>
		static _forceinline R call(BinaryReader & br, O & o, T f, F_ARGS && ... f_args)
		{
			return (o.*f)(std::forward<F_ARGS>(f_args)...);
		}
	};
	*/

	template <bool can_check, bool outmost, typename ... ARGS>
	struct multi_read_helper;

	template <typename ARG, typename ... ARGS>
	struct multi_read_helper<true, true, ARG, ARGS...>
	{
		static inline void read(BinaryReader & br, ARG & data, ARGS & ... args)
		{
			if (br.data_size - br.pos < sum_size<ARGS...>::value)
				throw UnexpectedEndOfData();

			br._read_uc(data);
			multi_read_helper<true, false, ARGS...>::read(br, args...);
		}
	};

	template <typename ARG, typename ... ARGS>
	struct multi_read_helper<true, false, ARG, ARGS...>
	{
		static inline void read(BinaryReader & br, ARG & data, ARGS & ... args)
		{
			br._read_uc(data);
			multi_read_helper<true, false, ARGS...>::read(br, args...);
		}
	};

	template <bool outmost, typename ARG, typename ... ARGS>
	struct multi_read_helper<false, outmost, ARG, ARGS...>
	{
		static inline void read(BinaryReader & br, ARG & data, ARGS & ... args)
		{
			ARG value = br.read<ARG>();
			multi_read_helper<detail::uc_read_e<ARGS...>::value, detail::uc_read_e<ARGS...>::value, ARGS...>::read(br, args...);
			data = std::move(value);
		}
	};

	template <typename ARG>
	struct multi_read_helper<true, true, ARG>
	{
		static inline void read(BinaryReader & br, ARG & data)
		{
			br._read(data);
		}
	};

	template <typename ARG>
	struct multi_read_helper<false, true, ARG>
	{
		static inline void read(BinaryReader & br, ARG & data)
		{
			br._read(data);
		}
	};

	template <bool can_check, bool outmost>
	struct multi_read_helper<can_check, outmost>
	{
		static inline void read(BinaryReader & br)
		{
		}
	};


public:
	/*
	template <class T>
	inline void read(T & value)
	{
		_read(value);
	}
	*/
	template <typename ... ARGS>
	inline void read(ARGS & ... args)
	{
		multi_read_helper<detail::uc_read_e<ARGS...>::value, true, typename std::remove_reference<ARGS>::type...>::read(*this, args...);
	}

	template <class T>
	inline T read()
	{
		T value = nullinit<T>();
		_read(value);
		return value;
	}

	template <class T>
	inline T peek()
	{
		T value = nullinit<T>();
		unsigned pos1 = tell();
		try
		{
			_read(value);
		}
		catch (...)
		{
			seek(pos1);
			throw;
		}
		seek(pos1);
		return value;
	}

	template <class T>
	bool conditional_read(T & value)
	{
		bool condition = read<unsigned char>() != 0;
		if (condition)
			_read(value);
		return condition;
	}

	template <class T>
	_forceinline BinaryReader & operator >> (T & value)
	{
		_read(value);
		return *this;
	}

	template <class T>
	_forceinline BinaryReader & operator & (T & value)
	{
		_read(value);
		return *this;
	}
	/*
	template <typename R, typename ... F_ARGS>
	R read_call(R(*f)(F_ARGS...))
	{
		assert(f != 0);
		return read_call_helper<sizeof...(F_ARGS), decltype(f), R, F_ARGS...>::template call<>(*this, f);
	}

	template <typename R, class O, typename ... F_ARGS>
	R read_call(O & o, R(O::*f)(F_ARGS...))
	{
		assert(f != 0);
		return read_call_helper2<sizeof...(F_ARGS), O, decltype(f), R, F_ARGS...>::template call<>(*this, o, f);
	}
	*/
	template <class T>
	void readAt(unsigned pos, T & value)
	{
		static_assert(std::is_pod<T>::value, "T is not a POD type");
		static_assert(!std::is_pointer<T>::value, "T is a pointer");

		if (data_size - pos < sizeof(T))
			throw UnexpectedEndOfData();
		value = *reinterpret_cast<const T*>(&data[pos]);
		//		pos += sizeof(T);
	}

	template <class T>
	T readAt(unsigned pos)
	{
		static_assert(std::is_pod<T>::value, "T is not a POD type");
		static_assert(!std::is_pointer<T>::value, "T is a pointer");

		if (data_size - pos < sizeof(T))
			throw UnexpectedEndOfData();
		return *reinterpret_cast<const T*>(&data[pos]);
	}

	template <class T>
	static bool is_aligned(unsigned pos)
	{
		return pos % alignof(T) == 0;
	}

	template <class T>
	bool is_aligned() const
	{
		return pos % alignof(T) == 0;
	}

	template <class T>
	inline void readArray(T * const & data, unsigned const & count)
	{
		for (unsigned i = 0; i < count; ++i)
			read(data[i]);
	}

};


std::ostream & operator << (std::ostream & o, BinaryReader const & br);

class BinaryWriter
{
public:
	std::vector<char>  data;


	BinaryWriter()
	{
		data.reserve(1024);
	}

	BinaryWriter(BinaryReader const & br)
	{
		if (br.data_size > 0)
		{
			data.resize(br.data_size);
			memcpy(&data[0], br.data, br.data_size);
		}
	}

	BinaryWriter(const char * _data, unsigned len)
		: data(_data, _data + len)
	{
	}

	void clear()
	{
		data.clear();
	}

	inline unsigned tell() const
	{
		return data.size();
	}

	void write_length(unsigned len);

	template <class T>
	T* allocate(unsigned count)
	{
		static_assert(std::is_pod<T>::value, "BinaryWriter::Allocate must be called on POD types");
		//		assert(count > 0);
		if (count > 0)
		{
			data.resize(data.size() + sizeof(T) * count);
			return reinterpret_cast<T*>(&data[data.size() - sizeof(T) * count]);
		}
		else
			return nullptr;
	}


	template <class T>
	inline T& allocate()
	{
		return *allocate<T>(1);
	}

	template <class T>
	unsigned allocate_pos(unsigned count)
	{
		static_assert(std::is_pod<T>::value, "BinaryWriter::Allocate must be called on POD types");
		//		assert(count > 0);
		if (count > 0)
		{
			data.resize(data.size() + sizeof(T) * count);
			return data.size() - sizeof(T) * count;
		}
		else
			return ~0;
	}

	template <class T>
	inline unsigned allocate_pos()
	{
		return allocate_pos<T>(1);
	}


private:

	template <class T, bool is_reflectable = false/*::is_reflectable<T>::value*/, bool is_pod = std::is_pod<T>::value, bool has_bw = has_binary_write<T>::value, bool is_arr = std::is_array<T>::value>
	struct write_helper
	{
		static _forceinline void write(BinaryWriter & bw, T const & value)
		{
			value.operator <<(bw);
		}
	};

	template <class T, bool is_reflectable, bool is_pod>
	struct write_helper < T, is_reflectable, is_pod, true, false >
	{
		static _forceinline void write(BinaryWriter & bw, T const & value)
		{
			value.operator <<(bw);
		}
	};

	template <class T>
	struct write_helper < T, true, false, false, false >
	{
		static _forceinline void write(BinaryWriter & bw, T const & value)
		{
			bw._write_reflectable(value);
		}
	};

	template <class T, bool is_reflectable>
	struct write_helper < T, is_reflectable, true, false, false >
	{
		static _forceinline void write(BinaryWriter & bw, T const & value)
		{
			bw.data.resize(bw.data.size() + sizeof(T));
			*reinterpret_cast<T*>(&bw.data[bw.data.size() - sizeof(T)]) = value;
		}
	};

	template <bool is_reflectable>
	struct write_helper < unsigned long, is_reflectable, true, false, false >
	{
		static void write(BinaryWriter & bw, unsigned long const & value);	//unsigned long check
	};

	template <class T, bool is_reflectable, bool is_pod, bool has_bw>
	struct write_helper < T, is_reflectable, is_pod, has_bw, true >
	{
		static _forceinline void write(BinaryWriter & bw, T const & value)
		{
			bw.writeArray(&value[0], std::extent<T>::value);
		}
	};

	template <class T>
	void _forceinline _write(T const & value)
	{
		write_helper<T>::write(*this, value);
	}

	void _write(unsigned long const & value);	//unsigned long check

	template <unsigned i, unsigned e, class T>
	struct write_reflectable
	{
		_forceinline static void write(BinaryWriter & br, T const & value)
		{
//			br.write(ref_get<i>(value));
//			write_reflectable<i + 1, e, T>::write(br, value);
		}
	};

	template <unsigned i, class T>
	struct write_reflectable<i, i, T>
	{
		_forceinline static void write(BinaryWriter & br, T const & value)
		{
		}
	};



	template <class T>
	void _forceinline _write_reflectable(T const & value)
	{
		write_reflectable<0, T::memberCount, T>::write(*this, value);
	}



	template <unsigned i, unsigned e, typename ... ARGS>
	struct write_tuple
	{
		_forceinline static void write(BinaryWriter & br, std::tuple<ARGS...> const & value)
		{
			br.write(std::get<i>(value));
			write_tuple<i + 1, e, ARGS...>::write(br, value);
		}
	};

	template <unsigned i, typename ... ARGS>
	struct write_tuple<i, i, ARGS...>
	{
		_forceinline static void write(BinaryWriter & br, std::tuple<ARGS...> const & value)
		{
		}
	};

	template <typename ... ARGS>
	inline void _write(std::tuple<ARGS...> const & value)
	{
		write_tuple<0, sizeof...(ARGS), ARGS...>::write(*this, value);
	}

	template <class T, class U>
	inline void _write(std::pair<T, U> const & value)
	{
		_write(value.first);
		_write(value.second);
	}

	template <class T>
	inline void _write(std::atomic<T> const & value)
	{
		T temp = value;
		_write(temp);
	}

	template <class T, bool is_pod = std::is_pod<T>::value && !has_binary_write<T>::value, bool dummy = false>
	struct array_write_helper
	{
		static inline void write(BinaryWriter * bw, T const * data, unsigned count)
		{
			for (unsigned i = 0; i < count; ++i)
				bw->_write(data[i]);
		}

		template <class IT>
		static inline void write(BinaryWriter * bw, IT first, unsigned count)
		{
			for (unsigned i = 0; i < count; ++first, ++i)
				bw->_write(*first);
		}
	};

	template <class T, bool dummy>
	struct array_write_helper < T, true, dummy >
	{
		static inline void write(BinaryWriter * bw, T const * data, unsigned length)
		{
			T * out = bw->allocate<T>(length);
			memcpy(out, data, length * sizeof(T));
		}

		template <class IT>
		static inline void write(BinaryWriter * bw, IT first, unsigned count)
		{
			T * out = bw->allocate<T>(count);
			for (unsigned i = 0; i < count; ++first, ++i)
				out[i] = *first;
		}
	};

	template <bool dummy>
	struct array_write_helper < bool, true, dummy >
	{
		template <class IT>
		static inline void write(BinaryWriter * bw, IT first, unsigned count)
		{
			unsigned char * out = bw->allocate<unsigned char>(count / 8 + (count % 8 > 0 ? 1 : 0));
			unsigned i, j;
			unsigned char v;

			for (j = 0; j < count;)
			{
				v = 0;
				for (i = 0; i < 8 && j < count; ++j, ++i, ++first)
					v |= (int)*first << i;
				out[j] = v;
			}
		}
	};

	void _write(bool const & value)
	{
		_write<unsigned char>(value ? 1 : 0);
	}

	template <class T>
	void _write(std::unique_ptr<T> const & value)
	{
		_write<unsigned char>(value ? 1 : 0);
		if (value)
			_write(*value);
	}

	template <class T>
	void _write(std::shared_ptr<T> const & value)
	{
		_write<unsigned char>(value ? 1 : 0);
		if (value)
			_write(*value);
	}

	void _write(const char * value)
	{
		unsigned length = strlen(value);
		write_length(length);

		char * out = allocate<char>(length);
		memcpy(out, value, length);
	}

	void _write(BinaryReader const & br)
	{
		unsigned length = br.data_size;
		write_length(length);

		char * out = allocate<char>(length);
		memcpy(out, br.data, length);
	}

	inline void _write(BinaryWriter const & bw)
	{
		_write(bw.data);
	}

	void _write(std::string const & value)
	{
		unsigned length = value.length();
		write_length(length);

		char * out = allocate<char>(length);
		memcpy(out, value.data(), length);
	}

	template <class T>
	void _write(std::array<T, 0> const & value)
	{
	}

	void _write(std::array<bool, 0> const & value)
	{
	}

	template <class T, unsigned long length> //1
	void _write(std::array<T, length> const & value)
	{
		array_write_helper<typename std::remove_const<T>::type>::write(this, &value[0], length);
	}

	template <unsigned long length>	//1
	void _write(std::array<bool, length> const & value)
	{
		array_write_helper<bool>::write(this, value.begin(), length);
	}


	template <class T>
	void _write(std::vector<T> const & value)
	{
		write_length(value.size());
		array_write_helper<typename std::remove_const<T>::type>::write(this, value.data(), value.size());
	}

	void _write(std::vector<bool> const & value)
	{
		write_length(value.size());
		array_write_helper<bool>::write(this, value.begin(), value.size());
	}

	template <class T>
	void _write(std::list<T> const & value)
	{
		write_length(value.size());
		array_write_helper<typename std::remove_const<T>::type>::write(this, value.begin(), value.size());
	}

	template <class T>
	void _write(std::set<T> const & value)
	{
		write_length(value.size());
		array_write_helper<typename std::remove_const<T>::type>::write(this, value.begin(), value.size());
	}

	template <class T, class U>
	void _write(std::map<T, U> const & value)
	{
		write_length(value.size());
		array_write_helper<std::pair<T, U> >::write(this, value.begin(), value.size());
	}

	template <class T>
	void _write(std::unordered_set<T> const & value)
	{
		write_length(value.size());
		array_write_helper<typename std::remove_const<T>::type>::write(this, value.begin(), value.size());
	}

	template <class T, class U>
	void _write(std::unordered_map<T, U> const & value)
	{
		write_length(value.size());
		array_write_helper<std::pair<T, U> >::write(this, value.begin(), value.size());
	}
/*
	template <unsigned i, unsigned j, unsigned dummy, typename ... ARGS>
	struct write_call_helper;


	template <unsigned i, unsigned j, unsigned dummy, typename ARG, typename ... ARGS>
	struct write_call_helper<i, j, dummy, ARG, ARGS...>
	{
        void fckyou(){}
		template <typename ... F_ARGS>
		static _forceinline void write(BinaryWriter & br, ARG && arg, ARGS && ... args)
		{
			br.write<typename get_vararg<i, F_ARGS...>::type>(arg);
			write_call_helper<i + 1, j, dummy, ARGS...>::write (br, std::forward<ARGS>(args)...);
		}
	};

	template <unsigned j, unsigned dummy>
	struct write_call_helper < j, j, dummy >
	{
        void fckyou(){}
		template <typename ... F_ARGS>
		static _forceinline void write(BinaryWriter & br)
		{
		}
	};
*/
	template <int dummy, typename ... ARGS>
	struct reserve_helper;

	template <int dummy, typename ARG, typename ... ARGS>
	struct reserve_helper<dummy, ARG, ARGS...>
	{
		static inline void reserve(BinaryWriter & bw, unsigned count, ARG const & arg, ARGS const & ... args)
		{
			reserve_helper<dummy, ARGS...>::reserve(bw, count + detail::stream_size<typename std::remove_reference<ARG>::type>::size(arg), args...);
		}
	};
	template <int dummy>
	struct reserve_helper<dummy>
	{
		static inline void reserve(BinaryWriter & bw, unsigned count)
		{
			bw.data.reserve(bw.data.size() + count);
		}
	};
	/*
	template <typename ARG, typename ... ARGS>
	inline void multi_write(ARG const & arg, ARGS const & ... args)
	{
		_write(arg);
		_multi_write(args...);
	}
	*/
	template <bool outmost, typename ... ARGS>
	struct multi_write_helper;

	template <typename ARG, typename ... ARGS>
	struct multi_write_helper<false, ARG, ARGS...>
	{
		static inline void write(BinaryWriter & bw, ARG const & data, ARGS const & ... args)
		{
			bw._write(data);
			multi_write_helper<false, ARGS...>::write(bw, args...);
		}
	};

	template <typename ARG, typename ... ARGS>
	struct multi_write_helper<true, ARG, ARGS...>
	{
		static inline void write(BinaryWriter & bw, ARG const & data, ARGS const & ... args)
		{
			reserve_helper<0, ARG, ARGS...>::reserve(bw, 0, data, args...);
			bw._write(data);
			multi_write_helper<false, ARGS...>::write(bw, args...);
		}
	};

	template <typename ARG>
	struct multi_write_helper<true, ARG>
	{
		static inline void write(BinaryWriter & bw, ARG const & data)
		{
			bw._write(data);
		}
	};

	template <bool outmost>
	struct multi_write_helper<outmost>
	{
		static inline void write(BinaryWriter & bw)
		{
		}
	};

public:
	/*
	template <class T>
	inline void write(T const & value)
	{
		_write(value);
	}
	*/
	template <typename ... ARGS>
	inline void write(ARGS const & ... args)
	{
		//reserve_helper<ARGS...>::reserve(*this);
		//_multi_write(args...);
		multi_write_helper<true, typename std::remove_reference<ARGS>::type...>::write(*this, args...);
	}

	template <class T, typename ... ARGS>
	inline void emplace(ARGS && ... args)
	{
		char * p = this->allocate<char>(sizeof(T));
		new(p) T(std::forward<ARGS>(args)...);
	}

	template <class T>
	inline void conditional_write(bool condition, T const & value)
	{
		_write<unsigned char>(condition ? 1 : 0);
		if (condition)
			_write(value);
	}

	void write_strn(std::string const & str, unsigned count)
	{
		char * p = allocate<char>(count);
		unsigned i = 0;
		for (; i < str.size() && i < count; ++i)
			p[i] = str[i];
		for (; i < count; ++i)
			p[i] = 0;
	}

	void write_charn(char ch, unsigned count)
	{
		char * p = allocate<char>(count);
		for (unsigned i = 0; i < count; ++i)
			p[i] = ch;
	}

	template <class T>
	_forceinline BinaryWriter & operator << (T const & value)
	{
		_write(value);
		return *this;
	}

	template <class T>
	_forceinline BinaryWriter & operator & (T const & value)
	{
		_write(value);
		return *this;
	}
	/*
	template <typename ... F_ARGS, typename ... ARGS>
	void write_call(void(*f)(F_ARGS...), ARGS && ... args)
	{
		static_assert(sizeof...(F_ARGS) == sizeof...(ARGS), "Wrong number of paramters at call serialization");
		write_call_helper<0, sizeof...(ARGS), 0, ARGS...>::write(*this, std::forward<ARGS>(args)...);
	}

	template <class O, typename ... F_ARGS, typename ... ARGS>
	void write_call(void(O::*f)(F_ARGS...), ARGS && ... args)
	{
		static_assert(sizeof...(F_ARGS) == sizeof...(ARGS), "Wrong number of paramters at call serialization");
		write_call_helper<0, sizeof...(ARGS), 0, ARGS...>::write(*this, std::forward<ARGS>(args)...);
	}
	*/
	template <class T>
	void readAt(unsigned pos, T & value)
	{
		static_assert(std::is_pod<T>::value, "T is not a POD type");
		static_assert(!std::is_pointer<T>::value, "T is a pointer");

		if (data.size() - pos < sizeof(T))
			throw UnexpectedEndOfData();
		value = *reinterpret_cast<const T*>(&data[pos]);
	}

	template <class T>
	T readAt(unsigned pos)
	{
		static_assert(std::is_pod<T>::value, "T is not a POD type");
		static_assert(!std::is_pointer<T>::value, "T is a pointer");

		if (data.size() - pos < sizeof(T))
			throw UnexpectedEndOfData();
		return *reinterpret_cast<const T*>(&data[pos]);
	}


	template <class T>
	void writeAt(unsigned pos, T const & value)
	{
		static_assert(std::is_pod<T>::value, "T is not a POD type");

		if (data.size() - pos < sizeof(T))
			throw UnexpectedEndOfData();

		*reinterpret_cast<T*>(&data[pos]) = value;
	}

	template <class T>
	T & at(unsigned pos)
	{
		static_assert(std::is_pod<T>::value, "T is not a POD type");
		static_assert(!std::is_pointer<T>::value, "T is a pointer");

		if (data.size() - pos < sizeof(T))
			throw UnexpectedEndOfData();
		return *reinterpret_cast<T*>(&data[pos]);
	}

	template <class T>
	static bool is_aligned(unsigned pos)
	{
		return pos % alignof(T) == 0;
	}

	template <class T>
	bool is_aligned() const
	{
		return data.size() % alignof(T) == 0;
	}

	template <class T>
	inline BinaryWriter & writeArray(T const * const data, unsigned const count)
	{
		array_write_helper<typename std::remove_reference<typename std::remove_const<T>::type>::type>::write(this, data, count);
		return *this;
	}

	template <class IT>
	inline BinaryWriter & writeArray(IT const it, unsigned const count)
	{
		typedef decltype(*it) T;
		array_write_helper<typename std::remove_reference<typename std::remove_const<T>::type>::type>::write(this, it, count);
		return *this;
	}
};

std::ostream & operator << (std::ostream & o, BinaryWriter & bw);

template <class T>
void stream_array<T>::operator << (BinaryWriter & bw) const
{
	bw.write_length(size());
	bw.writeArray(reinterpret_cast<const T*>(pos + 4), size());
}

template <class T>
void stream_array<T>::operator >> (BinaryReader & br)
{
	pos = &br.data[br.tell()];
	unsigned s = br.read_length();
	br.skip(s * sizeof(T));
}

template <class T, unsigned count>
void fixed_stream_array<T, count>::operator << (BinaryWriter & bw) const
{
	bw.writeArray(reinterpret_cast<const T*>(pos), size());
}

template <class T, unsigned count>
void fixed_stream_array<T, count>::operator >> (BinaryReader & br)
{
	pos = &br.data[br.tell()];
	br.skip(size() * sizeof(T));
}
template <class T>
void iterator_pair<T>::operator << (BinaryWriter & bw) const
{
	unsigned len = size();
	bw.write_length(len);
	bw.writeArray(b, len);
}

template <class T>
void iterator_pair<T>::operator >> (BinaryReader & br)
{
	unsigned len = br.read_length();
	if (br.data_size - br.pos < sizeof(T) * len)
		throw UnexpectedEndOfData();

	b = reinterpret_cast<value_type*>(&br.data[br.pos]);
	e = reinterpret_cast<value_type*>(&br.data[br.pos + len]);
}


inline bool operator== (std::string const & a, stream_array<char> const & b)
{
	if (a.size() == b.size())
		return strncmp(a.data(), b.data(), a.size()) == 0;
	else
		return false;
}

inline bool operator== (stream_array<char> const & a, std::string const & b)
{
	if (a.size() == b.size())
		return strncmp(a.data(), b.data(), a.size()) == 0;
	else
		return false;
}

inline bool operator!= (std::string const & a, stream_array<char> const & b)
{
	if (a.size() != b.size())
		return true;
	else
		return strncmp(a.data(), b.data(), a.size()) != 0;
}

inline bool operator!= (stream_array<char> const & a, std::string const & b)
{
	if (a.size() != b.size())
		return true;
	else
		return strncmp(a.data(), b.data(), a.size()) != 0;
}

template <unsigned len>
inline bool operator== (std::string const & a, fixed_stream_array<char, len> const & b)
{
	if (a.size() == b.size())
		return strncmp(a.data(), b.data(), a.size()) == 0;
	else
		return false;
}

template <unsigned len>
inline bool operator== (fixed_stream_array<char, len> const & a, std::string const & b)
{
	if (a.size() == b.size())
		return strncmp(a.data(), b.data(), a.size()) == 0;
	else
		return false;
}

template <unsigned len>
inline bool operator!= (std::string const & a, fixed_stream_array<char, len> const & b)
{
	if (a.size() != b.size())
		return true;
	else
		return strncmp(a.data(), b.data(), a.size()) != 0;
}

template <unsigned len>
inline bool operator!= (fixed_stream_array<char, len> const & a, std::string const & b)
{
	if (a.size() != b.size())
		return true;
	else
		return strncmp(a.data(), b.data(), a.size()) != 0;
}

template <unsigned len>
inline std::string to_string(fixed_stream_array<char, len> const & arr)
{
	unsigned i = 0;
	for (; i < len; ++i)
		if (arr[i] == 0)
			break;
	return std::string(arr.data(), i);
}

inline std::string to_string(stream_array<char> const & arr)
{
	unsigned i = 0;
	for (; i < arr.size(); ++i)
		if (arr[i] == 0)
			break;
	return std::string(arr.data(), i);
	return std::string(arr.data(), arr.size());
}
