#pragma once

////////////////////////////////////////////////////////////////////////////////
//  Headers
////////////////////////////////////////////////////////////////////////////////

#include "PCH.h"
#include "Core/Constants.h"

////////////////////////////////////////////////////////////////////////////////
/// STD FUNCTIONS
////////////////////////////////////////////////////////////////////////////////
namespace std
{
	////////////////////////////////////////////////////////////////////////////////
	// Represent an INI-esque data structure with key-value pairs organized into blocks.
	using unordered_pairs_in_blocks = map<string, map<string, string>>;
	using ordered_pairs_in_blocks = map<string, vector<pair<string, string>>>;

	////////////////////////////////////////////////////////////////////////////////
	// String type detection helper
	namespace is_string_impl
	{
		template <typename T>       struct is_string : false_type {};
		template <>                 struct is_string<char*> : true_type {};
		template <typename... Args> struct is_string<basic_string            <Args...>> : true_type {};
		template <typename... Args> struct is_string<basic_string_view       <Args...>> : true_type {};
	}

	// type trait to utilize the implementation type traits as well as decay the type
	template <typename T> struct is_string
	{
		static constexpr bool const value = is_string_impl::is_string<decay_t<T>>::value;
	};

	////////////////////////////////////////////////////////////////////////////////
	// Iomanip operator detection helper
	namespace is_iomanip_impl
	{
		// type trait to utilize the implementation type traits as well as decay the type
		template <typename T> struct is_iomanip
		{
			//static constexpr bool const value = is_iomanip_impl::is_iomanip<decay_t<T>>::value;
			static constexpr bool const value =
				is_same<T, decltype(setw(0))>::value ||
				is_same<T, decltype(setbase(0))>::value ||
				is_same<T, decltype(setiosflags(ios::showbase))>::value ||
				is_same<T, decltype(resetiosflags(ios::showbase))>::value;
		};
	}

	// type trait to utilize the implementation type traits as well as decay the type
	template <typename T> struct is_iomanip
	{
		static constexpr bool const value = is_iomanip_impl::is_iomanip<decay_t<T>>::value;
	};

	////////////////////////////////////////////////////////////////////////////////
	// Container type detection helper
	namespace is_container_impl
	{
		template <typename T>       struct is_container :false_type {};
		template <typename T, size_t N> struct is_container<array    <T, N>> :true_type {};
		template <typename... Args> struct is_container<vector            <Args...>> :true_type {};
		template <typename... Args> struct is_container<deque             <Args...>> :true_type {};
		template <typename... Args> struct is_container<list              <Args...>> :true_type {};
		template <typename... Args> struct is_container<forward_list      <Args...>> :true_type {};
		template <typename... Args> struct is_container<set               <Args...>> :true_type {};
		template <typename... Args> struct is_container<multiset          <Args...>> :true_type {};
		template <typename... Args> struct is_container<map               <Args...>> :true_type {};
		template <typename... Args> struct is_container<multimap          <Args...>> :true_type {};
		template <typename... Args> struct is_container<unordered_set     <Args...>> :true_type {};
		template <typename... Args> struct is_container<unordered_multiset<Args...>> :true_type {};
		template <typename... Args> struct is_container<unordered_map     <Args...>> :true_type {};
		template <typename... Args> struct is_container<unordered_multimap<Args...>> :true_type {};
		template <typename... Args> struct is_container<stack             <Args...>> :true_type {};
		template <typename... Args> struct is_container<queue             <Args...>> :true_type {};
		template <typename... Args> struct is_container<priority_queue    <Args...>> :true_type {};
	}

	// type trait to utilize the implementation type traits as well as decay the type
	template <typename T> struct is_container
	{
		static constexpr bool const value = is_container_impl::is_container<decay_t<T>>::value;
	};

	////////////////////////////////////////////////////////////////////////////////
	// Iterator detection helper
	template<typename T, typename = void>
	struct is_iterator
	{
		static constexpr bool value = false;
	};

	template<typename T>
	struct is_iterator<T, typename enable_if<!is_same<typename iterator_traits<T>::value_type, void>::value>::type>
	{
		static constexpr bool value = true;
	};

	////////////////////////////////////////////////////////////////////////////////
	// Check for whether a certain type has serialization and deserialization operators
	namespace has_iostream_operator_impl
	{
		// Fallback struct
		struct false_case {};

