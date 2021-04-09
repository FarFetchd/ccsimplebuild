# ccsimplebuild
A highly specific and fragile, but also simple and effective, build tool.

ccsimplebuild expects to be run in a directory full of .cc and .h files (must
use those suffixes, not cpp/hpp/etc). The directory should have a subdir named
"obj/" where every .cc file gets a corresponding .o.

To use, fill in a ccsimple.buildfile in that same directory (or don't!) See the
sample directories for examples. Run `ccsimplebuild` in there.

ccsimplebuild also has limited support for compiling source files outside of the
main source directory into the project - see sample2.
