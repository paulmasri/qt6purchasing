#include "googleplaystoreproduct.h"

GooglePlayStoreProduct::GooglePlayStoreProduct(QObject * parent) : AbstractProduct(parent)
{}

void GooglePlayStoreProduct::setJson(QJsonObject json)
{
    if (_json == json)
        return;
    
    _json = json;
    emit jsonChanged();
}

