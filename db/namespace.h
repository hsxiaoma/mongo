// namespace.h

#pragma once

#include "../util/hashtab.h"
#include "../util/mmap.h"

class Cursor;

#pragma pack(push)
#pragma pack(1)

class Namespace {
public:
	Namespace(const char *ns) { 
		*this = ns;
	}
	Namespace& operator=(const char *ns) { 
		memset(buf, 0, 128); /* this is just to keep stuff clean in the files for easy dumping and reading */
		strcpy_s(buf, 128, ns); return *this; 
	}

	bool operator==(const Namespace& r) { return strcmp(buf, r.buf) == 0; }
	int hash() const {
		unsigned x = 0;
		const char *p = buf;
		while( *p ) { 
			x = x * 131 + *p;
			p++;
		}
		return (x & 0x7fffffff) | 0x8000000; // must be > 0
	}

	char buf[128];
};

const int Buckets = 19;
const int MaxBucket = 18;
const int MaxIndexes = 10;

class IndexDetails { 
public:
	DiskLoc head; /* btree head */
	DiskLoc info; /* index info object. { name:, ns:, key: } */

	void getKeysFromObject(JSObj& obj, set<JSObj>& keys);

	// client.table.$index
	string indexNamespace() { 
		JSObj io = info.obj();
		string s;
		s.reserve(128);
		s = io.getStringField("ns");
		assert( !s.empty() );
		s += ".$";
		s += io.getStringField("name"); 
		return s;
	}
};

extern int bucketSizes[];

class NamespaceDetails {
public:
	NamespaceDetails() { 
		datasize = nrecords = 0;
		lastExtentSize = 0;
		nIndexes = 0;
		memset(reserved, 0, sizeof(reserved));
	} 
	DiskLoc firstExtent;
	DiskLoc lastExtent;
	DiskLoc deletedList[Buckets];
	long long datasize;
	long long nrecords;
	int lastExtentSize;
	int nIndexes;
	IndexDetails indexes[MaxIndexes];
	char reserved[256-16-4-4-8*MaxIndexes-8];

	//returns offset in indexes[]
	int findIndexByName(const char *name) { 
		for( int i = 0; i < nIndexes; i++ ) {
			if( strcmp(indexes[i].info.obj().getStringField("name"),name) == 0 )
				return i;
		}
		return -1;
	}

	static int bucket(int n) { 
		for( int i = 0; i < Buckets; i++ )
			if( bucketSizes[i] > n )
				return i;
		return Buckets-1;
	}

	DiskLoc alloc(int lenToAlloc, DiskLoc& extentLoc);
	void addDeletedRec(DeletedRecord *d, DiskLoc dloc);
private:
	DiskLoc _alloc(int len);
};

#pragma pack(pop)

class NamespaceIndex {
	friend class NamespaceCursor;
public:
	NamespaceIndex() { }

	void init(const char *dir) { 
		string path = dir;
		path += "namespace.idx";
		const int LEN = 16 * 1024 * 1024;
		void *p = f.map(path.c_str(), LEN);
		if( p == 0 ) { 
			cout << "couldn't open namespace.idx" << endl;
			exit(-3);
		}
		ht = new HashTable<Namespace,NamespaceDetails>(p, LEN, "namespace index");
	}

	void add(const char *ns, DiskLoc& loc) { 
		Namespace n(ns);
		NamespaceDetails details;
		details.lastExtent = details.firstExtent = loc;
		ht->put(n, details);
	}

	NamespaceDetails* details(const char *ns) { 
		Namespace n(ns);
		return ht->get(n); 
	}

	bool find(const char *ns, DiskLoc& loc) { 
		NamespaceDetails *l = details(ns);
		if( l ) {
			loc = l->firstExtent;
			return true;
		}
		return false;
	}

private:
	MemoryMappedFile f;
	HashTable<Namespace,NamespaceDetails> *ht;
};

extern NamespaceIndex namespaceIndex;

auto_ptr<Cursor> makeNamespaceCursor();
