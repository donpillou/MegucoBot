
#include "Tools/Math.h"

#include "BitstampUsd.h"
#if 0
BitstampMarket::BitstampMarket(const String& userName, const String& key, const String& secret) :
  userName(userName), key(key), secret(secret),
  balanceLoaded(false), lastNonce(0) {}

bool_t BitstampMarket::loadBalanceAndFee()
{
  if(balanceLoaded)
    return true;
  BotProtocol::MarketBalance balance;
  if(!loadBalance(balance))
    return false;
  return true;
}

bool_t BitstampMarket::createOrder(double amount, double price, BotProtocol::Order& order)
{
  if(!loadBalanceAndFee())
    return false;

  double maxAmount = Math::abs(amount > 0. ? getMaxBuyAmout(price) : getMaxSellAmout());
  if(Math::abs(amount) > maxAmount)
    amount = amount > 0. ? maxAmount : -maxAmount;

  HashMap<String, Variant> args;
  args.append("amount", Math::abs(amount));
  args.append("price", price);

  const char_t* url = amount > 0. ? "https://www.bitstamp.net/api/buy/" : "https://www.bitstamp.net/api/sell/";
  Variant result;
  if(!request(url, false, args, result))
    return false;

  const HashMap<String, Variant>& orderData = result.toMap();

  order.id = orderData.find("id")->toString();
  QString type = orderData["type"].toString();
  if(type != "0" && type != "1")
  {
    error = "Received invalid response.";
    return false;
  }

  bool buy = type == "0";

  QString dateStr = orderData["datetime"].toString();
  int lastDot = dateStr.lastIndexOf('.');
  if(lastDot >= 0)
    dateStr.resize(lastDot);
  QDateTime date = QDateTime::fromString(dateStr, "yyyy-MM-dd hh:mm:ss");
  date.setTimeSpec(Qt::UTC);
  order.date = date.toTime_t();

  order.price = orderData["price"].toDouble();
  order.amount = fabs(orderData["amount"].toDouble());
  if(!buy)
    order.amount = -order.amount;
  order.total = getOrderCharge(order.amount, order.price);
  order.fee = qAbs(order.total) - qAbs(order.price * order.amount);
  this->orders.insert(order.id, order);

  // update balance
  if(order.amount > 0) // buy order
  {
    balance.availableUsd -= order.total;
    balance.reservedUsd += order.total;
  }
  else // sell order
  {
    balance.availableBtc -= order.amount;
    balance.reservedBtc += order.amount;
  }
  return true;
}

bool BitstampMarket::cancelOrder(const QString& id)
{
  QHash<QString, Market::Order>::const_iterator it = orders.find(id);
  if(it == orders.end())
  {
    error = "Unknown order.";
    return false; // unknown order
  }
  const Market::Order& order = it.value();

  QVariantMap args;
  args["id"] = id;
  VariantBugWorkaround result;
  if(!request("https://www.bitstamp.net/api/cancel_order/", false, args, result))
    return false;

  // update balance
  if(order.amount > 0) // buy order
  {
    double orderValue = order.amount * order.price;
    balance.availableUsd += orderValue;
    balance.reservedUsd -= orderValue;
  }
  else // sell order
  {
    balance.availableBtc += order.amount;
    balance.reservedBtc -= order.amount;
  }
  return true;
}

bool BitstampMarket::loadOrders(QList<Order>& orders)
{
  if(!loadBalanceAndFee())
    return false;

  VariantBugWorkaround result;
  if(!request("https://www.bitstamp.net/api/open_orders/", false, QVariantMap(), result))
    return false;

  QVariantList ordersData = result.toList();
  orders.reserve(ordersData.size());
  this->orders.clear();
  this->orders.reserve(ordersData.size());
  foreach(const QVariant& orderDataVar, ordersData)
  {
    QVariantMap orderData = orderDataVar.toMap();
    
    QString id = orderData["id"].toString();
    QString type = orderData["type"].toString();
    if(type != "0" && type != "1")
      continue;

    orders.append(Market::Order());
    Market::Order& order = orders.back();
    order.id = id;
    bool buy = type == "0";

    QString dateStr = orderData["datetime"].toString();
    QDateTime date = QDateTime::fromString(dateStr, "yyyy-MM-dd hh:mm:ss");
    date.setTimeSpec(Qt::UTC);
    order.date = date.toTime_t();

    order.price = orderData["price"].toDouble();
    order.amount = fabs(orderData["amount"].toDouble());
    if(!buy)
      order.amount = -order.amount;
    order.total = getOrderCharge(order.amount, order.price);
    this->orders.insert(order.id, order);
  }
  return true;
}

