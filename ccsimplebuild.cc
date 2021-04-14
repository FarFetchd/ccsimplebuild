#include <sys/stat.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace std;
constexpr bool kDryRun = false;
vector<string> splitString(string s, char delim);

// =============================================================================
// everything in this section comes from ccsimple.buildfile, which is
// analagous to a Makefile.
//
// name of binary to build
string g_target_binary_name;
// g++ and front flags, e.g. g++ -g -Wall
string g_compile_cmd_prefix;
// end lib flags, e.g. -latomic -lcurl
string g_compile_end_libs;

// list of binary target's additional deps:
//  pathname of dep output (key of map)
//  list of pathnames of files it depends on
//  command to execute (everything AFTER g_compile_cmd_prefix)
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

void loadConfig()
{
  auto entry = filesystem::directory_entry("./ccsimple.buildfile");
  if (!entry.exists() || !entry.is_regular_file())
  {
    g_target_binary_name = "default_ccsimplebuild_output";
    g_compile_cmd_prefix = "g++ -std=c++17";
    g_compile_end_libs = "";
    return;
  }
  ifstream reader("./ccsimple.buildfile");
  vector<string> lines;
  string line;
  while (getline(reader, line))
    lines.push_back(line);
  while (lines.back().empty())
    lines.pop_back();
  assert(lines.size() >= 3);
  assert((lines.size() - 3) % 4 == 0);
  assert(lines[0].find("OutputBinaryFilename=") == 0);
  g_target_binary_name = lines[0].substr(21);
  assert(lines[1].find("CompileCommandPrefix=") == 0);
  g_compile_cmd_prefix = lines[1].substr(21);
  assert(lines[2].find("LibrariesToLink=") == 0);
  g_compile_end_libs = lines[2].substr(16);
  for (int i = 3; i < lines.size(); i+=4)
  {
    assert(lines[i].find("ExplicitDependency:") == 0);
    assert(lines[i+1].find("  Output=") == 0);
    string output = lines[i+1].substr(9);
    assert(lines[i+2].find("  CompileSuffix=") == 0);
    string suffix = lines[i+2].substr(16);
    assert(lines[i+3].find("  DependsOn=") == 0);
    string depends = lines[i+3].substr(12);
    g_explicit_deps[output] = ExplicitDep(splitString(depends, ','), suffix);
  }
}
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
    int ret = system(cmd.c_str());
    if (ret != 0)
      exit(ret);
  }
}

bool g_target_changed = false;

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

  time_t rebuild()
  {
    if (auto it = g_explicit_deps.find(path_) ; it != g_explicit_deps.end())
    {
      string cmd = g_compile_cmd_prefix + " " +
                   g_explicit_deps[path_].cmd_after_compile_prefix;
      buildCmd(cmd);
      return time(0);
    }
    else if (endsWith(path_, ".o"))
    {
      assert(deps_.size() == 1);
      for (auto [depname, dep] : deps_)
      {
        assert(endsWith(depname, ".cc"));
        string cmd = g_compile_cmd_prefix + " -c -o " + path_ + " " + depname;
        buildCmd(cmd);
        return time(0);
      }
    }
    else if (path_ == g_target_binary_name)
    {
      g_target_changed = true;
      string cmd = g_compile_cmd_prefix;
      for (auto [name, val] : deps_)
        cmd += " " + name;
      cmd += " -o " + g_target_binary_name + " " + g_compile_end_libs;
      buildCmd(cmd);
      return time(0);
    }
    return modified_;
  }

  time_t rebuildIfNeeded()
  {
    time_t most_recent = modified_;
    for (auto [path, node] : deps_)
      most_recent = std::max(most_recent, node->rebuildIfNeeded());

    if (most_recent > modified_)
      most_recent = std::max(most_recent, rebuild());
    return most_recent;
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
  for (auto [expdepath, expdepstuff] : g_explicit_deps)
  {
    DepNode node(expdepath);
    for (auto const& depath : expdepstuff.dep_paths)
      node.addDep(depath, getOrInsertNode(depath));
    g_all_nodes[expdepath] = node;
    g_all_nodes[g_target_binary_name].addDep(expdepath, &g_all_nodes[expdepath]);
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

int main(int argc, char** argv)
{
  loadConfig();
  mkdir("obj", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  DepNode target_binary(g_target_binary_name);

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
    vector<string> deps = directDeps(path);
    DepNode& node = g_all_nodes[path];
    for (auto const& dep : deps)
      node.addDep(dep, getOrInsertNode(dep));
  }

  // actually add the target binary node to the map, and load in explicitly
  // specified deps (all of which the target binary is assumed to depend on).
  g_all_nodes[g_target_binary_name] = target_binary;
  populateExplicitDeps();

  // a nice pretty view of your dependency "tree", if you're interested.
  // (entries are duplicated for each parent that depends on them).
  if (argc > 1 && string(argv[1]) == "--verbose")
    g_all_nodes[g_target_binary_name].printTree(0);

  // we should now have a complete dependency graph. traverse it to build.
  g_all_nodes[g_target_binary_name].rebuildIfNeeded();
  if (!g_target_changed)
    cout<<"ccsimplebuild: '"<<g_target_binary_name<<"' is up to date."<<endl;
}
