#include <mgr/mgrrpc.h>
#include <mgr/mgrclient.h>
#include <api/module.h>
#include <api/action.h>
#include <api/stdconfig.h>
#include <api/dbaction.h>
#include <mgr/mgrproc.h>
#include <mgr/mgrdb_struct.h>

#include "testdb.h"

#define CONF_BUGZILLA_URL "BugzillaUrl"
#define CONF_BUGZILLA_USER "BugzillaUser"
#define CONF_BUGZILLA_PASSWD "BugzillaPasswd"

#define BUGZILLA_RPC "/xmlrpc.cgi"
#define JENKINS_RPC "/api/xml"
#define JENKINS_GET_BUILD "/api/xml?tree=builds[number]"
#define JENKINS_GET_BUILD_PARAMS "/api/xml?tree=actions[parameters[*]]"

MODULE("integration");

namespace {
using namespace isp_api;

//VMmanager
class VMmgrSettings : public TableNameListAction<testdb::VMManagerTable> {
public:
	VMmgrSettings() : TableNameListAction("vmmanager.settings", MinLevel(lvAdmin), *testdb::GetCache()) {}
private:
	void CheckParams(Cursor &table) const {

	}

	virtual void TableSet(Session& ses, Cursor &table) const {
		CheckParams(table);
	}
};

class UpdateVMs : public Action {
public:
	UpdateVMs() : Action("vmupdate", MinLevel(lvAdmin)) {}
private:
	virtual void Execute(Session &ses) const {
		auto vm = testdb::GetCache()->Get<testdb::VMTable>();
		time_t tag = mgr_date::DateTime();
		ForEachQuery(testdb::GetCache(),
			"SELECT id"
			", url"
			", user"
			", passwd"
			" FROM vmmgr"
			, q) {
			string vmmgr_id = q->AsString("id");
			mgr_client::Remote rem_cli(q->AsString("url"));
			rem_cli.AddParam("authinfo", q->AsString("user") + ":" + q->AsString("passwd"));
			auto res_els = rem_cli.Query("func=vm").elems();
			ForEachI(res_els, r) {
				string vmid = r->FindNode("id").Str();
				if (!vm->FindByVMId(vmid, vmmgr_id)) {
					vm->New();
					vm->VMId = vmid;
					vm->VMManager = str::Int(vmmgr_id);

				}
				string os = r->FindNode("vmi").Str();
				if (os.empty()){
					os = r->FindNode("ostemplate").Str();
				}

				vm->Name = r->FindNode("name").Str();
				vm->Ip = r->FindNode("ip").Str();
				vm->Os = os;
				vm->State = r->FindNode("status").Str();
				vm->Tag = tag;
				vm->Post();
			}
		}
		vm->DbDelete("tag<>" + str::Str(tag) + " OR tag IS NULL");
	}
};

class InfoVMs: public Action {
public:
	InfoVMs() : Action("infovms", MinLevel(lvAdmin)) {}
private:
	virtual void Execute(Session &ses) const {
		auto repodb = testdb::GetCache()->Get<testdb::RepoTable>();
		auto vmdb = testdb::GetCache()->Get<testdb::VMTable>();
		ForEachQuery(testdb::GetCache(),
		"SELECT ip"
		", id"
		" FROM vm"
		, q){
			mgr_proc::Execute exec("sh addon/infovms.sh " + q->AsString("ip"), mgr_proc::Execute::efOut);
			const std::string repo = exec.Str();
			Debug("\n%s", repo.c_str());
			if(repo.empty())
				continue;
			repodb->AssertByName(repo);
			Debug("ID: %s", repodb->Id.AsString().c_str());
			vmdb->Assert(q->AsString("id"));
			vmdb->Repo = repodb->Id;
			vmdb->Post();
		}
}
};
//Bugzilla
class BugzillaSettings : public FormAction {
public:
	BugzillaSettings() : FormAction("bugzilla.settings", MinLevel(lvAdmin)) {}
private:
	void CheckParams(Session &ses) const {

	}

