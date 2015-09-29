
#pragma once

#include "Tools/Market.h"
#include "Tools/Websocket.h"

class BitstampBtcUsd : public Market
{
public:
  BitstampBtcUsd() : lastPingTime(0), lastTickerTimer(0) {}

  virtual String getChannelName() const {return String("Bitstamp/BTC/USD");}
  virtual bool_t connect();
  virtual void_t close() {websocket.close();}
  virtual bool_t isOpen() const {return websocket.isOpen();}
  virtual const String& getErrorString() const {return error;}
  virtual bool_t process(Callback& callback);

private:
  Websocket websocket;
  String error;
  int64_t localToServerTime;
  int64_t lastPingTime;
  int64_t lastTickerTimer;

  int64_t toServerTime(int64_t localTime) const {return localTime + localToServerTime;}
  bool_t handleStreamData(const Buffer& data, Callback& callback);
};
