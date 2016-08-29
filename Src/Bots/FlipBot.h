
#pragma once

#include "Tools/Bot.h"

class FlipBot : public Bot
{
public:

private:
  class Session : public Bot::Session
  {
  public:
    Session(Broker& broker);

  private:
    Broker& broker;

    //virtual ~Session() {}

    void_t updateBalance();

    void_t checkBuy(const Bot::Trade& trade);
    void_t checkSell(const Bot::Trade& trade);

  private: // Bot::Session
    virtual void_t handleTrade(const Trade& trade, int64_t tradeAge);
    virtual void_t handleBuy(uint64_t orderId, const Transaction& transaction);
    virtual void_t handleSell(uint64_t orderId, const Transaction& transaction);
    virtual void_t handleBuyTimeout(uint64_t orderId);
    virtual void_t handleSellTimeout(uint64_t orderId);
    virtual void_t handlePropertyUpdate(const Property& property) {};
    virtual void_t handleAssetUpdate(const Asset& asset);
    virtual void_t handleAssetRemoval(const Asset& asset);
  };

public: // Bot
  virtual Session* createSession(Broker& broker) {return new Session(broker);};
  virtual int64_t getMaxTradeAge() const {return 0;}
};
