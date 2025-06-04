#include "appleappstoretransaction.h"

#import <StoreKit/StoreKit.h>

AppleAppStoreTransaction::AppleAppStoreTransaction(SKPaymentTransaction * transaction, QObject * parent) : AbstractTransaction(QString::fromNSString(transaction.transactionIdentifier), parent),
    _nativeTransaction(transaction)
{}

QString AppleAppStoreTransaction::productId() const
{
    return QString::fromNSString(_nativeTransaction.payment.productIdentifier);
}
