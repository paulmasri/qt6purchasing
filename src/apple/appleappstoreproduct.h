#ifndef APPLEAPPSTOREPRODUCT_H
#define APPLEAPPSTOREPRODUCT_H

#include <qt6purchasing/abstractproduct.h>

Q_FORWARD_DECLARE_OBJC_CLASS(SKProduct);

class AppleAppStoreProduct : public AbstractProduct
{
    Q_OBJECT
	QML_ELEMENT

public:
    AppleAppStoreProduct(QObject * parent = nullptr);

    SKProduct * nativeProduct() { return _nativeProduct; }
    void setNativeProduct(SKProduct * np);

private:
    SKProduct * _nativeProduct = nullptr;
};

#endif // APPLEAPPSTOREPRODUCT_H
