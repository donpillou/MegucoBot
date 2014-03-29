
#include <nstd/File.h>

#include "Engine.h"

Engine::Engine(uint32_t id, const String& path) : id(id), path(path)
{
  name = File::basename(path, ".exe");
}