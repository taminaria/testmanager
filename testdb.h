#ifndef TESTDB_H
#define TESTDB_H

#include <mgr/mgrdb.h>
#include <mgr/mgrdb_struct.h>

namespace testdb {
mgr_db::JobCache * GetCache();

class VMManagerTable : public mgr_db::Table {
public:
	mgr_db::StringField Url;
	mgr_db::StringField User;
	mgr_db::StringField Passwd;
	mgr_db::StringField Comment;
	VMManagerTable();
};

class RepoTable : public mgr_db::Table {
public:
	mgr_db::StringField Comment;
	RepoTable();
	bool FindByName(const string& name);
};

class ManagerTable : public mgr_db::Table {
public:
	ManagerTable();
};

class BranchTable : public mgr_db::IdTable {
public:
	mgr_db::StringField Name;
	mgr_db::ReferenceField Manager;
	BranchTable();
	};

class BuildTable : public mgr_db::Table {
public:
	mgr_db::ReferenceField Branch;
	mgr_db::ReferenceField Repo;
	mgr_db::IntField Tag;
	BuildTable();
	bool FindByName(const string& name);
};

class VMTable : public mgr_db::IdTable {
public:
	mgr_db::StringField VMId;
	mgr_db::StringField Name;
	mgr_db::StringField Ip;
	mgr_db::StringField Os;
	mgr_db::StringField State;
	mgr_db::ReferenceField Repo;
	mgr_db::ReferenceField VMManager;
	mgr_db::StringField Comment;
	mgr_db::IntField Tag;
	VMTable();

	bool FindByVMId(const string& vmid, const string& vmmanager_id);
};


class BugTable : public mgr_db::Table {
public:
	mgr_db::StringField BugId;
	mgr_db::TextField Descr;
	mgr_db::ReferenceField Branch;
	mgr_db::StringField State;
	mgr_db::StringField Comment;
	mgr_db::IntField Tag;
	BugTable();
};

class TaskTable : public mgr_db::Table{
public:
	mgr_db::TextField Url;
	mgr_db::ReferenceField Manager;
	TaskTable();
};

}//end of testdb namespace
#endif //TESTDB_H
