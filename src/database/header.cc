// vim:set noet cinoptions= sw=4 ts=4:
// This file is part of the eix project and distributed under the
// terms of the GNU General Public License v2.
//
// Copyright (c)
//   Wolfgang Frisch <xororand@users.sourceforge.net>
//   Emil Beinroth <emilbeinroth@gmx.net>
//   Martin Väth <vaeth@mathematik.uni-wuerzburg.de>

#include "header.h"

#include <eixTk/exceptions.h>
#include <eixTk/filenames.h>

using namespace std;

const DBHeader::OverlayTest
	DBHeader::OVTEST_NONE,
	DBHeader::OVTEST_SAVED_PORTDIR,
	DBHeader::OVTEST_PATH,
	DBHeader::OVTEST_ALLPATH,
	DBHeader::OVTEST_LABEL,
	DBHeader::OVTEST_NUMBER,
	DBHeader::OVTEST_NOT_SAVED_PORTDIR,
	DBHeader::OVTEST_ALL;
const DBHeader::DBVersion DBHeader::current;

/** Get string for key from directory-table. */
const OverlayIdent &
DBHeader::getOverlay(Version::Overlay key) const
{
	if(key > countOverlays()) {
		static const OverlayIdent not_found("", "");
		return not_found;
	}
	return overlays[key];
}

/** Add overlay to directory-table and return key. */
Version::Overlay
DBHeader::addOverlay(const OverlayIdent& overlay)
{
	overlays.push_back(overlay);
	return countOverlays() - 1;
}

bool DBHeader::find_overlay(Version::Overlay *num, const char *name, const char *portdir, Version::Overlay minimal, OverlayTest testmode) const
{
	if(minimal > countOverlays())
		return false;
	if(*name == '\0') {
		if(countOverlays() == 1)
			return false;
		*num = (minimal != 0) ? minimal : 1;
		return true;
	}
	if(testmode & OVTEST_LABEL) {
		for(Version::Overlay i = minimal; i != countOverlays(); ++i) {
			if(getOverlay(i).label == name) {
				*num = i;
				return true;
			}
		}
	}
	if(testmode & OVTEST_PATH) {
		if(minimal == 0) {
			if(portdir) {
				if(same_filenames(name, portdir, true)) {
					*num = 0;
					return true;
				}
			}
		}
		for(Version::Overlay i = minimal; i != countOverlays(); ++i) {
			if(same_filenames(name, getOverlay(i).path.c_str(), true)) {
				if((i == 0) && ! (testmode & OVTEST_SAVED_PORTDIR))
					continue;
				*num = i;
				return true;
			}
		}
	}
	if( ! (testmode & OVTEST_NUMBER))
		return false;
	// Is name a number?
	Version::Overlay number;
	if(!is_numeric(name))
		return false;
	try {
		number = my_atoi(name);
		if(number >= countOverlays())
			return false;
		if(number < minimal)
			return false;
	}
	catch(const ExBasic &e) {
		return false;
	}
	*num = number;
	return true;
}

void
DBHeader::get_overlay_vector(set<Version::Overlay> *overlayset, const char *name, const char *portdir, Version::Overlay minimal, OverlayTest testmode) const
{
	Version::Overlay curr;
	for(curr = minimal; find_overlay(&curr, name, portdir, curr, testmode); ++curr)
		overlayset->insert(curr);
}
