#include "microsoftstoreproduct.h"
#include "windowsstorewrappers.h"

MicrosoftStoreProduct::MicrosoftStoreProduct(QObject * parent) 
    : AbstractProduct(parent), _storeProduct(nullptr)
{
}

MicrosoftStoreProduct::~MicrosoftStoreProduct()
{
    delete _storeProduct;
}

void MicrosoftStoreProduct::setStoreProduct(WindowsStoreProductWrapper * product)
{
    delete _storeProduct;
    _storeProduct = product;
}

WindowsStoreProductWrapper * MicrosoftStoreProduct::storeProduct() const
{
    return _storeProduct;
}
