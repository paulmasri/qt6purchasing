#ifndef WINDOWSSTOREWRAPPERS_H
#define WINDOWSSTOREWRAPPERS_H

#include <QString>
#include <winrt/base.h>
#include <winrt/Windows.Services.Store.h>

using namespace winrt::Windows::Services::Store;

// Windows Store Product Wrapper - handles WinRT StoreProduct objects
class WindowsStoreProductWrapper {
public:
    WindowsStoreProductWrapper(const StoreProduct& product) : _storeProduct(product) {}
    
    ~WindowsStoreProductWrapper() = default;
    
    QString title() const {
        return QString::fromStdWString(_storeProduct.Title().c_str());
    }
    
    QString description() const {
        return QString::fromStdWString(_storeProduct.Description().c_str());
    }
    
    QString price() const {
        return QString::fromStdWString(_storeProduct.Price().FormattedPrice().c_str());
    }
    
    QString productKind() const {
        return QString::fromStdWString(_storeProduct.ProductKind().c_str());
    }
    
    const StoreProduct& nativeProduct() const {
        return _storeProduct;
    }
    
private:
    StoreProduct _storeProduct;
};

// Windows Store Purchase Result Wrapper - handles WinRT StorePurchaseResult objects
class WindowsStorePurchaseResultWrapper {
public:
    WindowsStorePurchaseResultWrapper(const StorePurchaseResult& result) : _purchaseResult(result) {}
    
    ~WindowsStorePurchaseResultWrapper() = default;
    
    StorePurchaseStatus status() const {
        return _purchaseResult.Status();
    }
    
    const StorePurchaseResult& nativeResult() const {
        return _purchaseResult;
    }
    
private:
    StorePurchaseResult _purchaseResult;
};

#endif // WINDOWSSTOREWRAPPERS_H
