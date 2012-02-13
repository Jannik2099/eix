// vim:set noet cinoptions= sw=4 ts=4:
// This file is part of the eix project and distributed under the
// terms of the GNU General Public License v2.
//
// Copyright (c)
//   Martin Väth <vaeth@mathematik.uni-wuerzburg.de>

#include <config.h>
#include "eixcache.h"
#include <database/header.h>
#include <database/package_reader.h>
#include <eixTk/exceptions.h>
#include <eixTk/formated.h>
#include <eixTk/i18n.h>
#include <eixTk/likely.h>
#include <eixTk/ptr_list.h>
#include <eixTk/stringutils.h>
#include <portage/conf/portagesettings.h>
#include <portage/package.h>
#include <portage/packagetree.h>

#include <algorithm>
#include <string>
#include <vector>

#include <cstddef>
#include <cstdio>
#include <cstring>

using namespace std;

eix::ptr_list<EixCache> EixCache::all_eixcaches;

EixCache::~EixCache()
{
	eix::ptr_list<EixCache>::iterator it = find(all_eixcaches.begin(),
		all_eixcaches.end(), this);
	if(it != all_eixcaches.end()) {
		all_eixcaches.erase(it);
	}
}

bool EixCache::initialize(const string &name)
{
	vector<string> args;
	split_string(args, name, true, ":", false);
	if(strcasecmp(args[0].c_str(), "eix") == 0)
	{
		m_name = "eix";
		never_add_categories = true;
	}
	else if((strcasecmp(args[0].c_str(), "eix*") == 0) ||
		(strcasecmp(args[0].c_str(), "*eix") == 0))
	{
		m_name = "eix*";
		never_add_categories = false;
	}
	else
		return false;

	m_file.clear();
	if(args.size() >= 2) {
		if(! args[1].empty()) {
			m_name.append(1, ' ');
			m_name.append(args[1]);
			m_file = args[1];
		}
	}

	m_only_overlay = true;
	m_overlay.clear();
	m_get_overlay = 0;
	if(args.size() >= 3) {
		if(! args[2].empty()) {
			m_name.append(" [");
			m_name.append(args[2]);
			m_name.append("]");
			if(args[2] == "*") {
				m_only_overlay = false;
			}
			else
				m_overlay = args[2];
		}
	}
	slavemode = false;
	all_eixcaches.push_back(this);
	return (args.size() <= 3);
}

void
EixCache::setSchemeFinish()
{
	if(! m_file.empty())
		m_full = m_prefix + m_file;
	else
		m_full = m_prefix + EIX_CACHEFILE;
}

void
EixCache::allerrors(const vector<EixCache*> &slaves, const string &msg)
{
	for(vector<EixCache*>::const_iterator sl(slaves.begin());
		unlikely(sl != slaves.end()); ++sl) {
		string &s = (*sl)->err_msg;
		if(s.empty()) {
			s = msg;
		}
	}
	m_error_callback(err_msg);
}

void
EixCache::thiserror(const string &msg)
{
	err_msg = msg;
	if(!slavemode) {
		m_error_callback(err_msg);
	}
}

bool
EixCache::get_overlaydat(const DBHeader &header)
{
	if((!m_only_overlay) || (m_overlay.empty()))
		return true;
	const char *portdir(NULL);
	if(portagesettings)
			portdir = (*portagesettings)["PORTDIR"].c_str();
	if(m_overlay == "~") {
		bool found(false);
		if(!m_overlay_name.empty()) {
			found = header.find_overlay(&m_get_overlay, m_overlay_name.c_str(), portdir, 0, DBHeader::OVTEST_LABEL);
		}
		if(!found) {
			found = header.find_overlay(&m_get_overlay, m_scheme.c_str(), portdir, 0, DBHeader::OVTEST_LABEL);
		}
		if(!found) {
			thiserror(eix::format(_("Cache file %s does not contain overlay %r [%s]")) %
				m_overlay_name % m_scheme);
			return false;
		}
	}
	else if(!header.find_overlay(&m_get_overlay, m_overlay.c_str(), portdir, 0, DBHeader::OVTEST_ALL)) {
		thiserror(eix::format(_("Cache file %s does not contain overlay %s")) %
			m_full % m_overlay);
		return false;
	}
	return true;
}

