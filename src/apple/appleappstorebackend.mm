#include "appleappstorebackend.h"
#include "appleappstoreproduct.h"
#include "appleappstoretransaction.h"

#include <QDebug>
#include <QJsonObject>
#include <QThread>
#include <QCoreApplication>

#import <StoreKit/StoreKit.h>

// Qt 6.8 requires iOS 16+ so StoreKit 2 is always available

AppleAppStoreBackend* AppleAppStoreBackend::s_currentInstance = nullptr;

@interface InAppPurchaseManager : NSObject

-(id)init;
-(void)requestProductData:(NSString *)identifier;
-(void)startTransactionListener;

@end

@implementation InAppPurchaseManager

-(id)init {
    if (self = [super init]) {
        [self startTransactionListener];
    }
    return self;
}

-(void)dealloc
{
    // StoreKit 2 automatically handles cleanup
}

-(void)requestProductData:(NSString *)identifier
{
    NSSet<NSString *> * productIds = [NSSet<NSString *> setWithObject:identifier];

    [Product productsForIdentifiers:productIds completionHandler:^(NSArray<Product *> * _Nonnull products, NSError * _Nullable error) {
        AppleAppStoreBackend * backend = AppleAppStoreBackend::s_currentInstance;
        if (!backend) {
            qCritical() << "Apple Store product callback received but backend instance is null";
            return;
        }

        if (error) {
            qWarning() << "Product request failed:" << QString::fromNSString(error.localizedDescription);
            if (backend->product(QString::fromNSString(identifier)))
                backend->product(QString::fromNSString(identifier))->setStatus(AbstractProduct::Unknown);
            return;
        }

        if (products.count == 0) {
            qWarning() << "No products found for identifier:" << QString::fromNSString(identifier);
            if (backend->product(QString::fromNSString(identifier)))
                backend->product(QString::fromNSString(identifier))->setStatus(AbstractProduct::Unknown);
            return;
        }

        Product * nativeProduct = [products firstObject];
        AppleAppStoreProduct * qtProduct = reinterpret_cast<AppleAppStoreProduct*>(backend->product(QString::fromNSString(nativeProduct.id)));

        if (qtProduct) {
            qtProduct->setNativeProduct(nativeProduct);
            qtProduct->setDescription(QString::fromNSString(nativeProduct.description));
            qtProduct->setPrice(QString::fromNSString(nativeProduct.displayPrice));
            qtProduct->setTitle(QString::fromNSString(nativeProduct.displayName));
            qtProduct->setStatus(AbstractProduct::Registered);

            QMetaObject::invokeMethod(backend, "productRegistered", Qt::AutoConnection, Q_ARG(AbstractProduct*, qtProduct));
        }
    }];
}

-(void)startTransactionListener
{
    // Start listening for transaction updates
    [Transaction addTransactionObserver:^(NSArray<Transaction *> * _Nonnull transactions) {
        AppleAppStoreBackend * backend = AppleAppStoreBackend::s_currentInstance;
        if (!backend) {
            qCritical() << "Transaction callback received but backend instance is null";
            return;
        }

        for (Transaction * transaction in transactions) {
            // Verify the transaction
            if ([transaction verifyTransaction]) {
                AppleAppStoreTransaction * ta = new AppleAppStoreTransaction(transaction, backend);

                switch (transaction.transactionState) {
                case TransactionStatePurchased:
                    QMetaObject::invokeMethod(backend, "purchaseSucceeded", Qt::AutoConnection, Q_ARG(AbstractTransaction*, ta));
                    break;
                case TransactionStateFailed:
                    QMetaObject::invokeMethod(backend, "purchaseFailed", Qt::AutoConnection, Q_ARG(int, transaction.error.code));
                    break;
                case TransactionStateRestored:
                    QMetaObject::invokeMethod(backend, "purchaseRestored", Qt::AutoConnection, Q_ARG(AbstractTransaction*, ta));
                    break;
                case TransactionStatePending:
                    // Handle pending transactions if needed
                    break;
                }
            } else {
                qWarning() << "Transaction verification failed for transaction ID:" << transaction.id;
            }
        }
    }];
}


@end

AppleAppStoreBackend::AppleAppStoreBackend(QObject * parent) : AbstractStoreBackend(parent)
{
    this->startConnection();

    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
    s_currentInstance = this;
}

AppleAppStoreBackend::~AppleAppStoreBackend()
{
    if (s_currentInstance == this)
        s_currentInstance = nullptr;
}

void AppleAppStoreBackend::startConnection()
{
    _iapManager = [[InAppPurchaseManager alloc] init];
    setConnected(_iapManager != nullptr);
}

void AppleAppStoreBackend::registerProduct(AbstractProduct * product)
{
    [_iapManager requestProductData:(product->identifier().toNSString())];
}

void AppleAppStoreBackend::purchaseProduct(AbstractProduct * product)
{
    AppleAppStoreProduct * qtProduct = reinterpret_cast<AppleAppStoreProduct*>(product);
    Product * nativeProduct = qtProduct->nativeProduct();

    if (nativeProduct) {
        [Product purchaseProduct:nativeProduct completionHandler:^(Product * _Nullable purchasedProduct, NSError * _Nullable error) {
            if (error) {
                QMetaObject::invokeMethod(this, "purchaseFailed", Qt::AutoConnection, Q_ARG(int, error.code));
            } else {
                qDebug() << "Purchase initiated for product:" << QString::fromNSString(purchasedProduct.id);
            }
        }];
    } else {
        qWarning() << "Cannot purchase product - no native product available";
    }
}

void AppleAppStoreBackend::consumePurchase(AbstractTransaction * transaction)
{
    // StoreKit 2: Transactions are automatically finished when verified
    qDebug() << "Transaction consumed (auto-finished)";
    emit purchaseConsumed(transaction);
}
