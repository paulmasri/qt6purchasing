#ifndef MICROSOFTSTOREWORKERS_H
#define MICROSOFTSTOREWORKERS_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <windows.h>

class AbstractProduct;
class MicrosoftStoreBackend;

// Base worker class for Store operations
class StoreWorker : public QObject
{
    Q_OBJECT
public:
    explicit StoreWorker(HWND hwnd, QObject *parent = nullptr) 
        : QObject(parent), _hwnd(hwnd) {}

protected:
    HWND _hwnd;
};

// Worker for product registration/query
class StoreProductQueryWorker : public StoreWorker
{
    Q_OBJECT
public:
    StoreProductQueryWorker(const QString &productId, HWND hwnd)
        : StoreWorker(hwnd, nullptr), _productId(productId) {}

public slots:
    void performQuery();

signals:
    void queryComplete(bool success, const QVariantMap &productData);
    void finished();

private:
    QString _productId;
};

// Worker for purchase operations
class StorePurchaseWorker : public StoreWorker
{
    Q_OBJECT
public:
    StorePurchaseWorker(const QString &productId, HWND hwnd)
        : StoreWorker(hwnd, nullptr), _productId(productId) {}

public slots:
    void performPurchase();

signals:
    void purchaseComplete(bool success, int status, const QString &result);
    void finished();

private:
    QString _productId;
};

// Worker for restore purchases
class StoreRestoreWorker : public StoreWorker
{
    Q_OBJECT
public:
    explicit StoreRestoreWorker(HWND hwnd)
        : StoreWorker(hwnd, nullptr) {}

public slots:
    void performRestore();

signals:
    void restoreComplete(const QList<QVariantMap> &restoredProducts);
    void finished();
};

// Worker for querying all products
class StoreAllProductsWorker : public StoreWorker
{
    Q_OBJECT
public:
    explicit StoreAllProductsWorker(HWND hwnd)
        : StoreWorker(hwnd, nullptr) {}

public slots:
    void performQuery();

signals:
    void queryComplete(const QList<QVariantMap> &products);
    void finished();
};

// Worker for consumable fulfillment
class StoreConsumableFulfillmentWorker : public StoreWorker
{
    Q_OBJECT
public:
    StoreConsumableFulfillmentWorker(const QString &storeId, uint32_t quantity, HWND hwnd)
        : StoreWorker(hwnd, nullptr), _storeId(storeId), _quantity(quantity) {}

public slots:
    void performFulfillment();

signals:
    void fulfillmentComplete(bool success, const QString &result);
    void finished();

private:
    QString _storeId;
    uint32_t _quantity;
};

#endif // MICROSOFTSTOREWORKERS_H
