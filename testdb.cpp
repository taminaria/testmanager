#include "testdb.h"
#include <api/module.h>

namespace testdb {
using namespace mgr_db;
JobCache * cache = nullptr;
JobCache * GetCache() {
	return cache;
}

VMManagerTable::VMManagerTable()
	: Table("vmmgr", 255)
	, Url(this, "url", 255)
	, User(this, "user", 255)
	, Passwd(this, "passwd", 255)
	, Comment(this, "comment", 255)
	{}

RepoTable::RepoTable()
	: Table("repo", 255)
	, Comment(this, "comment", 255)
	{}
bool RepoTable::FindByName(const string& name) {
	return DbFind("name=" + GetCache()->EscapeValue(name));
}

ManagerTable::ManagerTable()
	: Table("manager", 255)
	{}

BranchTable::BranchTable()
	: IdTable("branch", 255)
	, Name(this, "name", 255)
	, Manager(this, "manager", rtCascade)
	{
		AddIndex(itUnique, Name, Manager);
	}

BuildTable::BuildTable()
	: Table("build", 255)
	, Branch(this, "branch", rtCascade)
	, Repo(this, "repo", rtCascade)
	, Tag (this, "tag")
	{}
bool BuildTable::FindByName(const string& name) {
	return DbFind ("name=" + GetCache()->EscapeValue(name));
}

VMTable::VMTable()
	: IdTable("vm", 255)
	, VMId(this, "vmid", 255)
	, Name(this, "name", 255)
	, Ip(this, "ip", 255)
	, Os(this, "os", 255)
	, State(this, "state", 255)
	, Repo(this, "repo", rtCascade)
	, VMManager(this, "vmmgr", rtCascade)
	, Comment(this, "comment", 255)
	, Tag (this, "tag")
	{
		AddIndex(itUnique, VMId, VMManager);
		Repo.info().can_be_null=true;
	}

bool VMTable::FindByVMId(const string& vmid, const string& vmmanager_id) {
	return DbFind("vmid=" + GetCache()->EscapeValue(vmid) + " AND vmmgr=" + GetCache()->EscapeValue(vmmanager_id));
}


BugTable::BugTable()
	: Table("bug", 255)
	, BugId(this, "bugid", 16)
	, Descr(this, "descr")
	, Branch(this, "branch", rtCascade)
	, State (this, "state", 255)
	, Comment(this, "comment", 255)
	, Tag(this, "tag")
	{
		BugId.info().set_default("");
		AddIndex(itUnique, BugId);
	}

TaskTable::TaskTable()
	: Table ("task", 255)
	, Url (this, "url")
	, Manager(this, "manager", rtCascade)
{}

} //end of testdb namespace

namespace {
MODULE_INIT(testdb, "") {
	mgr_db::ConnectionParams params;
	params.type = DBTYPE_EMBEDDED;
	params.db = "etc/testmanager.db";
	isp_api::RegisterComponent(testdb::cache = new mgr_db::JobCache(params));
	testdb::cache->Register<testdb::VMManagerTable>();
	testdb::cache->Register<testdb::RepoTable>();
	testdb::cache->Register<testdb::ManagerTable>();
	testdb::cache->Register<testdb::BranchTable>();
	testdb::cache->Register<testdb::BuildTable>();
	testdb::cache->Register<testdb::VMTable>();
	testdb::cache->Register<testdb::BugTable>();
	testdb::cache->Register<testdb::TaskTable>();
}
} //end of private namespace
