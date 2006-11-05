/***************************************************************************
 *   eix is a small utility for searching ebuilds in the                   *
 *   Gentoo Linux portage system. It uses indexing to allow quick searches *
 *   in package descriptions with regular expressions.                     *
 *                                                                         *
 *   https://sourceforge.net/projects/eix                                  *
 *                                                                         *
 *   Copyright (c)                                                         *
 *     Wolfgang Frisch <xororand@users.sourceforge.net>                    *
 *     Emil Beinroth <emilbeinroth@gmx.net>                                *
 *     Martin V�th <vaeth@mathematik.uni-wuerzburg.de>                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "../config.h"

#include <global.h>
#include <set>

#include <output/formatstring.h>
#include <output/formatstring-print.h>

#include <portage/conf/portagesettings.h>

#include <eixTk/argsreader.h>
#include <eixTk/stringutils.h>
#include <eixTk/ansicolor.h>
#include <eixTk/utils.h>
#include <eixTk/filenames.h>

#include <cli.h>

#include <database/header.h>
#include <database/package_reader.h>

#include <signal.h> /* signal handlers */
#include <vector>
#include <stack>

#include <stdio.h>
#include <unistd.h>

using namespace std;

/** The name under which we have been called. */
const char *program_name = NULL;

int  is_current_dbversion(const char *filename);
void print_vector(const vector<string> &vec);
void print_unused(const char *filename, const eix::ptr_list<Package> &packagelist, bool test_empty = false);
void print_removed(const string &dirname, const eix::ptr_list<Package> &packagelist);

/** Show a short help screen with options and commands. */
static void
dump_help(int exit_code)
{
	printf( "Usage: %s [options] EXPRESSION\n"
			"\n"
			"Search for packages in the index generated by update-eix.\n"
			"EXPRESSION is true or false. Packages for which the EXPRESSION gives true,\n"
			"are included in the final report.\n"
			"\n"
			"A EXPRESSION can be:\n"
			"    EXPRESSION [-o|-a] EXPRESSION\n"
			"    [local-options] PATTERN\n"
			"\n"
			"Global:\n"
			"   Exclusive options:\n"
			"     -h, --help            show this screen and exit\n"
			"     -V, --version         show version and exit\n"
			"     --dump                dump variables to stdout\n"
			"     --dump-defaults       dump default values of variables\n"
			"\n"
			"   Special:\n"
			"     -t  --test-non-matching Before other output, print non-matching entries\n"
			"                           of /etc/portage/package.* and non-matching names\n"
			"                           of installed packages; this option is best\n"
			"                           combined with -T to clean up /etc/portage/package.*\n"
			"     -Q, --quick (toggle)  don't read unguessable slots of installed packages\n"
			"         --care            always read slots of installed packages\n"
			"\n"
			"   Output:\n"
			"     -q, --quiet (toggle)   (no) output\n"
			"     -n, --nocolor          do not use ANSI color codes\n"
			"     -F, --force-color      force colorful output\n"
			"     -c, --compact (toggle) compact search results\n"
			"     -v, --verbose (toggle) verbose search results\n"
			"     -x, --versionsort  (toggle) sort output by slots/versions\n"
			"     -l, --versionlines (toggle) print available versions line-by-line\n"
			"         --format           format string for normal output\n"
			"         --format-compact   format string for compact output\n"
			"         --format-verbose   format string for verbose output\n"
			"\n"
			"Local:\n"
			"  Miscellaneous:\n"
			"    -I, --installed       Next expression only matches installed packages.\n"
			"    -i, --multi-installed Match packages installed in several versions.\n"
			"    -d, --dup-packages    Match duplicated packages.\n"
			"    -D, --dup-versions    Match packages with duplicated versions.\n"
			"    -1, --slotted         Match packages with a nontrivial slot.\n"
			"    -2, --slots           Match packages with two different slots.\n"
			"    -u, --update          Match packages without best slotted version.\n"
			"    -O, --overlay         Match packages from overlays.\n"
			"    -T, --test-obsolete   Match packages with obsolete entries in\n"
			"                          /etc/portage/package.* (see man eix).\n"
			"                          Use -t to check non-existing packages.\n"
			"    -!, --not (toggle)    Invert the expression.\n"
			"    -|, --pipe            Use input from pipe of emerge -pv\n"
			"\n"
			"  Search Fields:\n"
			"    -S, --description     description\n"
			"    -A, --category-name   \"category/name\"\n"
			"    -C, --category        category\n"
			"    -s, --name            name (default)\n"
			"    -H, --homepage        homepage\n"
			"    -L, --license         license\n"
			"    -P, --provide         provides\n"
			"\n"
			"  Type of Pattern:\n"
			"    -r, --regex           Pattern is a regexp (default)\n"
			"    -e, --exact           Pattern is the exact string\n"
			"    -p, --pattern         Pattern is a wildcards-pattern\n"
			"    -f [m], --fuzzy [m]   Use fuzzy-search with a max. levenshtein-distance m.\n"
			"                          (default: "LEVENSHTEIN_DISTANCE_STR")\n"
			"\n"
			"You can contact the developers in #eix on irc.freenode.net or on\n"
			"the sourceforge-page "PACKAGE_BUGREPORT".\n"
			"There is also a wiki at "EIX_WIKI".\n"
			"This program is covered by the GNU General Public License. See COPYING for\n"
			"further information.\n",
		program_name);

	if(exit_code != -1) {
		exit(exit_code);
	}
}

