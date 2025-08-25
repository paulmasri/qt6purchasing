#include "appleappstorebackend.h"
#include "appleappstoreproduct.h"
#include "appleappstoretransaction.h"

#include <QDebug>
#include <QJsonObject>
#include <QThread>
#include <QCoreApplication>

#import <StoreKit/StoreKit.h>

// Helper functions for error mapping
static AbstractStoreBackend::PurchaseError mapStoreKitErrorToPurchaseError(int errorCode)
{
    switch (errorCode) {
        case SKErrorPaymentCancelled:
            return AbstractStoreBackend::PurchaseError::UserCanceled;
        case SKErrorPaymentNotAllowed:
            return AbstractStoreBackend::PurchaseError::NotAllowed;
        case SKErrorPaymentInvalid:
            return AbstractStoreBackend::PurchaseError::PaymentInvalid;
        case SKErrorClientInvalid:
        case SKErrorStoreProductNotAvailable:
            return AbstractStoreBackend::PurchaseError::ItemUnavailable;
        case SKErrorCloudServiceNetworkConnectionFailed:
        case SKErrorCloudServiceRevoked:
            return AbstractStoreBackend::PurchaseError::NetworkError;
        case SKErrorUnknown:
        default:
            return AbstractStoreBackend::PurchaseError::UnknownError;
    }
}

static QString getStoreKitErrorMessage(int errorCode)
{
    switch (errorCode) {
        case SKErrorPaymentCancelled:
            return "User canceled the payment request";
        case SKErrorPaymentNotAllowed:
            return "This device is not allowed to make the payment";
        case SKErrorPaymentInvalid:
            return "One of the payment parameters was not recognized by the App Store";
        case SKErrorClientInvalid:
            return "The client is not allowed to issue the request";
        case SKErrorStoreProductNotAvailable:
            return "The requested product is not available in the store";
        case SKErrorCloudServiceNetworkConnectionFailed:
            return "The device could not connect to the network";
        case SKErrorCloudServiceRevoked:
            return "The user has revoked permission to use this cloud service";
        case SKErrorUnknown:
            return "An unknown error occurred";
        default:
            return QString("Unknown StoreKit error code: %1").arg(errorCode);
    }
}

AppleAppStoreBackend* AppleAppStoreBackend::s_currentInstance = nullptr;

// Early observer that queues transactions until full backend is ready
@interface EarlyTransactionObserver : NSObject <SKPaymentTransactionObserver>
{
    NSMutableArray<SKPaymentTransaction *> *queuedTransactions;
}

@property (class, readonly) EarlyTransactionObserver *shared;
-(NSArray<SKPaymentTransaction *> *)getQueuedTransactions;
-(void)clearQueuedTransactions;

@end

@implementation EarlyTransactionObserver

+ (EarlyTransactionObserver *)shared {
    static EarlyTransactionObserver *sharedInstance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedInstance = [[self alloc] init];
    });
    return sharedInstance;
}

-(id)init {
    if (self = [super init]) {
        queuedTransactions = [[NSMutableArray<SKPaymentTransaction *> alloc] init];
    }
    return self;
}

-(NSArray<SKPaymentTransaction *> *)getQueuedTransactions {
    return [queuedTransactions copy];
}

-(void)clearQueuedTransactions {
    [queuedTransactions removeAllObjects];
}

-(void)paymentQueue:(SKPaymentQueue *)queue updatedTransactions:(NSArray<SKPaymentTransaction *> *)transactions {
    qDebug() << "Early observer received" << transactions.count << "transactions - queueing until backend ready";
    [queuedTransactions addObjectsFromArray:transactions];
}

@end

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
        
        // Replace early observer with this one
        [[SKPaymentQueue defaultQueue] removeTransactionObserver:[EarlyTransactionObserver shared]];
        [[SKPaymentQueue defaultQueue] addTransactionObserver:self];
        
        // Process any queued transactions from early observer
        NSArray<SKPaymentTransaction *> *queuedTransactions = [[EarlyTransactionObserver shared] getQueuedTransactions];
        if (queuedTransactions.count > 0) {
            qDebug() << "Processing" << queuedTransactions.count << "queued transactions from early observer";
            [self paymentQueue:[SKPaymentQueue defaultQueue] updatedTransactions:queuedTransactions];
        }
        [[EarlyTransactionObserver shared] clearQueuedTransactions];
    }
    return self;
}

-(void)dealloc
{
    [[SKPaymentQueue defaultQueue] removeTransactionObserver:self];
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
    SKProduct * skProduct = [skProducts count] == 1 ? [skProducts firstObject] : nil;

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

            product->setNativeProduct(skProduct);
            product->setDescription(QString::fromNSString(skProduct.localizedDescription));
            product->setPrice(QString::fromNSString(localizedPrice));
            product->setTitle(QString::fromNSString(skProduct.localizedTitle));
            product->setStatus(AbstractProduct::Registered);

            QMetaObject::invokeMethod(backend, "productRegistered", Qt::AutoConnection, Q_ARG(AbstractProduct*, product));
        } else {
        }
    }
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
            {
                // Extract product ID from the transaction
                QString productId = QString::fromNSString(transaction.payment.productIdentifier);
                int errorCode = transaction.error.code;
                AbstractStoreBackend::PurchaseError error = mapStoreKitErrorToPurchaseError(errorCode);
                QString message = getStoreKitErrorMessage(errorCode);
                QMetaObject::invokeMethod(backend, "purchaseFailed", Qt::AutoConnection, 
                    Q_ARG(QString, productId),
                    Q_ARG(int, static_cast<int>(error)), 
                    Q_ARG(int, errorCode), 
                    Q_ARG(QString, message));
            }
            break;
        case AppleAppStoreTransaction::Restored:
            QMetaObject::invokeMethod(backend, "purchaseRestored", Qt::AutoConnection, Q_ARG(AbstractTransaction*, ta));
            break;
        case AppleAppStoreTransaction::Deferred:
            QMetaObject::invokeMethod(backend, "purchasePending", Qt::AutoConnection, Q_ARG(AbstractTransaction*, ta));
            break;
        }
    }
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

void AppleAppStoreBackend::initializeEarly()
{
    qDebug() << "iOS IAP: Adding early transaction observer to catch transactions at app startup";
    [[SKPaymentQueue defaultQueue] addTransactionObserver:[EarlyTransactionObserver shared]];
}

void AppleAppStoreBackend::startConnection()
{
    _iapManager = [[InAppPurchaseManager alloc] init];
    setConnected(_iapManager != nullptr);
    setCanMakePurchases(canMakePurchases());
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
    emit consumePurchaseSucceeded(transaction);
}

void AppleAppStoreBackend::restorePurchases()
{
    qDebug() << "iOS restorePurchases() called - triggering SKPaymentQueue.restoreCompletedTransactions";
    [[SKPaymentQueue defaultQueue] restoreCompletedTransactions];
}

bool AppleAppStoreBackend::canMakePurchases() const
{
    return [SKPaymentQueue canMakePayments];
}
