#include "microsoftstoreproduct.h"
#include <winrt/Windows.Services.Store.h>

using namespace winrt::Windows::Services::Store;

MicrosoftStoreProduct::MicrosoftStoreProduct(QObject * parent) 
    : AbstractProduct(parent), m_storeProduct{nullptr}
{
}

void MicrosoftStoreProduct::setStoreProduct(const StoreProduct& product)
{
    m_storeProduct = product;
}

StoreProduct* MicrosoftStoreProduct::storeProduct() const
{
    return m_storeProduct ? &m_storeProduct : nullptr;
}
