#include "appleappstoreproduct.h"

AppleAppStoreProduct::AppleAppStoreProduct(QObject * parent) : AbstractProduct(parent)
{}

void AppleAppStoreProduct::setNativeProduct(Product * product)
{
    _nativeProduct = product;
}
