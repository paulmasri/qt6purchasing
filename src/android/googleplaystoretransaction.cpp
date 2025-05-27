#include "googleplaystoretransaction.h"

GooglePlayStoreTransaction::GooglePlayStoreTransaction(QJsonObject json, QObject * parent) : AbstractTransaction(json["orderId"].toString(), parent),
    _json(json)
{}

QString GooglePlayStoreTransaction::productId() const
{
    return _json["productId"].toString();
}
