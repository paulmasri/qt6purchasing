#include "appleappstoretransaction.h"

#import <StoreKit/StoreKit.h>

AppleAppStoreTransaction::AppleAppStoreTransaction(Transaction * transaction, QObject * parent) : AbstractTransaction(QString::number([transaction originalTransactionID]), parent),
    _nativeTransaction(transaction)
{}

QString AppleAppStoreTransaction::productId() const
{
    if (_nativeTransaction) {
        return QString::fromNSString([_nativeTransaction productID]);
    }
    return QString();
}
