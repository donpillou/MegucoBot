
#pragma once

#include <nstd/String.h>

class Market
{
public:
  uint32_t getId() const {return id;}
  const String& getName() const {return name;}
  const String& getCurrencyBase() const {return currencyBase;}
  const String& getCurrencyComm() const {return currencyBase;}

public:
  Market(uint32_t id, const String& name, const String& currencyBase, const String& currencyComm);

private:
  uint32_t id;
  String name;
  String currencyBase;
  String currencyComm;
};