bool BitstampMarket::loadBalance(Balance& balance)
{
  VariantBugWorkaround result;
  if(!request("https://www.bitstamp.net/api/balance/", false, QVariantMap(), result))
    return false;

  QVariantMap balanceData = result.toMap();
  balance.reservedUsd = balanceData["usd_reserved"].toDouble();
  balance.reservedBtc = balanceData["btc_reserved"].toDouble();
  balance.availableUsd = balanceData["usd_available"].toDouble();
  balance.availableBtc = balanceData["btc_available"].toDouble();
  balance.fee =  balanceData["fee"].toDouble() * 0.01;
  this->balance = balance;
  this->balanceLoaded = true;
  return true;
}

bool BitstampMarket::loadTransactions(QList<Transaction>& transactions)
{
  VariantBugWorkaround result;
  if(!request("https://www.bitstamp.net/api/user_transactions/", false, QVariantMap(), result))
    return false;

  QVariantList transactionData = result.toList();
  transactions.reserve(transactionData.size());
  foreach(const QVariant& transactionDataVar, transactionData)
  {
    QVariantMap transactionData = transactionDataVar.toMap();
    
    QString id = transactionData["id"].toString();
    QString type = transactionData["type"].toString();
    if(type != "2")
      continue;

    transactions.append(Market::Transaction());
    Market::Transaction& transaction = transactions.back();
    transaction.id = id;

    QString dateStr = transactionData["datetime"].toString();
    QDateTime date = QDateTime::fromString(dateStr, "yyyy-MM-dd hh:mm:ss");
    date.setTimeSpec(Qt::UTC);
    transaction.date = date.toTime_t();
    transaction.fee = transactionData["fee"].toDouble();

    double value = transactionData["usd"].toDouble();
    bool buy = value < 0.;
    transaction.total = buy ? -(fabs(value) + transaction.fee) : (fabs(value) - transaction.fee);
    transaction.amount = fabs(transactionData["btc"].toDouble());
    if(!buy)
      transaction.amount = -transaction.amount;
    transaction.price = fabs(value) / fabs(transaction.amount);
  }

  return true;
}

double BitstampMarket::getMaxSellAmout() const
{
  return balance.availableBtc;
}

double BitstampMarket::getMaxBuyAmout(double price) const
{
  double fee = balance.fee; // e.g. 0.0044
  double availableUsd = balance.availableUsd;
  double additionalAvailableUsd = 0.; //floor(canceledAmount * canceledPrice * (1. + fee) * 100.) / 100.;
  double usdAmount = availableUsd + additionalAvailableUsd;
  double result = floor(((100. / ( 100. + (fee * 100.))) * usdAmount) * 100.) / 100.;
  result /= price;
  result = floor(result * 100000000.) / 100000000.;
  return result;
}

double BitstampMarket::getOrderCharge(double amount, double price) const
{
  if(amount < 0.) // sell order
    return floor(-amount * price / (1. + balance.fee) * 100.) / 100.;
  else // buy order
    return floor(amount * price * (1. + balance.fee) * -100.) / 100.;
}