	virtual void Get(Session &ses, const string &elid) const {
		ses.NewNode("url", mgr_cf::GetParam(CONF_BUGZILLA_URL));
		ses.NewNode("user", mgr_cf::GetParam(CONF_BUGZILLA_USER));
		ses.NewNode("passwd", mgr_cf::GetParam(CONF_BUGZILLA_PASSWD));
	}

	virtual void Set(Session &ses, const string &elid) const {
		CheckParams(ses);
		mgr_cf::SetParam(CONF_BUGZILLA_URL, ses.Param("url"));
		mgr_cf::SetParam(CONF_BUGZILLA_USER, ses.Param("user"));
		mgr_cf::SetParam(CONF_BUGZILLA_PASSWD, ses.Param("passwd"));
	}
};

string GetMemberStrValue(XmlNode& node) {
	auto val = node.FindNode("value");
	if (!val)
		return "";
	return val.FindNode("string").Str();
}

struct BugDesc {
	string name;
	string branch;
	string product;
	string state;
};


typedef std::map<string, BugDesc> BugMap;

class UpdateBugs : public Action {
public:
	UpdateBugs() : Action("bugupdate", MinLevel(lvAdmin)) {}
private:
	virtual void Execute(Session &ses) const {
		mgr_rpc::HttpQuery login_q;
		time_t tag = mgr_date::DateTime();
		login_q.AddHeader("Content-Type: text/xml");
		std::stringstream str;
		string data = "<?xml version='1.0' encoding='UTF-8'?>"
				"<methodCall>"
					"<methodName>User.login</methodName>"
					"<params><param><struct>"
						"<member>"
							"<name>login</name>"
							"<value>"
								"<string>"
									+ mgr_cf::GetParam(CONF_BUGZILLA_USER) +
								"</string>"
							"</value>"
						"</member>"
						"<member>"
							"<name>password</name>"
							"<value>"
								"<string>"
									+ mgr_cf::GetParam(CONF_BUGZILLA_PASSWD) +
								"</string>"
							"</value>"
						"</member>"
					"</struct></param></params>"
				"</methodCall>";
		login_q.AcceptHeader();
		if (!login_q.Post(mgr_cf::GetParam(CONF_BUGZILLA_URL) + BUGZILLA_RPC, data, str).good())
			throw mgr_err::Error("badresponse");
		string qres = str.str();
		Debug("---------Login result----------");
		Debug("\n%s", qres.c_str());
		StringList cookies;
		string find_cookie = "Set-Cookie: Bugzilla_login";
		while(!qres.empty()) {
			//Set-Cookie: Bugzilla_login
			string line = str::GetWord(qres, '\n');
			if (line.compare(0, find_cookie.size(), find_cookie) == 0) {
				str::GetWord(line, "Set-Cookie: ");
				cookies.push_back(line);
			}
		}

		mgr_rpc::HttpQuery req;
		req.AddHeader("Content-Type: text/xml");
		data = "<?xml version='1.0' encoding='UTF-8'?>"
					"<methodCall>"
						"<methodName>Bug.search</methodName>"
						"<params><param><struct>"
							"<member>"
								"<name>qa_contact</name>"
								"<value>"
									"<string>"
										+ mgr_cf::GetParam(CONF_BUGZILLA_USER) +
									"</string>"
								"</value>"
							"</member>"
							"<member>"
								"<name>status</name>"
								"<value>"
									"<string>"
										"RESOLVED"
									"</string>"
								"</value>"
							"</member>"
						"</struct></param></params>"
					"</methodCall>";
		ForEachI(cookies, c) {
			Debug("LOGIN COOKIE '%s'", c->c_str());
			req.SetCookie(*c);
		}
		std::stringstream str_buglist;
		if (!req.Post(mgr_cf::GetParam(CONF_BUGZILLA_URL) + BUGZILLA_RPC, data, str_buglist).good())
			throw mgr_err::Error("badresponse");
		qres = str_buglist.str();
		Debug("-------------------------------------------------------------------------------");
		Debug("\n%s", qres.c_str());
		auto bugs = mgr_xml::XmlString(qres).GetNodes("/methodResponse/params/param/value/struct/member[name='bugs']/value/array/data/value/struct/member[name='id']/value/int");
		BugMap bug_descs;
		ForEachI(bugs, b) {
			string bnum = b->Str();
			auto &bug_desc = bug_descs[bnum];
			Debug("BUG NUMBER '%s'", bnum.c_str());
			auto mstruct = b->Parent().Parent().Parent(); //встать на struct
			auto cvar = mstruct.FirstChild();
			while (cvar) {
				if (!cvar.IsNode() || cvar.Name() != "member") {
					cvar = cvar.Next();
					continue;
				}
				string curname = cvar.FindNode("name").Str();
				if (curname == "summary") {
					bug_desc.name = GetMemberStrValue(cvar);
				} else if (curname == "cf_branch") {
					bug_desc.branch = GetMemberStrValue(cvar);
				} else if (curname == "product") {
					bug_desc.product = GetMemberStrValue(cvar);
				} else if (curname == "resolution") {
					bug_desc.state = GetMemberStrValue(cvar);
				}
				cvar = cvar.Next();
			}
		}

		auto bug_c = testdb::GetCache()->Get<testdb::BugTable>();
		auto branch_c = testdb::GetCache()->Get<testdb::BranchTable>();
		auto manager_c = testdb::GetCache()->Get<testdb::ManagerTable>();
		ForEachI(bug_descs, bd) {
			if (!manager_c->FindByName(bd->second.product)) {
				manager_c->New();
				manager_c->Name = bd->second.product;
				manager_c->Post();
			}

			if (!branch_c->DbFind("name=" + testdb::GetCache()->EscapeValue(bd->second.branch) +
					" AND manager=" + testdb::GetCache()->EscapeValue(manager_c->Id))) {
				branch_c->New();
				branch_c->Name = bd->second.branch;
				branch_c->Manager = manager_c->Id;
				branch_c->Post();
			}

			if (!bug_c->DbFind("bugid=" + testdb::GetCache()->EscapeValue(bd->first))) {
				bug_c->New();
				bug_c->BugId = bd->first;
			}

			bug_c->Name = bd->second.name;
			bug_c->State = bd->second.state;
			bug_c->Branch = branch_c->Id;
			bug_c->Tag = tag;
			bug_c->Post();
		}
		bug_c->DbDelete("tag<>" + str::Str(tag) + " OR tag IS NULL");
	}
};

//Jenkins
class JenkinsTask : public TableNameListAction<testdb::TaskTable> {
public:
	JenkinsTask() : TableNameListAction("jenkins.task", MinLevel(lvAdmin), *testdb::GetCache()) {}
private:
	void CheckParams(Cursor &table) const {

	}

