OutputBinaryFilename=my_other_fun_executable
CompileCommandPrefix=g++ -g -std=c++17 -Wall
LibrariesToLink=-latomic -lcurl
ExplicitDependency:
  Output=obj/weird.o
  CompileSuffix=-c somewhere/else/weird.cpp -o obj/weird.o
  DependsOn=one_of_ours.cc,one_of_ours.h,somewhere/else/weird.cpp,../reallyweird.h
ExplicitDependency:
  Output=obj/weird2.o
  CompileSuffix=-c somewhere/else/weird2.cpp -o obj/weird2.o
  DependsOn=example.cc,example.h,somewhere/else/weird2.cpp,../reallyweird.h
