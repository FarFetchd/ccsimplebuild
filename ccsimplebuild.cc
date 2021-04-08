#include <cassert>
#include <filesystem>
#include <iostream>
#include <map>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace std;
constexpr bool kDryRun = false;

// =============================================================================
// "to be loaded": everything in this section should ideally come from some
// config file, analagous to a Makefile.
//
// name of binary to build
const char kTargetBinaryName[] = "strider_ib_gateway_client";
// g++ and front flags
const char kCompileCmdPrefix[] =
    "g++ -I\"TwsApiCpp/TwsApiC++/Api\" -I\"TwsApiCpp/TwsApiC++/Src\" "
    "-I\"TwsApiCpp/source/PosixClient/Shared\" "
    "-I\"TwsApiCpp/source/PosixClient/src\" -g -pthread -D_REENTRANT -D_DEBUG "
    "-std=c++17 -Wall -Wno-switch -Wno-psabi";
// end lib flags
const char kCompileEndLibs[] = "-latomic -lcurl";

// list of binary target's additional deps:
//  pathname of dep output (key of map)
//  list of pathnames of files it depends on
//  command to execute (everything AFTER g++ and front flags)
struct ExplicitDep
{
  ExplicitDep() {}
  ExplicitDep(vector<string> dp, string c)
      : dep_paths(dp), cmd_after_compile_prefix(c) {}
  vector<string> dep_paths;
  string cmd_after_compile_prefix;
};
// key is path
unordered_map<string, ExplicitDep> g_explicit_deps;
void loadExplicitDeps()
{
  // e.g.:
//   g_explicit_deps["obj/TwsApiL0.o"] = ExplicitDep(
//       {
//         "callbacks.cc",
//         "callbacks.h",
//         "TwsApiCpp/TwsApiC++/Api/TwsApiL0.h",
//         "TwsApiCpp/TwsApiC++/Api/TwsApiDefs.h",
//         "TwsApiCpp/TwsApiC++/Api/Enumerations.h",
//         "TwsApiCpp/TwsApiC++/Src/TwsApiL0.cpp",
//         "TwsApiCpp/source/PosixClient/src/EClientSocketBase.cpp",
//         "TwsApiCpp/source/PosixClient/src/EPosixClientSocket.cpp",
//         "TwsApiCpp/source/PosixClient/src/EPosixClientSocket.h",
//         "TwsApiCpp/source/PosixClient/src/EPosixClientSocketPlatform.h"
//       },
//       "-c TwsApiCpp/TwsApiC++/Src/TwsApiL0.cpp -o obj/TwsApiL0.o");
};
// =============================================================================

string cleanPath(string path)
{
  return path[0] == '.' ? path.substr(2) : path;
}
template <typename TP>
time_t to_time_t(TP tp)
{
  using namespace chrono;
  auto sctp = time_point_cast<system_clock::duration>(
      tp - TP::clock::now() + system_clock::now());
  return system_clock::to_time_t(sctp);
}

// Basically like VAR=`cmd` in bash.
string runShellSync(const char* cmd)
{
  char buffer[1024];
  string ret;
  unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  assert(pipe);

  while (fgets(buffer, 1024, pipe.get()) != nullptr)
    ret += buffer;
  return ret;
}

vector<string> splitString(string s, char delim)
{
  vector<string> result;
  stringstream ss(s);
  string item;
  while (getline(ss, item, delim))
    result.push_back(item);
  return result;
}

vector<string> directDeps(string path)
{
  string cmd("grep \"#include \\\"\" ");
  cmd += path + " | sed 's/.*#include \"// ; s/\"//'";
  string out = runShellSync(cmd.c_str());
  return splitString(out, '\n');
}

bool endsWith(const string& str, const string suffix)
{
  if (str.empty() || suffix.empty())
    return false;
  int i = str.length() - 1;
  int j = suffix.length() - 1;
  while (i >= 0 && j >= 0)
  {
    if (str[i] != suffix[j])
      return false;
    i--;
    j--;
  }
  return j == -1 && suffix[0] == str[i+1];
}

void buildCmd(string cmd)
{
  if (kDryRun)
    cout<<"WOULD run: "<<cmd<<endl;
  else
  {
    cout<<cmd<<endl;
    runShellSync(cmd.c_str());
  }
}

class DepNode
{
public:
  DepNode() {}
  DepNode(string path) : path_(path)
  {
    auto entry = filesystem::directory_entry(path);
    if (entry.exists())
      modified_ = to_time_t(entry.last_write_time());
  }

  void addDep(string path, DepNode* node) { deps_[path] = node; }

  bool weAreReal()
  {
    return endsWith(path_, ".o") || path_ == kTargetBinaryName ||
           g_explicit_deps.find(path_) != g_explicit_deps.end();
  }

