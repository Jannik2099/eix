// vim:set noet cinoptions= sw=4 ts=4:
// This file is part of the eix project and distributed under the
// terms of the GNU General Public License v2.
//
// Copyright (c)
//   Wolfgang Frisch <xororand@users.sourceforge.net>
//   Emil Beinroth <emilbeinroth@gmx.net>
//   Martin Väth <vaeth@mathematik.uni-wuerzburg.de>

#ifndef EIX__STRINGUTILS_H__
#define EIX__STRINGUTILS_H__ 1

#include <config.h>

#include <string>
#include <vector>
#include <set>
#include <map>
#include <cstring>
#include <cstdlib>
#include <cctype>

#define UNUSED(p) ((void)(p))

#ifndef HAVE_STRNDUP
/** strndup in case we don't have one. */
#include <unistd.h>
char *strndup(const char *s, size_t n);
#endif

#ifdef HAVE_STRTOUL
#define my_atoi(a) strtoul(a, NULL, 10)
#else
#ifdef HAVE_STRTOL
#define my_atoi(a) strtol(a, NULL, 10)
#else
#define my_atoi(a) atoi(a)
#endif
#endif

/** Spaces for split strings */
extern const char *spaces;

/** Split names of Atoms in different ways. */
class ExplodeAtom {

	public:

		static const char *get_start_of_version(const char* str);

		/** Get the version-string of a Atom (e.g. get 1.2.3 from foobar-1.2.3).  */
		static char *split_version(const char* str);

		/** Get the name-string of a Atom (e.g. get foobar from foobar-1.2.3).  */
		static char *split_name(const char* str);

		/** Get name and version from a Atom (e.g. foobar and 1.2.3 from foobar-1.2.3).
		 * @warn You'll get a pointer to a static array of 2 pointer to char. */
		static char **split(const char* str);
};

/** Check string if it only contains digits. */
inline bool
is_numeric(const char *str)
{
	while( (*str) )
		if( !isdigit(*str++) )
			return false;
	return true;
}

/** Add symbol if it is not already the last one */
inline void
optional_append(std::string &s, char symbol)
{
	if(s.empty() || ((*(s.rbegin()) != symbol)))
		s.append(1, symbol);
}

/** Trim characters on left side of string.
 * @param str String that should be trimmed
 * @param delims characters that should me removed
 * @return trimmed string */
inline void
ltrim(std::string *str, const char *delims = spaces)
{
	// trim leading whitespace
	std::string::size_type notwhite = str->find_first_not_of(delims);
	if(notwhite != std::string::npos)
		str->erase(0, notwhite);
	else
		str->clear();
}

/** Trim characters on right side of string.
 * @param str String that should be trimmed
 * @param delims characters that should me removed
 * @return trimmed string */
inline void
rtrim(std::string *str, const char *delims = spaces)
{
	// trim trailing whitespace
	std::string::size_type  notwhite = str->find_last_not_of(delims);
	if(notwhite != std::string::npos)
		str->erase(notwhite+1);
	else
		str->clear();
}

/** Trim characters on left and right side of string.
 * @param str String that should be trimmed
 * @param delims characters that should me removed
 * @return trimmed string */
inline void
trim(std::string *str, const char *delims = spaces)
{
	ltrim(str, delims);
	rtrim(str, delims);
}

/** Escape all "at" and "\" characters in string. */
void escape_string(std::string &str, const char *at = spaces);

/** Split a string into multiple strings.
 * @param str Reference to the string that should be splitted.
 * @param at  Split at the occurrence of any these characters.
 * @param ignore_empty  Remove empty strings from the result.
 * @param handle_escape Do not split at escaped characters from "at" symbols,
 *                      removing escapes for \\ or "at" symbols from result.
 * @return    vector of strings. */
std::vector<std::string> split_string(const std::string &str, const bool handle_escape = false, const char *at = spaces, const bool ignore_empty = true);

/** Join a string-vector.
 * @param glue glue between the elements. */
std::string join_vector(const std::vector<std::string> &vec, const std::string &glue = " ");

/** Join a string-set
 * @param glue glue between the elements. */
std::string join_set(const std::set<std::string> &vec, const std::string &glue = " ");

/** Resolve a vector of -/+ keywords and store the result as a set.
 * If we find a -keyword we look for a (+)keyword. If one ore more (+)keywords
 * are found, they (and the -keyword) are removed.
 * @param obsolete_minus   If true do not treat -* special and keep -keyword.
 * @param warnminus        Set if there was -keyword which did not apply for
 * @param warnignore
 * @return true            if -* is contained */
bool resolve_plus_minus(std::set<std::string> &s, const std::vector<std::string> &l, bool obsolete_minus, bool *warnminus = NULL, const std::set<std::string> *warnignore = NULL);

/** Sort and unique. Return true if there were double entries */
#ifdef UNIQUE_WORKS

#include <algorithm>

template<typename T>
bool sort_uniquify(T &v, bool vector_is_ignored = false)
{
	std::sort(v.begin(), v.end());
	typename T::iterator i = std::unique(v.begin(), v.end());
	if(i == v.end())
		return false;
	if(! vector_is_ignored)
		v.erase(i, v.end());
	return true;
}

#else

template<typename T>
bool sort_uniquify(std::vector<T> &v, bool vector_is_ignored = false)
{
	std::set<T> s(v.begin(), v.end());
	if(! vector_is_ignored) {
		v.clear();
		v.insert(v.end(), s.begin(), s.end());
	}
	return (s.size() != v.size());
}

#endif

/// Add items from s to the end of d.
template<typename T>
void push_backs(std::vector<T> &d, const std::vector<T> &s)
{
	d.insert(d.end(), s.begin(), s.end());
}

/// Insert a whole vector to a set.
template<typename T>
void insert_list(std::set<T> &the_set, const std::vector<T> &the_list)
{
	the_set.insert(the_list.begin(), the_list.end());
}

/// Make a set from a vector.
template<typename T>
void make_set(std::set<T> &the_set, const std::vector<T> &the_list)
{
	the_set.clear();
	insert_list(the_set, the_list);
}


/** Make a vector from a set. */
template<typename T>
void make_vector(std::vector<T> &the_list, const std::set<T> &the_set)
{
	the_list.clear();
	for(typename std::set<T>::const_iterator it(the_set.begin()),
			it_end(the_set.end());
		it != it_end; ++it)
	{
		the_list.push_back(*it);
	}
}

class StringHash : public std::vector<std::string>
{
	public:
		StringHash(bool will_hash = true) : hashing(will_hash), finalized(false)
		{ }

		void init(bool will_hash)
		{
			hashing = will_hash; finalized = false;
			clear(); str_map.clear();
		}

		void finalize();

		void store_string(const std::string &s);
		void store_words(const std::vector<std::string> &v);
		void store_words(const std::string &s)
		{ hash_words(split_string(s)); }

		void hash_string(const std::string &s);
		void hash_words(const std::vector<std::string> &v);
		void hash_words(const std::string &s)
		{ hash_words(split_string(s)); }

		StringHash::size_type get_index(const std::string &s) const;

		void output(const char *s = NULL) const;

		const std::string& operator[](StringHash::size_type i) const;
	private:
		bool hashing, finalized;
		std::map<std::string, StringHash::size_type> str_map;
};



#endif /* EIX__STRINGUTILS_H__ */
