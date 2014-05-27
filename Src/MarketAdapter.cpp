
#include "MarketAdapter.h"
#include "BotProtocol.h"
#include "ClientHandler.h"

MarketAdapter::MarketAdapter(uint32_t id, const String& name, const String& path, const String& currencyBase, const String& currencyComm) :
  __id(id), name(name), path(path), currencyBase(currencyBase), currencyComm(currencyComm) {}

void_t MarketAdapter::getEntity(BotProtocol::MarketAdapter& marketAdapter) const
{
  marketAdapter.entityType = BotProtocol::marketAdapter;
  marketAdapter.entityId = __id;
  BotProtocol::setString(marketAdapter.name, name);
  BotProtocol::setString(marketAdapter.currencyBase, currencyBase);
  BotProtocol::setString(marketAdapter.currencyComm, currencyComm);
}