	virtual void TableSet(Session& ses, Cursor &table) const {
		CheckParams(table);
	}
	virtual void TableFormTune(Session& ses, Cursor &table) const {
		DbSelect(ses, "manager", testdb::GetCache()->Query("SELECT id, name FROM manager"));
	}
};


class UpdateBuild : public Action {
public:
	UpdateBuild() : Action("buildupdate", MinLevel(lvAdmin)) {}
private:
	virtual void Execute(Session &ses) const {
	auto repodb = testdb::GetCache()->Get<testdb::RepoTable>();
	auto branchdb = testdb::GetCache()->Get<testdb::BranchTable>();
	auto builddb = testdb::GetCache()->Get<testdb::BuildTable>();
	time_t tag = mgr_date::DateTime();
	ForEachQuery (testdb::GetCache(),
			"SELECT url"
			", manager"
			" FROM task"
			, t){
			auto repodb = testdb::GetCache()->Get<testdb::RepoTable>();
			auto branchdb = testdb::GetCache()->Get<testdb::BranchTable>();
			auto builddb = testdb::GetCache()->Get<testdb::BuildTable>();
			mgr_rpc::HttpQuery req;
			std::stringstream str_buildlist;
			if (!req.Post(t->AsString("url") + JENKINS_GET_BUILD, "", str_buildlist).good())
				throw mgr_err::Error("badresponse");
			string qresbuild = str_buildlist.str();
			auto builds = mgr_xml::XmlString(qresbuild).GetNodes("/matrixProject/build/number");
			ForEachI(builds, b) {
				string bnum = b->Str();
				if(bnum.empty())
					continue;
				auto builddb = testdb::GetCache()->Get<testdb::BuildTable>();

				std::stringstream str_buildparamlist;
				if (!req.Post(t->AsString("url") + bnum + JENKINS_GET_BUILD_PARAMS, "", str_buildparamlist).good())
						throw mgr_err::Error("badresponse");
				string qresbuildparams = str_buildparamlist.str();
				//ищем или пишем бранч
				auto br = mgr_xml::XmlString(qresbuildparams).GetNode("/matrixBuild/action/parameter[name='BRANCH']/value");
				string branches = br.Str();
				Debug("BRANCH %s", branches.c_str());
								if(branches.empty())
					continue;
				if (!branchdb->DbFind("name=" + testdb::GetCache()->EscapeValue(branches) + " and manager=" + t->AsString("manager"))) {
					branchdb->New();
					branchdb->Name = branches;
					branchdb->Manager = str::Int(t->AsString("manager"));
					branchdb->Post();
				}
				//ищем или пишем репозиторий
				auto re =  mgr_xml::XmlString(qresbuildparams).GetNode("/matrixBuild/action/parameter[name='TYPE']/value");
				string repos = re.Str();
				Debug("REPOS %s", repos.c_str());
				if(repos.empty())
					continue;
				if (repodb->DbFind("name=" + testdb::GetCache()->EscapeValue(repos))) {
					if (!builddb->FindByName(bnum)) {
						repodb->AssertByName(repos);
						string repoid = repodb->Id;
						Debug("-------------------------------REPO------------------------------------------------");
						Debug("\n%s", repoid.c_str());
						builddb->New();
						builddb->Name = bnum;
						builddb->Repo = repodb->Id;
						builddb->Branch = branchdb->Id;
						builddb->Post();
					}
					builddb->Repo = repodb->Id;
					builddb->Branch = branchdb->Id;
					builddb->Post();
				}
			}
	}
	ForEachQuery (testdb::GetCache(),
		"SELECT url"
		", manager"
		" FROM task"
		, t){
		auto builddb = testdb::GetCache()->Get<testdb::BuildTable>();
		ForEachQuery(testdb::GetCache(),
			"SELECT id"
			", name"
			" FROM repo "
			, r) {
			Debug("-------------------------------MANAGER------------------------------------------------");
			ForEachQuery(testdb::GetCache(),
					"select b.id"
					", b.name "
					"FROM build b "
					"INNER JOIN branch br ON br.id=b.branch "
					"INNER JOIN manager m ON m.id=br.manager "
					"INNER JOIN repo r ON r.id=b.repo "
					"where m.id="+ t->AsString("manager") +" and r.id=" + r->AsString("id") + " order by  b.name DESC limit 1"
					,s ){
				Debug("-------------------------------REPO------------------------------------------------");
				string id=s->AsString("id");
				if(id.empty())
					continue;
				Debug("-------------------------------TAG------------------------------------------------");
				Debug("\n%s", id.c_str());
				builddb->Assert(id);
				builddb->Tag = tag;
				builddb->Post();
			}
		}

		}
	builddb->DbDelete("tag<>" + str::Str(tag) + " OR tag IS NULL");
	}
};

MODULE_INIT(integration, "testdb") {
	mgr_cf::AddParam(CONF_BUGZILLA_URL);
	mgr_cf::AddParam(CONF_BUGZILLA_USER);
	mgr_cf::AddParam(CONF_BUGZILLA_PASSWD);

	new VMmgrSettings;
	new UpdateVMs;
	new InfoVMs;

	new BugzillaSettings;
	new UpdateBugs;

	new JenkinsTask;
	new UpdateBuild;
}
} //end of private namespace