/** On segfault: show some instructions to help us find the bug. */
void
sig_handler(int sig)
{
	if(sig == SIGSEGV)
		fprintf(stderr,
				"Received SIGSEGV - you probably found a bug in eix.\n"
				"Please proceed with the following few instructions and help us find the bug:\n"
				" * install gdb (sys-dev/gdb)\n"
				" * compile eix with FEATURES=\"nostrip\" CXXFLAGS=\"-g -ggdb3\"\n"
				" * enter gdb with \"gdb --args %s your_arguments_for_eix\"\n"
				" * type \"run\" and wait for the segfault to happen\n"
				" * type \"bt\" to get a backtrace (this helps us a lot)\n"
				" * post a bugreport and be sure to include the output from gdb ..\n"
				"\n"
				"Sorry for the inconvenience and thanks in advance!\n", program_name);
	exit(1);
}

/*	If you want to add a new parameter to eix just insert a line into
 *	long_options. If you only want a longopt, add a new define.
 *
 *	-- ebeinroth
 */

enum cli_options {
	O_FMT = 256,
	O_FMT_VERBOSE,
	O_FMT_COMPACT,
	O_DUMP,
	O_DUMP_DEFAULTS,
	O_CARE,
	O_IGNORE_ETC_PORTAGE,
	O_CURRENT,
	O_DEBUG
};

const char *format_normal, *format_verbose, *format_compact;
char overlay_mode;

PrintFormat format(get_package_property, print_package_property);

/** Local options for argument reading. */
static struct LocalOptions {
	bool be_quiet,
		 quick,
		 care,
		 verbose_output,
		 compact_output,
		 show_help,
		 show_version,
		 dump_eixrc,
		 dump_defaults,
		 test_unused,
		 do_debug,
		 ignore_etc_portage,
		 is_current;
} rc_options;

/** Arguments and shortopts. */
static struct Option long_options[] = {
	// Global options
	Option("quiet",        'q',     Option::BOOLEAN,       &rc_options.be_quiet),
	Option("quick",        'Q',     Option::BOOLEAN,       &rc_options.quick),
	Option("care",         O_CARE,  Option::BOOLEAN_T,     &rc_options.care),

	Option("nocolor",      'n',     Option::BOOLEAN_T,     &format.no_color),
	Option("force-color",  'F',     Option::BOOLEAN_F,     &format.no_color),
	Option("versionlines", 'l',     Option::BOOLEAN,       &format.style_version_lines),
	Option("versionsort",  'x',     Option::BOOLEAN,       &format.slot_sorted),

	Option("verbose",      'v',     Option::BOOLEAN,       &rc_options.verbose_output),
	Option("compact",      'c',     Option::BOOLEAN,       &rc_options.compact_output),
	Option("help",         'h',     Option::BOOLEAN_T,     &rc_options.show_help),
	Option("version",      'V',     Option::BOOLEAN_T,     &rc_options.show_version),
	Option("dump",         O_DUMP,  Option::BOOLEAN_T,     &rc_options.dump_eixrc),
	Option("dump-defaults",O_DUMP_DEFAULTS,Option::BOOLEAN_T,&rc_options.dump_defaults),
	Option("test-non-matching",'t', Option::BOOLEAN_T,     &rc_options.test_unused),
	Option("debug",        O_DEBUG, Option::BOOLEAN_T,     &rc_options.do_debug),

	Option("is-current",   O_CURRENT, Option::BOOLEAN_T,   &rc_options.is_current),

