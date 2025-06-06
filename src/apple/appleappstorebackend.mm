#include "appleappstorebackend.h"
#include "appleappstoreproduct.h"
#include "appleappstoretransaction.h"

#include <QDebug>
#include <QJsonObject>

#import <StoreKit/StoreKit.h>

AppleAppStoreBackend* AppleAppStoreBackend::s_currentInstance = nullptr;

@interface InAppPurchaseManager : NSObject <SKProductsRequestDelegate, SKPaymentTransactionObserver>
{
    NSMutableArray<SKPaymentTransaction *> *pendingTransactions;
}

-(id)init;
-(void)requestProductData:(NSString *)identifier;

@end

@implementation InAppPurchaseManager

-(id)init {
    if (self = [super init]) {
        pendingTransactions = [[NSMutableArray<SKPaymentTransaction *> alloc] init];
        [[SKPaymentQueue defaultQueue] addTransactionObserver:self];
    }
    return self;
}

-(void)dealloc
{
    [[SKPaymentQueue defaultQueue] removeTransactionObserver:self];
    [pendingTransactions release];
    [super dealloc];
}

-(void)requestProductData:(NSString *)identifier
{
    NSSet<NSString *> * productId = [NSSet<NSString *> setWithObject:identifier];
    SKProductsRequest * productsRequest = [[SKProductsRequest alloc] initWithProductIdentifiers:productId];
    productsRequest.delegate = self;
    [productsRequest start];
}

//SKProductsRequestDelegate
-(void)productsRequest:(SKProductsRequest *)request didReceiveResponse:(SKProductsResponse *)response
{
    AppleAppStoreBackend * backend = AppleAppStoreBackend::s_currentInstance;
    if (!backend) {
        qCritical() << "Apple Store product callback received but backend instance is null";
        return;
    }

    NSArray<SKProduct *> * skProducts = response.products;
    SKProduct * skProduct = [skProducts count] == 1 ? [[skProducts firstObject] retain] : nil;

    if (skProduct == nil) {
        //Invalid product ID
        NSString * invalidId = [response.invalidProductIdentifiers firstObject];
        if (backend->product(QString::fromNSString(invalidId)))
            backend->product(QString::fromNSString(invalidId))->setStatus(AbstractProduct::Unknown);
    } else {
        //Valid product query
        AppleAppStoreProduct * product = reinterpret_cast<AppleAppStoreProduct*>( backend->product(QString::fromNSString(skProduct.productIdentifier)) );

        if (product) {
            // formatting price string
            NSNumberFormatter *numberFormatter = [[NSNumberFormatter alloc] init];
            [numberFormatter setFormatterBehavior:NSNumberFormatterBehavior10_4];
            [numberFormatter setNumberStyle:NSNumberFormatterCurrencyStyle];
            [numberFormatter setLocale:skProduct.priceLocale];
            NSString * localizedPrice = [numberFormatter stringFromNumber:skProduct.price];
            [numberFormatter release];

            product->setNativeProduct(skProduct);
            product->setDescription(QString::fromNSString(skProduct.localizedDescription));
            product->setPrice(QString::fromNSString(localizedPrice));
            product->setTitle(QString::fromNSString(skProduct.localizedTitle));
            product->setStatus(AbstractProduct::Registered);

            QMetaObject::invokeMethod(backend, "productRegistered", Qt::AutoConnection, Q_ARG(AbstractProduct*, product));
        } else {
        }
        
        [skProduct release];
    }

    [request release];
}

//SKPaymentTransactionObserver
- (void)paymentQueue:(SKPaymentQueue *)queue updatedTransactions:(NSArray<SKPaymentTransaction *> *)transactions
{
    AppleAppStoreBackend* backend = AppleAppStoreBackend::s_currentInstance;
    if (!backend) {
        qCritical() << "Apple Store transaction callback received but backend instance is null - transactions will be lost";
        return;
    }

    Q_UNUSED(queue);

    for (SKPaymentTransaction * transaction in transactions) {
        AppleAppStoreTransaction * ta = new AppleAppStoreTransaction(transaction, backend);

        switch (static_cast<AppleAppStoreTransaction::AppleAppStoreTransactionState>(transaction.transactionState)) {
        case AppleAppStoreTransaction::Purchasing:
            //unhandled
            break;
        case AppleAppStoreTransaction::Purchased:
            QMetaObject::invokeMethod(backend, "purchaseSucceeded", Qt::AutoConnection, Q_ARG(AbstractTransaction*, ta));
            break;
        case AppleAppStoreTransaction::Failed:
            QMetaObject::invokeMethod(backend, "purchaseFailed", Qt::AutoConnection, Q_ARG(int, transaction.error.code));
            break;
        case AppleAppStoreTransaction::Restored:
            QMetaObject::invokeMethod(backend, "purchaseRestored", Qt::AutoConnection, Q_ARG(AbstractTransaction*, ta));
            break;
        case AppleAppStoreTransaction::Deferred:
            //unhandled
            break;
        }
    }
}

@end

AppleAppStoreBackend::AppleAppStoreBackend(QObject * parent) : AbstractStoreBackend(parent)
{
    this->startConnection();
    s_currentInstance = this;
}

AppleAppStoreBackend::~AppleAppStoreBackend()
{
    if (s_currentInstance == this)
        s_currentInstance = nullptr;

    [_iapManager release];
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
    SKProduct * skProduct = reinterpret_cast<AppleAppStoreProduct*>(product)->nativeProduct();

    SKPayment * payment = [SKPayment paymentWithProduct:skProduct];
    [[SKPaymentQueue defaultQueue] addPayment:payment];
}

void AppleAppStoreBackend::consumePurchase(AbstractTransaction * transaction)
{
    [[SKPaymentQueue defaultQueue] finishTransaction:reinterpret_cast<AppleAppStoreTransaction *>(transaction)->nativeTransaction()];
    emit purchaseConsumed(transaction);
}
