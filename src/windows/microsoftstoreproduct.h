#ifndef MICROSOFTSTOREPRODUCT_H
#define MICROSOFTSTOREPRODUCT_H

#include <qt6purchasing/abstractproduct.h>

// Forward declaration to avoid WinRT headers in public interface
class WindowsStoreProductWrapper;

class MicrosoftStoreProduct : public AbstractProduct
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Product)

public:
    explicit MicrosoftStoreProduct(QObject * parent = nullptr);
    ~MicrosoftStoreProduct();

    // Microsoft Store specific methods
    void setStoreProduct(WindowsStoreProductWrapper * product);
    WindowsStoreProductWrapper * storeProduct() const;

private:
    WindowsStoreProductWrapper * _storeProduct = nullptr;
};

#endif // MICROSOFTSTOREPRODUCT_H
