
#pragma once

#include <nstd/String.h>

#include "BotProtocol.h"

class ClientHandler;

class MarketAdapter
{
public:
  uint32_t getId() const {return __id;}
  const String& getName() const {return name;}
  const String& getPath() const {return path;}
  const String& getCurrencyBase() const {return currencyBase;}
  const String& getCurrencyComm() const {return currencyComm;}

  void_t getEntity(BotProtocol::MarketAdapter& marketAdapater) const;

public:
  MarketAdapter(uint32_t id, const String& name, const String& path, const String& currencyBase, const String& currencyComm);

private:
  uint32_t __id;
  String name;
  String path;
  String currencyBase;
  String currencyComm;
};
