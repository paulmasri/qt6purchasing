#include "appleappstoreproduct.h"

AppleAppStoreProduct::AppleAppStoreProduct(QObject * parent) : AbstractProduct(parent)
{}

void AppleAppStoreProduct::setNativeProduct(SKProduct * np)
{
    if (_nativeProduct == np)
        return;
    
    _nativeProduct = np;
}