		// Default fallback operators 
		// (T1, ..., are dummy arguments to avoid ambiguity in case the actual operator is also a template)
		// This is based on the following rule for overload resolution:
		//   "F1 is determined to be a better function than F2 if implicit conversions for all arguments of F1 
		//    are not worse than the implicit conversions for all arguments of F2, and if F1 and F2 
		//    are both template specializations and F1 is more specialized according to the partial ordering
		//    rules for template specializations"
		// Source: https://en.cppreference.com/w/cpp/language/overload_resolution
		template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5> false_case operator<< (ostream& stream, T const&);
		template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5> false_case operator>> (istream& stream, T&);

		// Test struct for operator>>(stream, val)
		template<typename T>
		struct test_in_struct
		{
			enum { value = !is_same<decltype(declval<istream&>() >> declval<T&>()), false_case>::value };
		};

		// Test struct for operator<<(stream, val)
		template<typename T>
		struct test_out_struct
		{
			enum { value = !is_same<decltype(declval<ostream&>() << declval<T const&>()), false_case>::value };
		};
	}

	template<typename T>
	struct has_istream_operator
	{
		static constexpr bool value = has_iostream_operator_impl::test_in_struct<T>::value;
	};

	template<typename T>
	struct has_ostream_operator
	{
		static constexpr bool value = has_iostream_operator_impl::test_out_struct<T>::value;
	};

	////////////////////////////////////////////////////////////////////////////////
	// Check for whether a certain type has serialization and deserialization operators
	namespace is_trivially_callable_impl
	{
		template <typename T>
		constexpr bool is_trivially_callable_impl(typename enable_if<bool(sizeof((declval<T>()(), 0)))>::type*)
		{
			return true;
		}

		template<typename T>
		constexpr bool is_trivially_callable_impl(...)
		{
			return false;
		}

		template<typename T>
		constexpr bool is_trivially_callable()
		{
			return is_trivially_callable_impl::is_trivially_callable_impl<T>(nullptr);
		}
	}

	template<typename T>
	struct is_trivially_callable
	{
		static constexpr bool value = is_trivially_callable_impl::is_trivially_callable<T>();
	};

	////////////////////////////////////////////////////////////////////////////////
	/** Helper functions to easily access the first and second elements of a pair. */
	const auto pair_first = [](auto const& pair) -> auto const& { return pair.first; };
	const auto pair_second = [](auto const& pair) -> auto const& { return pair.second; };

	////////////////////////////////////////////////////////////////////////////////
	/** ostream operator for iterator pairs. */
	template<typename T>
	ostream& printRange(ostream& stream, T begin, T end)
	{
		for (T it = begin; it != end; ++it)
		{
			if (it != begin) stream << ",\t";
			stream << (*it);
		}
		return stream;
	}

	////////////////////////////////////////////////////////////////////////////////
	/** ostream operator for pairs. */
	template<typename T1, typename T2>
	ostream& operator<<(ostream& stream, pair<T1, T2> const& container)
	{
		stream << "(" << container.first << ", " << container.second << ")";
		return stream;
	}

	////////////////////////////////////////////////////////////////////////////////
	/** ostream operator for arrays. */
	template<typename T, size_t N>
	ostream& operator<<(ostream& stream, array<T, N> const& container)
	{
		stream << "[";
		printRange(stream, container.begin(), container.end());
		stream << "]";
		return stream;
	}

	////////////////////////////////////////////////////////////////////////////////
	/** ostream operator for vectors. */
	template<typename T>
	ostream& operator<<(ostream& stream, vector<T> const& container)
	{
		stream << "[";
		printRange(stream, container.begin(), container.end());
		stream << "]";
		return stream;
	}

	////////////////////////////////////////////////////////////////////////////////
	/** ostream operator for sets. */
	template<typename T>
	ostream& operator<<(ostream& stream, set<T> const& container)
	{
		stream << "{";
		printRange(stream, container.begin(), container.end());
		stream << "}";
		return stream;
	}

	////////////////////////////////////////////////////////////////////////////////
	/** ostream operator for sets. */
	template<typename T>
	ostream& operator<<(ostream& stream, unordered_set<T> const& container)
	{
		stream << "{";
		printRange(stream, container.begin(), container.end());
		stream << "}";
		return stream;
	}

	////////////////////////////////////////////////////////////////////////////////
	/** ostream operator for maps. */
	template<typename K, typename V>
	ostream& operator<<(ostream& stream, map<K, V> const& container)
	{
		stream << "{";
		printRange(stream, container.begin(), container.end());
		stream << "}";
		return stream;
	}