	Option("ignore-etc-portage",  O_IGNORE_ETC_PORTAGE, Option::BOOLEAN_T,  &rc_options.ignore_etc_portage),

	Option("format",         O_FMT,         Option::STRING,   &format_normal),
	Option("format-verbose", O_FMT_VERBOSE, Option::STRING,   &format_verbose),
	Option("format-compact", O_FMT_COMPACT, Option::STRING,   &format_compact),

	// Options for criteria
	Option("installed",     'I'),
	Option("multi-installed",'i'),
	Option("slotted",       '1'),
	Option("slots",         '2'),
	Option("update",        'u'),
	Option("overlay",       'O'),
	Option("dup-packages",  'd'),
	Option("dup-versions",  'D'),
	Option("test-obsolete", 'T'),
	Option("not",           '!'),
	Option("pipe",          '|'),

	// Algorithms for a criteria
	Option("fuzzy",         'f'),
	Option("regex",         'r'),
	Option("exact",         'e'),
	Option("pattern",       'p'),

	// What to match in this criteria
	Option("name",          's'),
	Option("category",      'C'),
	Option("category-name", 'A'),
	Option("description",   'S'),
	Option("license",       'L'),
	Option("homepage",      'H'),
	Option("provide",       'P'),

	// What to do with the next one
	Option("or",            'o'),
	Option("and",           'a'),

	Option(0 , 0)
};

/** Setup default values for all global variables. */
void
setup_defaults()
{
	EixRc &rc = get_eixrc();

	// Setup defaults
	(void) memset(&rc_options, 0, sizeof(rc_options));

	rc_options.quick           = rc.getBool("QUICKMODE");
	rc_options.be_quiet        = rc.getBool("QUIETMODE");
	rc_options.care            = rc.getBool("CAREMODE");

	format_verbose             = (char*) rc["FORMAT_VERBOSE"].c_str();
	format_compact             = (char*) rc["FORMAT_COMPACT"].c_str();
	format_normal              = (char*) rc["FORMAT"].c_str();
	string s                   = rc["DEFAULT_FORMAT"];
	if((strcasecmp(s.c_str(), "FORMAT_VERBOSE") == 0) ||
	   (strcasecmp(s.c_str(), "verbose") == 0)) {
		rc_options.verbose_output = true;
	}
	else if((strcasecmp(s.c_str(), "FORMAT_COMPACT") == 0) ||
	   (strcasecmp(s.c_str(), "compact") == 0)) {
		rc_options.compact_output = true;
	}
	format.dateFormat          = rc["FORMAT_INSTALLATION_DATE"];
	format.dateFormatShort     = rc["FORMAT_SHORT_INSTALLATION_DATE"];
	format.instUseFormat       = rc["FORMAT_INSTALLED_USE"];

	format.color_masked        = rc["COLOR_MASKED"];
	format.color_unstable      = rc["COLOR_UNSTABLE"];
	format.color_stable        = rc["COLOR_STABLE"];
	format.color_overlaykey    = rc["COLOR_OVERLAYKEY"];
	format.color_virtualkey    = rc["COLOR_VIRTUALKEY"];
	format.color_slots         = rc["COLOR_SLOTS"];

	format.no_color            = !isatty(1) && !rc.getBool("FORCE_USECOLORS");
	format.color_original      = rc.getBool("COLOR_ORIGINAL");
	format.color_local_mask    = rc.getBool("COLOR_LOCAL_MASK");
	format.mark_installed      = rc["MARK_INSTALLED"];
	format.mark_version        = rc["MARK_VERSIONS"];
	format.show_slots          = rc.getBool("PRINT_SLOTS");
	format.style_version_lines = rc.getBool("STYLE_VERSION_LINES");
	format.slot_sorted         = !rc.getBool("STYLE_VERSION_SORTED");
	format.colon_slots         = rc.getBool("COLON_SLOTS");
	format.colored_slots       = rc.getBool("COLORED_SLOTS");
	format.recommend_local     = rc.getBool("RECOMMEND_ALWAYS_LOCAL");

	format.tag_for_profile            = rc["TAG_FOR_PROFILE"];
	format.tag_for_masked             = rc["TAG_FOR_MASKED"];
	format.tag_for_ex_profile         = rc["TAG_FOR_EX_PROFILE"];
	format.tag_for_ex_masked          = rc["TAG_FOR_EX_MASKED"];
	format.tag_for_locally_masked     = rc["TAG_FOR_LOCALLY_MASKED"];
	format.tag_for_stable             = rc["TAG_FOR_STABLE"];
	format.tag_for_unstable           = rc["TAG_FOR_UNSTABLE"];
	format.tag_for_minus_asterisk     = rc["TAG_FOR_MINUS_ASTERISK"];
	format.tag_for_minus_keyword      = rc["TAG_FOR_MINUS_KEYWORD"];
	format.tag_for_missing_keyword    = rc["TAG_FOR_MISSING_KEYWORD"];
	format.tag_for_other              = rc["TAG_FOR_OTHER"];
	format.tag_for_ex_unstable        = rc["TAG_FOR_EX_UNSTABLE"];
	format.tag_for_ex_minus_asterisk  = rc["TAG_FOR_EX_MINUS_ASTERISK"];
	format.tag_for_ex_minus_keyword   = rc["TAG_FOR_EX_MINUS_KEYWORD"];
	format.tag_for_ex_missing_keyword = rc["TAG_FOR_EX_MISSING_KEYWORD"];
	format.tag_for_ex_other           = rc["TAG_FOR_EX_OTHER"];

	Package::upgrade_to_best   = rc.getBool("UPGRADE_TO_HIGHEST_SLOT");

	string overlay = rc["OVERLAYS_LIST"];
	if(overlay.find("if") != string::npos)
		overlay_mode = 2;
	else if(overlay.find("number") != string::npos)
		overlay_mode = 0;
	else if(overlay.find("used") != string::npos)
		overlay_mode = 1;
	else if((overlay.find("no") != string::npos) ||
		(overlay.find("false") != string::npos))
		overlay_mode = 4;
	else
		overlay_mode = 3;
}

