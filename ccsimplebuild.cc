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
// everything in this section comes from default.ccbuildfile, which is
// analagous to a Makefile.
//
// name of binary to build
string g_target_binary_name;
// g++ and front flags, e.g. g++ -g -Wall
string g_compile_cmd_prefix;
// end lib flags, e.g. -latomic -lcurl
string g_compile_end_libs;

void loadConfig(string fname)
{
  auto entry = filesystem::directory_entry(fname);
  if (!entry.exists() || !entry.is_regular_file())
  {
    if (fname != "default.ccbuildfile")
    {
      cout << "Error: buildfile " << fname << " does not exist." << endl;
      exit(1);
    }
    g_target_binary_name = "default_ccsimplebuild_output";
    g_compile_cmd_prefix = "g++ -std=c++17";
    g_compile_end_libs = "";
    return;
  }
  ifstream reader(fname);
  vector<string> lines;
  string line;
  while (getline(reader, line))
    lines.push_back(line);
  while (lines.back().empty())
    lines.pop_back();
  assert(lines.size() == 3);
  assert(lines[0].find("OutputBinaryFilename=") == 0);
  g_target_binary_name = lines[0].substr(21);
  assert(lines[1].find("CompileCommandPrefix=") == 0);
  g_compile_cmd_prefix = lines[1].substr(21);
  assert(lines[2].find("LibrariesToLink=") == 0);
  g_compile_end_libs = lines[2].substr(16);
}
// =============================================================================

void checkSanitized(string path)
{
  if (path.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                             "abcdefghijklmnopqrstuvwxyz"
                             "0123456789_-+./") != std::string::npos)
  {
    cout << "Invalid filename: " << path << endl;
    exit(1);
  }
}
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
  checkSanitized(path);
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
    checkSanitized(path);
    auto entry = filesystem::directory_entry(path);
    if (entry.exists())
      modified_ = to_time_t(entry.last_write_time());
  }

  void addDep(string path, DepNode* node)
  {
    checkSanitized(path);
    deps_[path] = node;
  }

  time_t rebuild()
  {
    if (endsWith(path_, ".o"))
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

      // external static libraries must come at the end
      vector<string> put_at_end;
      for (auto [name, val] : deps_)
      {
        if (endsWith(name, ".a"))
          put_at_end.push_back(name);
        else
          cmd += " " + name;
      }
      for (auto name : put_at_end)
        cmd += " " + name;

      cmd += " -o " + g_target_binary_name + " " + g_compile_end_libs;
      buildCmd(cmd);
      return time(0);
    }
    return modified_;
  }

  time_t rebuildIfNeeded(vector<string> loopguard)
  {
    loopguard.push_back(path_);
    if (loopguard.size() > 99)
    {
      cout << "(Likely) infinite dependency loop detected: ";
      for (auto x : loopguard)
        cout << x << " ==> ";
      cout << "..." << endl;
      exit(1);
    }

    time_t most_recent = modified_;
    for (auto [path, node] : deps_)
      most_recent = std::max(most_recent, node->rebuildIfNeeded(loopguard));

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
  string config_fname = "default.ccbuildfile";
  bool print_tree = false;
  if (argc > 1)
  {
    if (string(argv[1]) == "--verbose")
      print_tree = true;
    else
      config_fname = argv[1];
  }
  if (argc > 2)
  {
    if (string(argv[2]) == "--verbose")
      print_tree = true;
    else
      config_fname = argv[2];
  }
  loadConfig(config_fname);

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

  // actually add the target binary node to the map.
  g_all_nodes[g_target_binary_name] = target_binary;

  // a nice pretty view of your dependency "tree", if you're interested.
  // (entries are duplicated for each parent that depends on them).
  if (print_tree)
    g_all_nodes[g_target_binary_name].printTree(0);

  // we should now have a complete dependency graph. traverse it to build.
  g_all_nodes[g_target_binary_name].rebuildIfNeeded({});
  if (!g_target_changed)
    cout<<"ccsimplebuild: '"<<g_target_binary_name<<"' is up to date."<<endl;
}