	////////////////////////////////////////////////////////////////////////////////
	/** ostream operator for maps. */
	template<typename K, typename V>
	ostream& operator<<(ostream& stream, unordered_map<K, V> const& container)
	{
		stream << "{";
		printRange(stream, container.begin(), container.end());
		stream << "}";
		return stream;
	}
	////////////////////////////////////////////////////////////////////////////////
	/** ostream operator for variants. */
	template<typename... Ts, typename enable_if<sizeof...(Ts) != 0, int>::type = 0>
	ostream& operator<<(ostream& stream, variant<Ts...> const& variant)
	{
		return stream << visit([](auto const& val) { return to_string(val); }, variant);
	}

	////////////////////////////////////////////////////////////////////////////////
	template<typename T, typename enable_if<has_ostream_operator<T>::value, int>::type = 0>
	string to_string(T const& val)
	{
		stringstream ss;
		ss << val;
		return ss.str();
	}

	////////////////////////////////////////////////////////////////////////////////
	template<typename T, typename enable_if<has_istream_operator<T>::value, int>::type = 0>
	T from_string(string const& str)
	{
		istringstream iss(str);
		T result;
		iss >> result;
		return result;
	}

	////////////////////////////////////////////////////////////////////////////////
	template<typename Vs, typename Vd>
	Vd get_or(Vs const& variant, Vd def)
	{
		return variant.index() == 0 ? def : get<Vd>(variant);
	}

	////////////////////////////////////////////////////////////////////////////////
	template<typename T>
	vector<T> iota(size_t count, T start = 0)
	{
		vector<T> result(count);
		iota(result.begin(), result.end(), start);
		return result;
	}

	////////////////////////////////////////////////////////////////////////////////
	template<typename T>
	inline T exp256(T x)
	{
		x = 1.0 + x / 256.0;
		x *= x; x *= x; x *= x; x *= x;
		x *= x; x *= x; x *= x; x *= x;
		return x;
	}

	////////////////////////////////////////////////////////////////////////////////
	template<typename T>
	inline T exp1024(T x)
	{
		x = 1.0 + x / 1024;
		x *= x; x *= x; x *= x; x *= x;
		x *= x; x *= x; x *= x; x *= x;
		x *= x; x *= x;
		return x;
	}

	////////////////////////////////////////////////////////////////////////////////
	int pow(int base, int exp);

	////////////////////////////////////////////////////////////////////////////////
	/** Function for generating bitmasks from a series of bit ids. */
	template<typename T>
	static constexpr size_t bit_mask(T id)
	{
		return (1 << id);
	}

	////////////////////////////////////////////////////////////////////////////////
	/** Function for generating bitmasks from a series of bit ids. */
	template<typename T>
	static constexpr size_t bit_mask(initializer_list<T> ids)
	{
		size_t result = 0;
		for (auto it = ids.begin(); it != ids.end(); ++it)
		{
			result = result | (1 << *it);
		}
		return result;
	}

	////////////////////////////////////////////////////////////////////////////////
	unsigned number_of_digits(unsigned i);

	////////////////////////////////////////////////////////////////////////////////
	// compute the next highest power of 2 of 32-bit v
	size_t next_pow2(size_t v);

	////////////////////////////////////////////////////////////////////////////////
	int64_t factorial(int64_t n);

	////////////////////////////////////////////////////////////////////////////////
	double round_to_digits(double val, int accuracy);

	////////////////////////////////////////////////////////////////////////////////
	namespace resize_N_impl
	{
		////////////////////////////////////////////////////////////////////////////////
		template<typename C, typename S>
		void resizeN(C& container, S size)
		{
			container.resize(size);
		}

