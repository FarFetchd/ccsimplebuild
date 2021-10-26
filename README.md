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

Suggested compilation and installation command:
`g++ -std=c++17 ccsimplebuild.cc -o ccsimplebuild -lstdc++fs && sudo cp ccsimplebuild /usr/local/bin/`

# build.sh
If you publish code meant to be built with ccsimplebuild, and don't want people
to be turned off by an unfamiliar build tool, include build.sh: it correctly
interprets the ccbuildfile format, so that `./build.sh` works just as well as
`ccsimplebuild` for a first time compilation. It politely points out that they
can switch to ccsimplebuild for smoother recompilation.