bool BitstampMarket::request(const char* url, bool isPublic, const QVariantMap& params, QVariant& result)
{
  avoidSpamming();

  QByteArray buffer;
  if (isPublic)
  {
    if(!httpRequest.get(url, buffer))
    {
      error = httpRequest.getLastError();
      return false;
    }
  }
  else
  {
    QByteArray clientId(this->userName.toUtf8());
    QByteArray key(this->key.toUtf8());
    QByteArray secret(this->secret.toUtf8());

    quint64 newNonce = QDateTime::currentDateTime().toTime_t();
    if(newNonce <= lastNonce)
      newNonce = lastNonce + 1;
    lastNonce = newNonce;

    QByteArray nonce(QString::number(newNonce).toAscii());
    QByteArray message = nonce + clientId + key;
    QByteArray signature = Sha256::hmac(secret, message).toHex().toUpper();
    
    QMap<QString, QString> formData;
    formData["key"] = key;
    formData["signature"] = signature;
    formData["nonce"] = nonce;
    for(QVariantMap::const_iterator j = params.begin(), end = params.end(); j != end; ++j)
      formData[j.key()] = j.value().toString();

    if(!httpRequest.post(url, formData, buffer))
    {
      error = httpRequest.getLastError();
      return false;
    }
  }

  if(strcmp(url, "https://www.bitstamp.net/api/cancel_order/") == 0) // does not return JSON if successful
  {
    QString answer(buffer);
    if(answer != "true")
    {
      result = Json::parse(buffer);
      if(!result.toMap().contains("error"))
      {
        QVariantMap cancelData;
        cancelData["error"] = answer;
        result = cancelData;
      }
    }
    else
      result = true;
  }
  else
    result = Json::parse(buffer);

  if(!result.isValid())
  {
    error = "Received unparsable data.";
    return false;
  }
  else if(result.toMap().contains("error"))
  {
    QStringList errors;
    struct ErrorStringCollector
    {
      static void collect(QVariant var, QStringList& errors)
      {
        switch(var.type())
        {
        case QVariant::String:
          errors.push_back(var.toString());
          break;
        case QVariant::List:
          {
            QVariantList list = var.toList();
            foreach(const QVariant& var, list)
              collect(var, errors);
          }
          break;
        case QVariant::Map:
          {
            QVariantMap map = var.toMap();
            for(QVariantMap::iterator i = map.begin(), end = map.end(); i != end; ++i)
              collect(i.value(), errors);
          }
          break;
        default:
          break;
        }
      }
    };
    ErrorStringCollector::collect(result, errors);
    error = errors.join(" ");
    return false;
  }
  else
    return true;
}

void BitstampMarket::avoidSpamming()
{
  const qint64 queryDelay = 1337LL;
  QDateTime now = QDateTime::currentDateTime();
  qint64 elapsed = lastRequestTime.isNull() ? queryDelay : lastRequestTime.msecsTo(now);
  if(elapsed < queryDelay)
  {
    QMutex mutex;
    QWaitCondition condition;
    condition.wait(&mutex, queryDelay - elapsed); // wait without processing messages while waiting
    lastRequestTime = now;
    lastRequestTime  = lastRequestTime.addMSecs(queryDelay - elapsed);
  }
  else
    lastRequestTime = now;
  // TODO: allow more than 1 request per second but limit requests to 600 per 10 minutes
}

BitstampMarket::VariantBugWorkaround::~VariantBugWorkaround()
{
  // Something is wrong with Qt. The QVariant destructor crashes when it tries to free a variant list.
  // So, lets prevent the destructor from doing so:
  struct VariantListDestructorBugfix
  {
    static void findLists(const QVariant& var, QList<QVariantList>& lists)
    {
      switch(var.type())
      {
      case QVariant::List:
        {
          QVariantList list = var.toList();
          lists.append(list);
          foreach(const QVariant& var, list)
            findLists(var, lists);
        }
        break;
      case QVariant::Map:
        {
          QVariantMap map = var.toMap();
          for(QVariantMap::iterator i = map.begin(), end = map.end(); i != end; ++i)
            findLists(i.value(), lists);
        }
        break;
      default:
        break;
      }
    }
  };
  QList<QVariantList> lists;
  VariantListDestructorBugfix::findLists(*this, lists);
  this->clear();
}
#endif