		////////////////////////////////////////////////////////////////////////////////
		template<typename C, typename S, typename... S_rest>
		void resizeN(C& container, S size, S_rest... sizes)
		{
			container.resize(size);

			for (size_t i = 0; i < size; ++i)
			{
				resizeN(container[i], sizes...);
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	template<typename C, typename... S>
	void resizeN(C& container, S... sizes)
	{
		resize_N_impl::resizeN(container, sizes...);
	}

	////////////////////////////////////////////////////////////////////////////////
	vector<const char*> to_cstr(vector<string> const& values);

	////////////////////////////////////////////////////////////////////////////////
	template<typename T>
	vector<string> meta_enum_names(T const& meta)
	{
		vector<string> result(meta.members.size());

		for (size_t i = 0; i < result.size(); ++i)
		{
			result[i] = string(meta.members[i].name.begin(), meta.members[i].name.end());
		}

		return result;
	}

	////////////////////////////////////////////////////////////////////////////////
	template<typename T>
	vector<string> meta_enum_names(T const& meta, vector<typename T::EnumType> const& enums)
	{
		vector<string> result(enums.size());

		for (size_t i = 0; i < result.size(); ++i)
			for (size_t j = 0; j < meta.members.size(); ++j)
			{
				if (meta.members[j].value == enums[i])
				{
					result[i] = string(meta.members[j].name.begin(), meta.members[j].name.end());
					break;
				}
			}

		return result;
	}

	////////////////////////////////////////////////////////////////////////////////
	inline bool string_replace_first(string& str, string const& from, string const& to)
	{
		size_t start_pos = str.find(from);
		if (start_pos == string::npos) return false;
		str.replace(start_pos, from.length(), to);
		return true;
	}

	////////////////////////////////////////////////////////////////////////////////
	inline string string_replace_first(string const& str, string const& from, string const& to)
	{
		string result = str;
		string_replace_first(result, from, to);
		return result;
	}

	////////////////////////////////////////////////////////////////////////////////
	inline void string_replace_all(string& str, string const& from, string const& to)
	{
		if (from.empty()) return;

		size_t start_pos = 0;
		while ((start_pos = str.find(from, start_pos)) != string::npos)
		{
			str.replace(start_pos, from.length(), to);
			start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	inline string string_replace_all(string const& str, string const& from, string const& to)
	{
		string result = str;
		string_replace_all(result, from, to);
		return result;
	}

	////////////////////////////////////////////////////////////////////////////////
	template<typename It, typename T>
	inline string string_join(string const& separator, It begin, It end, T const& transform)
	{
		// Make sure we are not dealing with an empty range
		if (begin == end) return string();

		// Add the first element
		stringstream ss;
		ss << transform(*begin);
		++begin;

		// Append the tail of the list
		for (; begin != end; ++begin)
			ss << separator << transform(*begin);

		// Return the result
		return ss.str();
	}

	////////////////////////////////////////////////////////////////////////////////
	template<typename It>
	inline string string_join(string const& separator, It begin, It end)
	{
		// Make sure we are not dealing with an empty range
		if (begin == end) return string();

		// Add the first element
		stringstream ss;
		ss << *begin;
		++begin;

		// Append the tail of the list
		for (; begin != end; ++begin)
			ss << separator << *begin;

		// Return the result
		return ss.str();
	}

	////////////////////////////////////////////////////////////////////////////////
	template<typename T>
	void atomic_min(atomic<T>& min_value, T value)
	{
		T prev_value = min_value;
		while (prev_value > value && min_value.compare_exchange_weak(prev_value, value) == false)
			;
	}

	////////////////////////////////////////////////////////////////////////////////
	template<typename T>
	void atomic_max(atomic<T>& max_value, T value)
	{
		T prev_value = max_value;
		while (prev_value < value && max_value.compare_exchange_weak(prev_value, value) == false)
			;
	}

	////////////////////////////////////////////////////////////////////////////////
	template<typename K, typename V>
	vector<K> map_keys(unordered_map<K, V> const& map)
	{
		vector<K> result(map.size());
		size_t idx = 0;
		for (auto const& elem : map)
			result[idx++] = elem.first;
		return result;
	}

	////////////////////////////////////////////////////////////////////////////////
	template<typename K, typename V>
	vector<K> map_values(unordered_map<K, V> const& map)
	{
		vector<K> result(map.size());
		size_t idx = 0;
		for (auto const& elem : map)
			result[idx++] = elem.second;
		return result;
	}

	////////////////////////////////////////////////////////////////////////////////
	template<typename C>
	C random_samples(C const& container, const size_t num_samples)
	{
		// Early out if we have less samples than needed
		if (container.size() <= num_samples) return container;

		// Make a random set of indices
		random_device rd;
		mt19937 gen(rd());
		vector<size_t> ids = iota<size_t>(container.size(), 0);
		shuffle(ids.begin(), ids.end(), gen);

		// Return the first n indices
		C result = C(num_samples);
		transform(ids.begin(), ids.begin() + num_samples, result.begin(), [&](const size_t idx) { return container[idx]; });
		return result;
	}
}