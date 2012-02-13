// vim:set noet cinoptions= sw=4 ts=4:
// This file is part of the eix project and distributed under the
// terms of the GNU General Public License v2.
//
// Copyright (c)
//   Wolfgang Frisch <xororand@users.sourceforge.net>
//   Emil Beinroth <emilbeinroth@gmx.net>
//   Martin Väth <vaeth@mathematik.uni-wuerzburg.de>

#include <config.h>
#include "cdb.h"
#include <cache/common/unpickle.h>
#include <eixTk/exceptions.h>
#include <eixTk/formated.h>
#include <eixTk/i18n.h>
#include <eixTk/inttypes.h>
#include <eixTk/likely.h>
#include <eixTk/stringutils.h>
#include <portage/package.h>
#include <portage/packagetree.h>
#include <portage/version.h>

#include <map>
#include <string>

#include <cstddef>
#include <cstdlib>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

using namespace std;

/* Path to portage cache */
#define PORTAGE_CACHE_PATH "/var/cache/edb/dep"

class Cdb {
	private:
		bool is_ready;

		uint32_t *cdb_data;
		off_t cdb_data_size;
		uint32_t *cdb_records_end;
		uint32_t *current;

		bool mapData(int fd) {
			struct stat st;
			if (fstat(fd,&st) == 0) {
				void *x(mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0));
				if (x != MAP_FAILED) {
					cdb_data_size = st.st_size;
					cdb_data = static_cast<uint32_t *>(x);
					return true;
				}
			}
			return false;
		}

		void init() {
			uint32_t record_end_offset;
			UINT32_UNPACK(reinterpret_cast<char *>(cdb_data), &record_end_offset);
			cdb_records_end = reinterpret_cast<uint32_t *>((reinterpret_cast<char *>(cdb_data)) + record_end_offset);
			current = cdb_data + (2 * 256);
			is_ready = true;
		}

	public:
		Cdb(const char *file) {
			cdb_data = NULL;
			cdb_data_size = 0;
			is_ready = false;
			int fd(open(file, O_RDONLY));
			if(fd == -1) {
				return;
			}
			if(unlikely(!mapData(fd))) {
				close(fd);
				return;
			}
			close(fd);

			init();
		}

		~Cdb() {
			if(likely(cdb_data != NULL)) {
				munmap(cdb_data, cdb_data_size);
			}
		}

		string get(uint32_t *dlen, void **data) {
			uint32_t klen;
			UINT32_UNPACK(reinterpret_cast<char *>(current), &klen);
			++current;
			UINT32_UNPACK(reinterpret_cast<char *>(current), dlen);
			++current;
			string key(reinterpret_cast<char *>(current), klen);
			current = reinterpret_cast<uint32_t*>((reinterpret_cast<char *>(current)) + (klen));
			*data = current;
			current = reinterpret_cast<uint32_t*>((reinterpret_cast<char *>(current)) + (*dlen));
			return key;
		}

		bool end() {
			return current >= cdb_records_end;
		}

		bool isReady() {
			return is_ready;
		}
};

bool
CdbCache::readCategoryPrepare(const char *cat_name) throw(ExBasic)
{
	m_catname = cat_name;
	string catpath(m_prefix + PORTAGE_CACHE_PATH + m_scheme + cat_name + ".cdb");
	cdb = new Cdb(catpath.c_str());
	return cdb->isReady();
}

void
CdbCache::readCategoryFinalize()
{
	if(cdb != NULL) {
		delete cdb;
		cdb = NULL;
	}
	m_catname.clear();
}

bool
CdbCache::readCategory(Category &cat) throw(ExBasic)
{
	uint32_t dlen;
	void *data;
	string key;

	while(likely(!cdb->end())) {
		key = cdb->get(&dlen, &data);
		map<string,string> mapping;
		if( ! unpickle_get_mapping(static_cast<char *>(data), dlen, mapping)) {
			m_error_callback(eix::format(_("Problems with %s.. skipping")) % key);
			continue;
		}

		/* Split string into package and version, and catch any errors. */
		char **aux(ExplodeAtom::split(key.c_str()));
		if(aux == NULL) {
			m_error_callback(eix::format(_("Cannot split %r into package and version")) % key);
			continue;
		}
		/* Search for existing package */
		Package *pkg(cat.findPackage(aux[0]));
		/* If none was found create one */
		if(pkg == NULL) {
			pkg = cat.addPackage(m_catname, aux[0]);
		}

		/* Make version and add it to package. */
		Version *version(new Version(aux[1]));
		pkg->addVersionStart(version);
		/* For the latest version read/change corresponding data */
		if(*(pkg->latest()) == *version)
		{
			pkg->desc     = mapping["DESCRIPTION"];
			pkg->homepage = mapping["HOMEPAGE"];
			pkg->licenses = mapping["LICENSE"];
		}

		/* Read stability */
		version->set_full_keywords(mapping["KEYWORDS"]);
		version->slotname = mapping["SLOT"];
		version->set_iuse(mapping["IUSE"]);
		version->set_restrict(mapping["RESTRICT"]);
		version->set_properties(mapping["PROPERTIES"]);
		version->overlay_key = m_overlay_key;
		pkg->addVersionFinalize(version);

		/* Free split */
		free(aux[0]);
		free(aux[1]);
	}
	return true;
}
