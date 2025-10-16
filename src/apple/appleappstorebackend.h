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
    void consumePurchase(Transaction transaction) override;
    void restorePurchases() override;
    bool canMakePurchases() const override;

    // Transaction processing control
    void enableProcessing() override;

    // Static early initialization from main.cpp (before any instances exist)
    static void initializeEarlyTransactionQueue();

    static AppleAppStoreBackend * s_currentInstance;

    // Internal access for EarlyTransactionObserver
    InAppPurchaseManager * iapManager() const { return _iapManager; }

private:
    
    InAppPurchaseManager * _iapManager = nullptr;

};

#endif // APPLEAPPSTOREBACKEND_H
