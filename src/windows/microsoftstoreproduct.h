#ifndef MICROSOFTSTOREPRODUCT_H
#define MICROSOFTSTOREPRODUCT_H

#include <qt6purchasing/abstractproduct.h>

// Forward declaration to avoid WinRT headers in public interface
namespace winrt::Windows::Services::Store {
    struct StoreProduct;
}

class MicrosoftStoreProduct : public AbstractProduct
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Product)

public:
    explicit MicrosoftStoreProduct(QObject * parent = nullptr);

    // Microsoft Store specific methods
    void setStoreProduct(const winrt::Windows::Services::Store::StoreProduct& product);
    winrt::Windows::Services::Store::StoreProduct* storeProduct() const;

private:
    winrt::Windows::Services::Store::StoreProduct m_storeProduct{nullptr};
};

#endif // MICROSOFTSTOREPRODUCT_H
