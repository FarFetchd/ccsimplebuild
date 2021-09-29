OutputBinaryFilename=clickitongue
CompileCommandPrefix=g++ -std=c++17 -O2 -DPA_LITTLE_ENDIAN -I../portaudio/include -I../portaudio/src/common -I../portaudio/src/os/unix -pthread -DCONFIG_STUFF=1 -Wall -Wno-sign-compare
LibrariesToLink=-lasound -lm
ExplicitDependency:
  Output=libportaudio.a
  CompileSuffix=
  DependsOn=
