#ifndef APPLEAPPSTOREBACKEND_H
#define APPLEAPPSTOREBACKEND_H

#include <qt6purchasing/abstractstorebackend.h>

Q_FORWARD_DECLARE_OBJC_CLASS(InAppPurchaseManager);

class AppleAppStoreBackend : public AbstractStoreBackend
{
    Q_OBJECT
	QML_ELEMENT

public:
    AppleAppStoreBackend(QObject * parent = nullptr);
    ~AppleAppStoreBackend();

    void startConnection() override;
    void registerProduct(AbstractProduct * product) override;
    void purchaseProduct(AbstractProduct * product) override;
    void consumePurchase(AbstractTransaction * transaction) override;

//    void emitProductRegistered(AbstractProduct * product);
private:
    InAppPurchaseManager * _iapManager = nullptr;

};

#endif // APPLEAPPSTOREBACKEND_H