  void rebuild()
  {
    if (auto it = g_explicit_deps.find(path_) ; it != g_explicit_deps.end())
    {
      string cmd(kCompileCmdPrefix);
      cmd += " " + g_explicit_deps[path_].cmd_after_compile_prefix;
      buildCmd(cmd);
    }
    else if (endsWith(path_, ".o"))
    {
      assert(deps_.size() == 1);
      for (auto [depname, dep] : deps_)
      {
        assert(endsWith(depname, ".cc"));
        string cmd(kCompileCmdPrefix);
        cmd += " -c -o " + path_ + " " + depname;
        buildCmd(cmd);
      }
    }
    else if (path_ == kTargetBinaryName)
    {
      string cmd(kCompileCmdPrefix);
      for (auto [name, val] : deps_)
        cmd += " " + name;
      cmd += " -o " + string(kTargetBinaryName) + " " + kCompileEndLibs;
      buildCmd(cmd);
    }
    if (weAreReal())
      modified_ = time(0);
  }

  bool rebuildIfNeeded(time_t cutoff)
  {
    bool need_rebuild = false;
    if (weAreReal() && cutoff > modified_)
      cutoff = modified_;
    if (modified_ > cutoff)
      need_rebuild = true;

    for (auto [path, node] : deps_)
      if (node->rebuildIfNeeded(cutoff))
        need_rebuild = true;

    if (need_rebuild)
      rebuild();
    return need_rebuild;
  }

  void printTree(int depth)
  {
    string pad(depth * 2, '-');
    cout << pad << path_ << endl;
    for (auto [name, node] : deps_)
      node->printTree(depth + 1);
  }

private:
  string path_;
  time_t modified_ = 0;
  map<string, DepNode*> deps_;
};
// key is path of dependency output. e.g. if the key is foo.o, the DepNode will
// have a single dependency of foo.c. foo.c and foo.h will also be keys in this
// map, with dependency links to anything they #include.
unordered_map<string, DepNode> g_all_nodes;

DepNode* getOrInsertNode(string path)
{
  if (auto it = g_all_nodes.find(path); it == g_all_nodes.end())
    g_all_nodes[path] = DepNode(path);
  return &g_all_nodes[path];
}

void populateExplicitDeps()
{
  loadExplicitDeps();
  for (auto [expdepath, expdepstuff] : g_explicit_deps)
  {
    DepNode node(expdepath);
    for (auto const& depath : expdepstuff.dep_paths)
      node.addDep(depath, getOrInsertNode(depath));
    g_all_nodes[expdepath] = node;
    g_all_nodes[kTargetBinaryName].addDep(expdepath, &g_all_nodes[expdepath]);
  }
}

void makeObjDepFromCc(string cc_path, DepNode* target_binary)
{
  string obj_path("obj/");
  obj_path += cc_path.substr(0, cc_path.length()-3) + ".o";
  DepNode obj_node(obj_path);
  obj_node.addDep(cc_path, &g_all_nodes[cc_path]);
  g_all_nodes[obj_path] = obj_node;
  target_binary->addDep(obj_path, &g_all_nodes[obj_path]);
}

int main()
{
  DepNode target_binary(kTargetBinaryName);

  // first just put all *.h and *.cc in the source dir into the map.
  // just the items themselves; don't yet search for their deps.
  for (auto const& item : filesystem::directory_iterator("."))
  {
    if (!item.is_regular_file())
      continue;
    string path = cleanPath(item.path());
    if (endsWith(path, ".h"))
      g_all_nodes[path] = DepNode(path);
    else if (endsWith(path, ".cc"))
    {
      g_all_nodes[path] = DepNode(path);
      // .o files get auto-added when the corresponding .cc is seen. .o files
      // get added to the final target binary's deps here.
      makeObjDepFromCc(path, &target_binary);
    }
  }

  // next, establish the immediate dependencies of the .h and .cc files in the
  // source dir. (these links may reach to .h's in other places, but we won't
  // follow up on the dependencies of those external things).
  vector<string> preexisting;
  for (auto [path, node] : g_all_nodes)
    if (!endsWith(path, ".o"))
      preexisting.push_back(path);
  for (auto path : preexisting)
  {
    if (endsWith(path, ".o"))
      continue;
    vector<string> deps = directDeps(path);
    DepNode& node = g_all_nodes[path];
    for (auto const& dep : deps)
      node.addDep(dep, getOrInsertNode(dep));
  }

  // actually add the target binary node to the map, and load in explicitly
  // specified deps (all of which the target binary is assumed to depend on).
  g_all_nodes[kTargetBinaryName] = target_binary;
  populateExplicitDeps();

  // a nice pretty view of your dependency "tree", if you're interested.
  // (entries are duplicated for each parent that depends on them).
  // g_all_nodes[kTargetBinaryName].printTree(0);

  // we should now have a complete dependency graph. traverse it to build.
  if (!g_all_nodes[kTargetBinaryName].rebuildIfNeeded(time(0)))
    cout << "ccsimplebuild: '"<<kTargetBinaryName<<"' is up to date." << endl;
}
