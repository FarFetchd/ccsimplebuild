OutputBinaryFilename=clickitongue
CompileCommandPrefix=g++ -std=c++17 -O2 -DPA_LITTLE_ENDIAN -pthread -DCONFIG_STUFF=1 -Wall -Wno-sign-compare
LibrariesToLink=-lasound -lm
ExplicitDependency:
  Output=libportaudio.a
  CompileSuffix=
  DependsOn=
