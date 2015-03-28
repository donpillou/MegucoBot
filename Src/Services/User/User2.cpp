
#include "User2.h"
#include "Market2.h"

User2::~User2()
{
  for(List<Market2*>::Iterator i = markets.begin(), end = markets.end(); i != end; ++i)
    delete *i;
}

Market2* User2::createMarket(const meguco_user_market_entity& marketEntity, const String& executable)
{
  Market2* market = new Market2(marketEntity, executable);
  markets.append(market);
  return market;
}
