
#pragma once

#include <nstd/String.h>

class MarketAdapter
{
public:
  uint32_t getId() const {return id;}
  const String& getName() const {return name;}
  const String& getPath() const {return path;}
  const String& getCurrencyBase() const {return currencyBase;}
  const String& getCurrencyComm() const {return currencyComm;}

public:
  MarketAdapter(uint32_t id, const String& name, const String& path, const String& currencyBase, const String& currencyComm);

private:
  uint32_t id;
  String name;
  String path;
  String currencyBase;
  String currencyComm;
};