bool
print_overlay_table(PrintFormat &format, DBHeader &header, vector<bool> *overlay_used)
{
	bool printed_overlay = false;
	for(Version::Overlay i = 1;
		i != header.countOverlays();
		++i)
	{
		if(overlay_used)
			if(!((*overlay_used)[i-1]))
				continue;
		Version::Overlay ov_num = i;
		cout << format.overlay_keytext(i) << " ";
		cout << header.getOverlay(i) << endl;
		printed_overlay = true;
	}
	return printed_overlay;
}

int
run_eix(int argc, char** argv)
{
	const string var_db_pkg = "/var/db/pkg/";

	EixRc &eixrc = get_eixrc();

	// Setup defaults for all global variables like rc_options
	setup_defaults();

	// Read our options from the commandline.
	ArgumentReader argreader(argc, argv, long_options);

	// Only check if the versions uses the current layout
	if(rc_options.is_current) {
		return is_current_dbversion(EIX_CACHEFILE);
	}

	// Dump eixrc-stuff
	if(rc_options.dump_eixrc || rc_options.dump_defaults) {
		eixrc.dumpDefaults(stdout, rc_options.dump_defaults);
		exit(0);
	}

	// Show help screen
	if(rc_options.show_help) {
		dump_help(0);
	}

	// Show version
	if(rc_options.show_version) {
		dump_version(0);
	}

	// Honour a STFU
	if(rc_options.be_quiet) {
		close(1);
	}

	VarDbPkg varpkg_db(var_db_pkg.c_str(), !rc_options.quick, rc_options.care);

	PortageSettings portagesettings;
	MarkedList *marked_list = NULL;
	Matchatom *query = parse_cli(eixrc, varpkg_db, portagesettings, &marked_list, argreader.begin(), argreader.end());

	string varname;
	try {
		if(rc_options.verbose_output) {
			varname = "FORMAT_VERBOSE";
			format.setFormat(format_verbose);
		}
		else if(rc_options.compact_output) {
			varname = "FORMAT_COMPACT";
			format.setFormat(format_compact);
		}
		else {
			varname = "FORMAT";
			format.setFormat(format_normal);
		}
	}
	catch(ExBasic &e) {
		cout << "Problems while parsing " << varname << "." << endl
			<< e << endl;
		exit(1);
	}

	format.setupColors();

	eix::ptr_list<Package> matches;
	eix::ptr_list<Package> all_packages;


	/* Open database file */
	FILE *fp = fopen(EIX_CACHEFILE, "rb");
	if(!fp) {
		fprintf(stderr, "Can't open the database file "EIX_CACHEFILE" for reading (mode = 'rb')\n"
				"Did you forget to create it with 'update-eix'?\n");
		exit(1);
	}

	DBHeader header;

	io::read_header(fp, header);
	if(!header.isCurrent()) {
		fprintf(stderr, "Your database-file uses an obsolete format (%i, current is %i).\n"
				"Please run 'update-eix' and try again.\n", header.version, DBHeader::current);
		exit(1);
	}

	PackageReader reader(fp, header.size);
	while(reader.next())
	{
		if(query->match(&reader))
		{
			Package *release=reader.release();
			matches.push_back(release);
			if(rc_options.test_unused)
				all_packages.push_back(release);
		}
		else
		{
			if(rc_options.test_unused)
				all_packages.push_back(reader.release());
			else
				reader.skip();
		}
	}
	if(rc_options.test_unused)
	{
		bool empty = eixrc.getBool("TEST_FOR_EMPTY");
		cout << "\n";
		print_unused("/etc/portage/package.keywords", all_packages, empty);
		print_unused("/etc/portage/package.mask",     all_packages);
		print_unused("/etc/portage/package.unmask",   all_packages);
		print_unused("/etc/portage/package.use",      all_packages, empty);
		print_unused("/etc/portage/package.cflags",   all_packages, empty);
		print_removed(var_db_pkg, all_packages);
	}

	/* Sort the found matches by rating */
	if(FuzzyAlgorithm::sort_by_levenshtein()) {
		matches.sort(FuzzyAlgorithm::compare);
	}

	Keywords default_accepted_keywords = portagesettings.getAcceptKeywordsDefault();
	Keywords local_accepted_keywords;

	if(!eixrc.getBool("LOCAL_PORTAGE_CONFIG")) {
		rc_options.ignore_etc_portage = true;
	}
	else {
		if(!rc_options.ignore_etc_portage) {
			// No need to apply local settings for each test.
			// This saves a lot of time.
			format.recommend_local = false;
		}
		local_accepted_keywords = portagesettings.getAcceptKeywordsLocal();
	}

	format.set_marked_list(marked_list);
	if(overlay_mode != 0)
		format.set_overlay_translations(NULL);
	if(header.countOverlays())
	{
		format.clear_virtual(header.countOverlays());
		for(Version::Overlay i = 1; i != header.countOverlays(); i++)
			format.set_as_virtual(i, is_virtual(header.getOverlay(i).c_str()));
	}
	bool need_overlay_table = false;
	vector<bool> overlay_used(header.countOverlays(), false);
	for(eix::ptr_list<Package>::iterator it = matches.begin();
		it != matches.end();
		++it)
	{
		portagesettings.setStability(*it,  default_accepted_keywords, true);
		/* Add individual maskings from this machines /etc/portage/ */
		if(!rc_options.ignore_etc_portage) {
			portagesettings.user_config->setMasks(*it);
			portagesettings.user_config->setStability(*it, local_accepted_keywords);
		}

		if(it->largest_overlay)
		{
			need_overlay_table = true;
			if(overlay_mode <= 1)
			{
				for(Package::iterator ver = it->begin();
					ver != it->end(); ++ver)
				{
					Version::Overlay key = ver->overlay_key;
					if(key>0)
						overlay_used[key - 1] = true;
				}
			}
		}
		if(overlay_mode != 0)
			format.print(*it, &varpkg_db, &portagesettings);
	}
	switch(overlay_mode)
	{
		case 3: need_overlay_table = true;  break;
		case 4: need_overlay_table = false; break;
	}
	vector<Version::Overlay> overlay_num(header.countOverlays());
	if(overlay_mode == 0)
	{
		short i = 1;
		vector<bool>::iterator  uit = overlay_used.begin();
		vector<Version::Overlay>::iterator nit = overlay_num.begin();
		for(; uit != overlay_used.end(); ++uit, ++nit)
			if(*uit == true)
				*nit = i++;
		format.set_overlay_translations(&overlay_num);
		for(eix::ptr_list<Package>::iterator it = matches.begin();
			it != matches.end();
			++it)
			format.print(*it, &varpkg_db, &portagesettings);
	}
	bool printed_overlay = false;
	if(need_overlay_table)
	{
		printed_overlay = print_overlay_table(format, header,
			(overlay_mode <= 1)? &overlay_used : NULL);
	}

	if(!matches.size()) {
		if(eixrc.getBool("PRINT_COUNT_ALWAYS"))
			cout << "Found 0 matches.\n";
		else
			cout << "No matches found.\n";
	}
	else if(matches.size() == 1) {
		if(eixrc.getBool("PRINT_COUNT_ALWAYS"))
		{
			if(printed_overlay)
				cout << "\n";
			cout << "Found 1 match.\n";
		}
	}
	else {
		if(printed_overlay)
			cout << "\n";
		cout << "Found " << matches.size() << " matches.\n";
	}

	// Delete old query
	delete query;

	// Delete matches
	matches.delete_and_clear();

	return EXIT_SUCCESS;
}

