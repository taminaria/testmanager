#include <api/module.h>
#include <api/dbaction.h>
#include <api/stdconfig.h>
#include <mgr/mgrtask.h>
#include <mgr/mgrproc.h>
#include <mgr/mgrsession.h>
#include <api/filter.h>

#include "testdb.h"

MODULE("testmanager");

namespace {
using namespace isp_api;

class VMs : public TableIdListAction<testdb::VMTable> {
public:
	VMs() : TableIdListAction("vm", MinLevel(lvAdmin), *testdb::GetCache()), configure(this), m_filter(this) {}

	class aConfigure: public ModuleAction {
	public:
		aConfigure(Action * parent): ModuleAction("configure", MinLevel(lvAdmin), parent) {}

		virtual void ModuleExecute(Session &ses) const {
			string elid = ses.Param("elid", test::NotEmpty);
			auto vm_table = testdb::GetCache()->Get<testdb::VMTable>(elid);

			string ip = vm_table->Ip;
			Debug("VDS IP: '%s'", ip.c_str());
			mgr_proc::Execute exec("sh addon/configure.sh " + ip, mgr_proc::Execute::efOut);
			string error = exec.Str();
			if (exec.Result()!= EXIT_SUCCESS)
				throw mgr_err::Error("configure_error");
		}

	} configure;

private:
	string TableList(Session& ses, const string &table) const {
		return AddFilter(ses, "SELECT v.id AS id"
		", v.name AS name"
		", v.ip AS ip"
		", v.os AS os"
		", r.name AS repo"
		", v.state AS state"
		", v.comment AS comment"
		", m.name AS vmmgrname"
		" FROM " + table + " v"
		" INNER JOIN vmmgr m ON m.id=v.vmmgr"
		" LEFT JOIN repo r ON r.id=v.repo");
	}
	DbFilterAction m_filter;
};



class Bugs : public TableIdListAction<testdb::BugTable> {
public:
	Bugs() : TableIdListAction("bugs", MinLevel(lvAdmin), *testdb::GetCache()), go(this), m_filter(this) {}

	class aGo: public ModuleAction {
		public:
		aGo(Action * parent): ModuleAction("go", MinLevel(lvAdmin), parent) {}

			virtual void ModuleExecute(Session &ses) const {
				string elid = ses.Param("elid", test::NotEmpty);
				auto bug_table = testdb::GetCache()->Get<testdb::BugTable>(elid);
				ses.Ok(mgr_session::BaseSession::okBlank, "http://bugtrack.ispsystem.net/show_bug.cgi?id=" + bug_table->BugId);
			}
	} go;

private:
	string TableList(Session& ses, const string &table) const {
		return "SELECT b.id AS id"
		", b.name AS name"
		", b.bugid AS bugid"
		", b.descr AS descr"
		", b.comment AS comment"
		", br.name AS branchname"
		", m.name AS managername"
		", b.state AS state"
		" FROM " + table + " b"
		" INNER JOIN branch br ON br.id=b.branch"
		" INNER JOIN manager m ON m.id=br.manager";
	}
	DbFilterAction m_filter;
};

class Managers : public TableNameListAction<testdb::ManagerTable> {
public:
	Managers() : TableNameListAction("managers", MinLevel(lvAdmin), *testdb::GetCache()) {}

};

class Builds : public TableIdListAction<testdb::BuildTable> {
public:
	Builds() : TableIdListAction("builds", MinLevel(lvAdmin), *testdb::GetCache()) {}
private:
	string TableList(Session& ses, const string &table) const {
		return "SELECT b.name as name"
		", br.name as branch"
		", m.name as manager"
		", r.name as repo"
		" FROM " + table + " b"
		" INNER JOIN branch br ON br.id=b.branch"
		" INNER JOIN manager m ON m.id=br.manager"
		" INNER JOIN repo r ON r.id=b.repo";
	}

};

class Repo : public TableNameListAction<testdb::RepoTable> {
public:
	Repo() : TableNameListAction("repo", MinLevel(lvAdmin), *testdb::GetCache()) {}

};

class Workspace : public TableIdListAction<testdb::BugTable> {
public:
	Workspace() : TableIdListAction("workspace", MinLevel(lvAdmin), *testdb::GetCache()), m_filter(this) {}
private:
	string TableList(Session& ses, const string &table) const {
		return "SELECT b.id as id"
				", b.bugid as bugid"
				", b.name as name"
				", b.state as state"
				", r.name as repo"
				", br.name as branch"
				", b.comment as comment"
				", m.name as manager"
				" FROM bug b"
				" INNER JOIN branch br ON br.id=b.branch"
				" INNER JOIN manager m ON m.id=br.manager"
				" LEFT JOIN build bl ON bl.branch=br.id"
				" LEFT JOIN repo r ON r.id=bl.repo";
	}
	DbFilterAction m_filter;
};


MODULE_INIT(testmanager, "testdb") {
	new VMs;
	new Bugs;
	new Managers;
	new Builds;
	new Repo;
	new Workspace;
}
} //end of private namespace
