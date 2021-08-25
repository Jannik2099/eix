// vim:set noet cinoptions= sw=4 ts=4:
// This file is part of the eix project and distributed under the
// terms of the GNU General Public License v2.
//
// Copyright (c)
//   Wolfgang Frisch <xororand@users.sourceforge.net>
//   Emil Beinroth <emilbeinroth@gmx.net>
//   Martin Väth <martin@mvath.de>

#ifndef SRC_EIXTK_UTILS_H_
#define SRC_EIXTK_UTILS_H_ 1

#include <config.h>  // IWYU pragma: keep

#include <string>

#include "eixTk/attribute.h"
#include "eixTk/eixint.h"
#include "eixTk/null.h"
#include "eixTk/stringtypes.h"

/**
scandir which even works on poor man's systems.
We keep the original type for the callback function
(including possible case distinctions whether its argument is const)
for the case that we *have* to use scandir() for the implementation
on some systems (which however is rather unlikely)
**/
struct dirent;
#define SCANDIR_ARG3 const struct dirent *
typedef int (*select_dirent)(SCANDIR_ARG3 dir_entry);
ATTRIBUTE_NONNULL_ bool scandir_cc(const std::string& dir, WordVec *namelist, select_dirent select, bool sorted);
ATTRIBUTE_NONNULL_ inline static bool scandir_cc(const std::string& dir, WordVec *namelist, select_dirent select);
inline static bool scandir_cc(const std::string& dir, WordVec *namelist, select_dirent select) {
	return scandir_cc(dir, namelist, select, true);
}

/**
push_back every line of file or dir into v.
**/
ATTRIBUTE_NONNULL((1, 2)) bool pushback_lines(const char *file, LineVec *v, bool recursive, bool keep_empty, eix::SignedBool keep_comments, std::string *errtext);
ATTRIBUTE_NONNULL_ inline static bool pushback_lines(const char *file, LineVec *v, bool recursive, bool keep_empty, eix::SignedBool keep_comments);
inline static bool pushback_lines(const char *file, LineVec *v, bool recursive, bool keep_empty, eix::SignedBool keep_comments) {
	return pushback_lines(file, v, recursive, keep_empty, keep_comments, NULLPTR);
}
ATTRIBUTE_NONNULL_ inline static bool pushback_lines(const char *file, LineVec *v, bool recursive, bool keep_empty);
inline static bool pushback_lines(const char *file, LineVec *v, bool recursive, bool keep_empty) {
	return pushback_lines(file, v, recursive, keep_empty, 0);
}
ATTRIBUTE_NONNULL_ inline static bool pushback_lines(const char *file, LineVec *v, bool recursive);
inline static bool pushback_lines(const char *file, LineVec *v, bool recursive) {
	return pushback_lines(file, v, recursive, true);
}
ATTRIBUTE_NONNULL_ inline static bool pushback_lines(const char *file, LineVec *v);
inline static bool pushback_lines(const char *file, LineVec *v) {
	return pushback_lines(file, v, false);
}

/**
List of files in directory.
Pushed names of file in directory into string-vector if they don't match any
char * in given exlude list.
@param dir_path Path to directory
@param into pointer to WordVec .. files get append here (with full path)
@param exlude list of char * that don't need to be put into vector
@param only_type: if 1: consider only ordinary files, if 2: consider only dirs, if 3: consider only files or dirs
@param no_hidden ignore hidden files
@param full_path return full pathnames
@return true if everything is ok. Nonexisting directory is not ok.
**/
ATTRIBUTE_NONNULL((2)) bool pushback_files(const std::string& dir_path, WordVec *into, const char *const exclude[], unsigned char only_files, bool no_hidden, bool full_path);

/**
Files excluded for pushback_lines in recursive mode
**/
extern const char *pushback_files_recurse_exclude[];

/**
Recursive list of files in directory
*/
ATTRIBUTE_NONNULL((4)) bool pushback_files_recurse(const std::string& dir_path, const std::string& subpath, int depth, WordVec *into, bool full_path, std::string *errtext);
ATTRIBUTE_NONNULL((2)) inline static bool pushback_files_recurse(const std::string& dir_path, WordVec *into, bool full_path, std::string *errtext);
ATTRIBUTE_NONNULL((2)) inline static bool pushback_files_recurse(const std::string& dir_path, WordVec *into, bool full_path, std::string *errtext) {
	return pushback_files_recurse(dir_path, "", 0, into, full_path, errtext);
}

/**
Print version of eix to stdout.
**/
ATTRIBUTE_NORETURN void dump_version();

#endif  // SRC_EIXTK_UTILS_H_