int is_current_dbversion(const char *filename) {
	DBHeader header;
	FILE *fp = fopen(filename, "rb");
	if(!fp)
	{
		fprintf(stderr, "Can't open the database file %s for reading (mode = 'rb')\n"
				"Did you forget to create it with 'update-eix'?\n", filename);
		return 1;
	}
	io::read_header(fp, header);
	fclose(fp);

	return header.isCurrent() ? 0 : 1;
}

void print_vector(const vector<string> &vec)
{
	cout << ":\n\n";
	for(vector<string>::const_iterator it=vec.begin(); it != vec.end(); it++)
		cout << *it << endl;
	cout << "--\n\n";
}

void print_unused(const char *filename, const eix::ptr_list<Package> &packagelist, bool test_empty)
{
	vector<string> unused;
	vector<string> lines;
	pushback_lines(filename, &lines, false, true);
	for(unsigned int i = 0;
		i<lines.size();
		i++)
	{
		if(lines[i].size() == 0)
		{
			continue;
		}
		KeywordMask *m = NULL;

		try {
			string::size_type n = lines[i].find_first_of("\t ");
			if(n == string::npos) {
				if(test_empty)
				{
					unused.push_back(lines[i]);
					continue;
				}
				m = new KeywordMask(lines[i].c_str());
			}
			else {
				m = new KeywordMask(lines[i].substr(0, n).c_str());
			}
		}
		catch(ExBasic e) {
			portage_parse_error(filename, i, lines[i], e);
		}
		if(!m)
			continue;

		eix::ptr_list<Package>::const_iterator pi;
		for(pi = packagelist.begin(); pi != packagelist.end(); ++pi)
		{
			if(m->ismatch(**pi))
				break;
		}
		if(pi == packagelist.end())
		{
			unused.push_back(lines[i]);
		}
		delete m;
	}
	if(unused.empty())
		cout << "No non-matching";
	else
		cout << "Non-matching";
	if(test_empty)
		cout << " or empty";
	cout << " entries in " << filename;
	if(unused.empty())
	{
		cout << ".\n";
		return;
	}
	print_vector(unused);
	return;
}

