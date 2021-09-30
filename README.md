# ccsimplebuild
A highly specific and rigid, but maximally simple and effective, build tool for
C++. Automatically detects dependencies among .cc and .h files by scanning
#includes, and recompiles only what is necessary. `make` without the Makefile,
basically.

ccsimplebuild expects to be run in a directory full of .cc and .h files (must
use those suffixes, not cpp/hpp/etc). ccsimplebuild will create a subdir named
"obj/", where it will make a .o for every .cc file.

To use, you can either:

* Simply run `ccsimplebuild` in that directory. ccsimplebuild has default
  behavior that should work for simple projects. (See sample0).
* Run `ccsimplebuild` with a file named default.ccbuildfile existing in that same
  directory. See the sample directories for buildfile examples.
* Run `ccsimplebuild mycustombuildfilename` to load a buildfile named
  mycustombuildfilename, rather than default.ccbuildfile.

Suggested compilation command:
`g++ -g -std=c++17 ccsimplebuild.cc -o ccsimplebuild && sudo cp ccsimplebuild /usr/local/bin/`
