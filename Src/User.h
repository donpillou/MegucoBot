
#pragma once

#include <nstd/String.h>

class User
{
public:
  String userName;
  byte_t key[64];
  byte_t pwhmac[64];
};