void print_removed(const string &dirname, const eix::ptr_list<Package> &packagelist)
{
	/* For faster testing, we build a category->name set */
	map< string, set<string> > cat_name;
	for(eix::ptr_list<Package>::const_iterator pit = packagelist.begin();
		pit != packagelist.end(); ++pit )
		cat_name[pit->category].insert(pit->name);

	/* This will contain categories/packages to be printed */
	vector<string> failure;

	/* Read all installed packages (not versions!) and fill failures */
	vector<string> categories;
	pushback_files(dirname, categories, NULL, 2, true, false);
	for(vector<string>::const_iterator cit = categories.begin();
		cit != categories.end(); ++cit )
	{
		vector<string> names;
		string cat_slash = *cit + "/";
		pushback_files(dirname + cat_slash, names, NULL, 2, true, false);
		map< string, set<string> >::const_iterator cat = cat_name.find(*cit);
		const set<string> *ns = ( (cat == cat_name.end()) ? NULL : &(cat->second) );
		for(vector<string>::const_iterator nit = names.begin();
			nit != names.end(); ++nit )
		{
			const char *name;
			try { name = ExplodeAtom::split_name(nit->c_str()); }
			catch(ExBasic &e) { name = new char[1]; }
			if(ns)
				if(ns->find(name) != ns->end())
					continue;
			failure.push_back(cat_slash + name);
			delete name;
		}
	}
	if(failure.empty())
	{
		cout << "The names of all installed packages are in the database.\n\n";
		return;
	}
	cout << "The following installed packages are not in the database";
	print_vector(failure);
	return;
}

int main(int argc, char** argv)
{
	program_name = argv[0];

	/* Install signal handler */
	signal(SIGSEGV, sig_handler);

	int ret = 0;
	try {
		ret = run_eix(argc, argv);
	}
	catch(ExBasic e) {
		cout << e << endl;
		return 1;
	}
	return ret;
}
