#ifndef APPLEAPPSTOREPRODUCT_H
#define APPLEAPPSTOREPRODUCT_H

#include <qt6purchasing/abstractproduct.h>

Q_FORWARD_DECLARE_OBJC_CLASS(Product);

class AppleAppStoreProduct : public AbstractProduct
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Product)

public:
    AppleAppStoreProduct(QObject * parent = nullptr);

    Product * nativeProduct() { return _nativeProduct; }
    void setNativeProduct(Product * product);

private:
    Product * _nativeProduct = nullptr;
};

#endif // APPLEAPPSTOREPRODUCT_H
