# ccsimplebuild
A highly specific and fragile, but also simple and effective, build tool.

ccsimplebuild expects to be run in a directory full of .cc and .h files, with a
subdir named "obj/" where every .cc file gets a corresponding .o.

To use, set `kTargetBinaryName`, `kCompileCmdPrefix`, and `kCompileEndLibs`
appropriately. Perhaps someday I'll have these be read from a Makefile-esque
config file, rather than hardcoded. Compile with
`g++ -std=c++17 -o ccsimplebuild ccsimplebuild.cc`, then run `./ccsimplebuild`
inside your source dir.

ccsimplebuild also has limited support for compiling source files outside of the
main source directory into the project - see `loadExplicitDeps()` for an
example of loading a single one of these. This could also someday go in a
config file.
