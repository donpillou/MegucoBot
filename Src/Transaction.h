
#pragma once

#include <nstd/Base.h>
#include <nstd/Variant.h>

#include "BotProtocol.h"

class Transaction
{
public:
  Transaction(uint32_t id, double price, double amount, double fee, BotProtocol::Transaction::Type type);
  Transaction(const Variant& variant);
  void_t toVariant(Variant& variant);

  uint32_t getId() const {return id;}
  double getPrice() const {return price;}
  double getAmount() const {return amount;}
  double getFee() const {return fee;}
  BotProtocol::Transaction::Type getType() const {return type;}
  timestamp_t getDate() const {return date;}

private:
  uint32_t id;
  double price;
  double amount;
  double fee;
  BotProtocol::Transaction::Type type;
  timestamp_t date;
};
