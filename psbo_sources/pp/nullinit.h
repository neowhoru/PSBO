#pragma once
#include <string.h>
#include <tuple>
#include <type_traits>
#include <array>

namespace nullinit_helper
{
	template <class T>
	struct nullinit_class
	{
		static constexpr inline T init()
		{
			return T();
		}
	};

	template <class A, class B>
	struct nullinit_class<std::pair<A, B> >
	{
		static constexpr inline std::pair<A, B> init();
	};

	template <typename... ARGS>
	struct nullinit_class<std::tuple<ARGS...> >
	{
		static constexpr inline std::tuple<ARGS...> init();
	};


	template <class T, unsigned L>
	struct nullinit_class<std::array<T, L> >
	{
		static constexpr inline std::array<T, L> init();
	};

	template <class T, bool is_scalar = std::is_scalar<T>::value, bool is_struct = std::is_class<T>::value && std::is_pod<T>::value>
	struct nullinit_c1
	{
		static constexpr inline T init()
		{
			return nullinit_class<T>::init();
		}
	};

	template <class T, bool is_struct>
	struct nullinit_c1<T, true, is_struct>
	{
		static constexpr inline T init()
		{
			return (T)0;
		}
	};

	template <class T>
	struct nullinit_c1<T, false, true>
	{
		static inline T init()
		{
			T data;
			memset(&data, 0, sizeof(T));
			return data;
		}
	};

};

template <class T>
struct nullinit_ex
{
	static constexpr inline T init()
	{
		return nullinit_helper::nullinit_c1<typename std::remove_reference<T>::type>::init();
	}
};



template <class T>
constexpr inline T nullinit()
{
	return nullinit_ex<T>::init();
}


namespace nullinit_helper
{


	template <class A, class B>
	constexpr inline std::pair<A, B> nullinit_class<std::pair<A, B> >::init()
	{
		return std::pair<A, B>(nullinit<A>(), nullinit<B>());
	}

	template <typename... ARGS>
	constexpr inline std::tuple<ARGS...> nullinit_class<std::tuple<ARGS...> >::init()
	{
		return std::make_tuple(nullinit<ARGS>() ...);
	}


	template <class T, unsigned L, bool is_pod = std::is_pod<T>::value>
	struct array_nullinit
	{
		static constexpr inline std::array<T, L> init()
		{
			return std::array<T, L>();
		}
	};

	template <class T, unsigned L>
	struct array_nullinit<T, L, true>
	{
		static inline std::array<T, L> init()
		{
			std::array<T, L> r;
			memset(&r[0], 0, sizeof(T)* L);
			return r;
		}
	};

	template <class T>
	struct array_nullinit<T, 0, true>
	{
		static constexpr inline std::array<T, 0> init()
		{
			return std::array<T, 0>();
		}
	};

	template <class T, unsigned L>
	inline constexpr std::array<T, L> nullinit_class<std::array<T, L> >::init()
	{
		return array_nullinit<T, L>::init();
	}


};
