

#include <nstd/String.h>

class User
{
public:
  String username;
  byte_t key[64];
  byte_t pwhmac[64];
};