bool
EixCache::get_destcat(PackageTree *packagetree, const char *cat_name, Category *category, const string &pcat)
{
	if(likely(err_msg.empty())) {
		if(unlikely(packagetree == NULL)) {
			if(unlikely(cat_name == pcat)) {
				dest_cat = category;
				return true;
			}
		}
		else if(never_add_categories) {
			dest_cat = packagetree->find(pcat);
			return (dest_cat != NULL);
		}
		else {
			dest_cat = &((*packagetree)[pcat]);
			return true;
		}
	}
	dest_cat = NULL;
	return false;
}

void
EixCache::get_package(Package *p)
{
	if(dest_cat == NULL)
		return;
	bool have_onetime_info(false), have_pkg(false);
	Package *pkg(NULL);
	for(Package::iterator it(p->begin()); likely(it != p->end()); ++it) {
		if(m_only_overlay) {
			if(likely(it->overlay_key != m_get_overlay))
				continue;
		}
		Version *version(new Version(it->getFull().c_str()));
		version->overlay_key = m_overlay_key;
		version->set_full_keywords(it->get_full_keywords());
		version->slotname = it->slotname;
		version->restrictFlags = it->restrictFlags;
		version->propertiesFlags = it->propertiesFlags;
		version->iuse = it->iuse;
		if(pkg == NULL) {
			pkg = dest_cat->findPackage(p->name);
			if(pkg != NULL) {
				have_onetime_info = have_pkg = true;
			}
			else {
				pkg = new Package(p->category, p->name);
			}
		}
		pkg->addVersion(version);
		if(*(pkg->latest()) == *version) {
			pkg->homepage = p->homepage;
			pkg->licenses = p->licenses;
			pkg->desc     = p->desc;
			have_onetime_info = true;
		}
	}
	if(have_onetime_info) { // if the package exists:
		// add collected iuse from the saved data
		pkg->iuse.insert(p->iuse);
		if(!have_pkg) {
			dest_cat->addPackage(pkg);
		}
	}
	else {
		delete pkg;
	}
}

bool
EixCache::readCategories(PackageTree *packagetree, const char *cat_name, Category *category) throw(ExBasic)
{
	if(slavemode) {
		if(err_msg.empty())
			return true;
		m_error_callback(err_msg);
		return false;
	}
	vector<EixCache*> slaves;
	for(eix::ptr_list<EixCache>::iterator sl = all_eixcaches.begin();
		unlikely(sl != all_eixcaches.end()); ++sl) {
		if(sl->m_full == m_full) {
			if(*sl != this) {
				sl->slavemode = true;
			}
			sl->err_msg.clear();
			slaves.push_back(*sl);
		}
	}
	FILE *fp(fopen(m_full.c_str(), "rb"));
	if(unlikely(fp == NULL)) {
		allerrors(slaves, eix::format(_("Can't read cache file %s: %s")) %
			m_full % strerror(errno));
		m_error_callback(err_msg);
		return false;
	}

	DBHeader header;

	if(!io::read_header(fp, header)) {
		fclose(fp);
		allerrors(slaves, eix::format(
			(header.version > DBHeader::current) ?
			_("Cache file %s uses newer format %s (current is %s)") :
			_("Cache file %s uses obsolete format %s (current is %s)"))
			% m_full % header.version % DBHeader::current);
		m_error_callback(err_msg);
		return false;
	}
	bool success(false);
	for(vector<EixCache*>::const_iterator sl(slaves.begin());
		unlikely(sl != slaves.end()); ++sl) {
		if((*sl)->get_overlaydat(header))
			success = true;
	}
	if(!success) {
		fclose(fp);
		return false;
	}

	for(PackageReader reader(fp, header); reader.next(); reader.skip())
	{
		reader.read(PackageReader::NAME);
		Package *p(reader.get());

		success = false;
		for(vector<EixCache*>::const_iterator sl(slaves.begin());
			unlikely(sl != slaves.end()); ++sl) {
			if((*sl)->get_destcat(packagetree, cat_name, category, p->category)) {
				success = true;
			}
		}
		if(!success) {
			continue;
		}
		reader.read(PackageReader::VERSIONS);
		p = reader.get();
		for(vector<EixCache*>::const_iterator sl(slaves.begin());
			unlikely(sl != slaves.end()); ++sl) {
			(*sl)->get_package(p);
		}
	}
	fclose(fp);
	return true;
}
