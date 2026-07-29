#ifndef MICROSOFTSTOREPRODUCT_H
#define MICROSOFTSTOREPRODUCT_H

#include <qt6purchasing/abstractproduct.h>

class MicrosoftStoreProduct : public AbstractProduct
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Product)

public:
    explicit MicrosoftStoreProduct(QObject * parent = nullptr);
};

#endif // MICROSOFTSTOREPRODUCT_H
