#ifndef APPLEAPPSTOREBACKEND_H
#define APPLEAPPSTOREBACKEND_H

#include <qt6purchasing/abstractstorebackend.h>

Q_FORWARD_DECLARE_OBJC_CLASS(InAppPurchaseManager);

class AppleAppStoreBackend : public AbstractStoreBackend
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Store)

public:
    AppleAppStoreBackend(QObject * parent = nullptr);
    ~AppleAppStoreBackend();

    void startConnection() override;
    void registerProduct(AbstractProduct * product) override;
    void purchaseProduct(AbstractProduct * product) override;
    void consumePurchase(AbstractTransaction * transaction) override;
    bool canMakePurchases() const override;

    static void initializeEarly();

    static AppleAppStoreBackend * s_currentInstance;

private:
    static PurchaseError mapStoreKitErrorToPurchaseError(int errorCode);
    static QString getStoreKitErrorMessage(int errorCode);
    
    InAppPurchaseManager * _iapManager = nullptr;

};

#endif // APPLEAPPSTOREBACKEND_H
