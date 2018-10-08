// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/paicoin-config.h"
#endif

#include "paicoingui.h"

#include "chainparams.h"
#include "clientmodel.h"
#include "fs.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "intro.h"
#include "networkstyle.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "splashscreen.h"
#include "utilitydialog.h"
#include "winshutdownmonitor.h"

#ifdef ENABLE_WALLET
#include "paymentserver.h"
#include "walletmodel.h"
#include "walletselectionpage.h"
#include "welcomepage.h"
#include "setpinpage.h"
#include "paperkeyintropage.h"
#include "paperkeywritedownpage.h"
#include "paperkeycompletionpage.h"
#include "restorewalletpage.h"
#include "authmanager.h"
#endif

#include "init.h"
#include "rpc/server.h"
#include "scheduler.h"
#include "ui_interface.h"
#include "util.h"
#include "warnings.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <stdint.h>

#include <boost/thread.hpp>

#include <QApplication>
#include <QDebug>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QSettings>
#include <QThread>
#include <QTimer>
#include <QTranslator>
#include <QSslConfiguration>
#include <QMessageBox>

#if defined(QT_STATICPLUGIN)
#include <QtPlugin>
#if QT_VERSION < 0x050000
Q_IMPORT_PLUGIN(qcncodecs)
Q_IMPORT_PLUGIN(qjpcodecs)
Q_IMPORT_PLUGIN(qtwcodecs)
Q_IMPORT_PLUGIN(qkrcodecs)
Q_IMPORT_PLUGIN(qtaccessiblewidgets)
#else
#if QT_VERSION < 0x050400
Q_IMPORT_PLUGIN(AccessibleFactory)
#endif
#if defined(QT_QPA_PLATFORM_XCB)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_WINDOWS)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_COCOA)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin);
#endif
#endif
#endif

#if QT_VERSION < 0x050000
#include <QTextCodec>
#endif

#define	EXIT_FIRST_RUN	2	/* Exited during first run.  */

// Declare meta types used for QMetaObject::invokeMethod
Q_DECLARE_METATYPE(bool*)
Q_DECLARE_METATYPE(CAmount)

static void InitMessage(const std::string &message)
{
    LogPrintf("init message: %s\n", message);
}

/*
   Translate string to current locale using Qt.
 */
static std::string Translate(const char* psz)
{
    return QCoreApplication::translate("paicoin-core", psz).toStdString();
}

static QString GetLangTerritory()
{
    QSettings settings;
    // Get desired locale (e.g. "de_DE")
    // 1) System default language
    QString lang_territory = QLocale::system().name();
    // 2) Language from QSettings
    QString lang_territory_qsettings = settings.value("language", "").toString();
    if(!lang_territory_qsettings.isEmpty())
        lang_territory = lang_territory_qsettings;
    // 3) -lang command line argument
    lang_territory = QString::fromStdString(gArgs.GetArg("-lang", lang_territory.toStdString()));
    return lang_territory;
}

/** Set up translations */
static void initTranslations(QTranslator &qtTranslatorBase, QTranslator &qtTranslator, QTranslator &translatorBase, QTranslator &translator)
{
    // Remove old translators
    QApplication::removeTranslator(&qtTranslatorBase);
    QApplication::removeTranslator(&qtTranslator);
    QApplication::removeTranslator(&translatorBase);
    QApplication::removeTranslator(&translator);

    // Get desired locale (e.g. "de_DE")
    // 1) System default language
    QString lang_territory = GetLangTerritory();

    // Convert to "de" only by truncating "_DE"
    QString lang = lang_territory;
    lang.truncate(lang_territory.lastIndexOf('_'));

    // Load language files for configured locale:
    // - First load the translator for the base language, without territory
    // - Then load the more specific locale translator

    // Load e.g. qt_de.qm
    if (qtTranslatorBase.load("qt_" + lang, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qtTranslatorBase);

    // Load e.g. qt_de_DE.qm
    if (qtTranslator.load("qt_" + lang_territory, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qtTranslator);

    // Load e.g. paicoin_de.qm (shortcut "de" needs to be defined in paicoin.qrc)
    if (translatorBase.load(lang, ":/translations/"))
        QApplication::installTranslator(&translatorBase);

    // Load e.g. paicoin_de_DE.qm (shortcut "de_DE" needs to be defined in paicoin.qrc)
    if (translator.load(lang_territory, ":/translations/"))
        QApplication::installTranslator(&translator);
}

/* qDebug() message handler --> debug.log */
#if QT_VERSION < 0x050000
void DebugMessageHandler(QtMsgType type, const char *msg)
{
    if (type == QtDebugMsg) {
        LogPrint(BCLog::QT, "GUI: %s\n", msg);
    } else {
        LogPrintf("GUI: %s\n", msg);
    }
}
#else
void DebugMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString &msg)
{
    Q_UNUSED(context);
    if (type == QtDebugMsg) {
        LogPrint(BCLog::QT, "GUI: %s\n", msg.toStdString());
    } else {
        LogPrintf("GUI: %s\n", msg.toStdString());
    }
}
#endif

/** Class encapsulating PAI Coin Core startup and shutdown.
 * Allows running startup and shutdown in a different thread from the UI thread.
 */
class PAIcoinCore: public QObject
{
    Q_OBJECT
public:
    explicit PAIcoinCore();
    /** Basic initialization, before starting initialization/shutdown thread.
     * Return true on success.
     */
    static bool baseInitialize();

public Q_SLOTS:
    void initialize();
    void shutdown();
    void initFinalize();

Q_SIGNALS:
    void initializeResult(bool success);
    void initializeFirstRun();
    void shutdownResult();
    void runawayException(const QString &message);

private:
    boost::thread_group threadGroup;
    CScheduler scheduler;

    /// Pass fatal exception message to UI thread
    void handleRunawayException(const std::exception *e);
};

/** Main PAI Coin application object */
class PAIcoinApplication: public QApplication
{
    Q_OBJECT
public:
    explicit PAIcoinApplication(int &argc, char **argv);
    ~PAIcoinApplication();

#ifdef ENABLE_WALLET
    /// Create payment server
    void createPaymentServer();
#endif
    /// parameter interaction/setup based on rules
    void parameterSetup();
    /// Create options model
    void createOptionsModel(bool resetSettings);
    /// Create main window
    void createWindow(bool firstRun);
    /// Create splash screen
    void createSplashScreen();

    /// Request core initialization
    void requestInitialize();
    /// Request core shutdown
    void requestShutdown();

    /// Get process return value
    int getReturnValue() const { return returnValue; }

    /// Get window identifier of QMainWindow (PAIcoinGUI)
    WId getMainWinId() const;

    /// Get runtime application name
    const QString& getName() const;

public Q_SLOTS:
    void initializeResult(bool success);
    void initializeFirstRun();
    void createNewWallet();
    void restoreWallet(std::string paperKey);
    void completeNewWalletInitialization();
    void enableWalletDisplay();
    void shutdownResult();
    /// Handle runaway exceptions. Shows a message box with the problem and quits the program.
    void handleRunawayException(const QString &message);
    void message(const QString &title, const QString &message, unsigned int style, bool *ret);

Q_SIGNALS:
    void requestedInitialize();
    void requestedShutdown();
    void requestedInitFinalize();
    void stopThread();
    void splashFinished(QWidget *window);
    void walletCreated(std::string phrase);
    void walletRestored(std::string phrase);
    void completeUiWalletInitialization();

private:
    QThread *coreThread;
    OptionsModel *optionsModel;
    ClientModel *clientModel;
    PAIcoinGUI *window;
    QTimer *pollShutdownTimer;
#ifdef ENABLE_WALLET
    PaymentServer* paymentServer;
    WalletModel *walletModel;
    const NetworkStyle *networkStyle;
    std::string walletPhrase;
#endif
    int returnValue;
    const PlatformStyle *platformStyle;
    std::unique_ptr<QWidget> shutdownWindow;

    void startThread();
};

#include "paicoin.moc"

PAIcoinCore::PAIcoinCore():
    QObject()
{
}

void PAIcoinCore::handleRunawayException(const std::exception *e)
{
    PrintExceptionContinue(e, "Runaway exception");
    Q_EMIT runawayException(QString::fromStdString(GetWarnings("gui")));
}

bool PAIcoinCore::baseInitialize()
{
    if (!AppInitBasicSetup())
    {
        return false;
    }
    if (!AppInitParameterInteraction())
    {
        return false;
    }
    if (!AppInitSanityChecks())
    {
        return false;
    }
    if (!AppInitLockDataDirectory())
    {
        return false;
    }
    return true;
}

void PAIcoinCore::initialize()
{
    try
    {
        qDebug() << __func__ << ": Running initialization in thread";
        bool firstRun = false;
        bool rv = AppInitMain(threadGroup, scheduler, firstRun);
        if (!rv || !firstRun) {
            Q_EMIT initializeResult(rv);
        } else {
            Q_EMIT initializeFirstRun();
        }
    } catch (const std::exception& e) {
        handleRunawayException(&e);
    } catch (...) {
        handleRunawayException(nullptr);
    }
}

void PAIcoinCore::shutdown()
{
    try
    {
        qDebug() << __func__ << ": Running Shutdown in thread";
        Interrupt(threadGroup);
        threadGroup.join_all();
        Shutdown();
        qDebug() << __func__ << ": Shutdown finished";
        Q_EMIT shutdownResult();
    } catch (const std::exception& e) {
        handleRunawayException(&e);
    } catch (...) {
        handleRunawayException(nullptr);
    }
}

void PAIcoinCore::initFinalize()
{
    bool hasResult = false;
    bool result = AppInitMainFinalize(threadGroup, scheduler, hasResult);
    if (hasResult && !result)
    {
        shutdown();
    }
}

PAIcoinApplication::PAIcoinApplication(int &argc, char **argv):
    QApplication(argc, argv),
    coreThread(nullptr),
    optionsModel(nullptr),
    clientModel(nullptr),
    window(nullptr),
    pollShutdownTimer(nullptr),
#ifdef ENABLE_WALLET
    paymentServer(nullptr),
    walletModel(nullptr),
    networkStyle(nullptr),
#endif
    returnValue(EXIT_SUCCESS)
{
    setQuitOnLastWindowClosed(false);

    // UI per-platform customization
    // This must be done inside the PAIcoinApplication constructor, or after it, because
    // PlatformStyle::instantiate requires a QApplication
    std::string platformName;
    platformName = gArgs.GetArg("-uiplatform", PAIcoinGUI::DEFAULT_UIPLATFORM);
    platformStyle = PlatformStyle::instantiate(QString::fromStdString(platformName));
    if (!platformStyle) // Fall back to "other" if specified name not found
        platformStyle = PlatformStyle::instantiate("other");
    assert(platformStyle);
    networkStyle = NetworkStyle::instantiate(QString::fromStdString(Params().NetworkIDString()));
    assert(networkStyle);
}

PAIcoinApplication::~PAIcoinApplication()
{
    if(coreThread)
    {
        qDebug() << __func__ << ": Stopping thread";
        Q_EMIT stopThread();
        coreThread->wait();
        qDebug() << __func__ << ": Stopped thread";
    }

    delete window;
    window = 0;
#ifdef ENABLE_WALLET
    delete paymentServer;
    paymentServer = 0;
    delete networkStyle;
    networkStyle = nullptr;
#endif
    delete optionsModel;
    optionsModel = 0;
    delete platformStyle;
    platformStyle = 0;
}

#ifdef ENABLE_WALLET
void PAIcoinApplication::createPaymentServer()
{
    paymentServer = new PaymentServer(this);
}
#endif

void PAIcoinApplication::createOptionsModel(bool resetSettings)
{
    optionsModel = new OptionsModel(nullptr, resetSettings);
}

void PAIcoinApplication::createWindow(bool firstRun)
{
    window = new PAIcoinGUI(platformStyle, networkStyle, firstRun, 0);

    pollShutdownTimer = new QTimer(window);
    connect(pollShutdownTimer, SIGNAL(timeout()), window, SLOT(detectShutdown()));
    connect(window, &PAIcoinGUI::firstRunComplete, [=] (void) { returnValue = EXIT_SUCCESS; });
#ifdef ENABLE_WALLET
    if (firstRun)
    {
        connect(this, SIGNAL(walletCreated(std::string)), window, SLOT(walletCreated(std::string)));
        connect(this, SIGNAL(walletRestored(std::string)), window, SLOT(walletRestored(std::string)));
        connect(this, SIGNAL(completeUiWalletInitialization()), window, SLOT(completeUiWalletInitialization()));
        connect(window, SIGNAL(createNewWalletRequest()), this, SLOT(createNewWallet()));
        connect(window, SIGNAL(restoreWalletRequest(std::string)), this, SLOT(restoreWallet(std::string)));
        connect(window, SIGNAL(linkWalletToMainApp()), this, SLOT(completeNewWalletInitialization()));
        connect(window, SIGNAL(enableWalletDisplay()), this, SLOT(enableWalletDisplay()));
    }
    connect(window, SIGNAL(shutdown()), this, SLOT(shutdownResult()));
#endif // ENABLE_WALLET
    pollShutdownTimer->start(200);

    // core signals should now be routed to the PAICoinGUI
    GUIUtil::unsubscribeFromCoreSignals(this);
}

void PAIcoinApplication::createSplashScreen()
{
    SplashScreen *splash = new SplashScreen(0, networkStyle);
    // We don't hold a direct pointer to the splash screen after creation, but the splash
    // screen will take care of deleting itself when slotFinish happens.
    splash->show();
    connect(this, SIGNAL(splashFinished(QWidget*)), splash, SLOT(slotFinish(QWidget*)));
    connect(this, SIGNAL(requestedShutdown()), splash, SLOT(close()));
}

void PAIcoinApplication::startThread()
{
    if(coreThread)
        return;
    coreThread = new QThread(this);
    PAIcoinCore *executor = new PAIcoinCore();
    executor->moveToThread(coreThread);

    /*  communication to and from thread */
    connect(executor, SIGNAL(initializeResult(bool)), this, SLOT(initializeResult(bool)));
    connect(executor, SIGNAL(initializeFirstRun()), this, SLOT(initializeFirstRun()));
    connect(executor, SIGNAL(shutdownResult()), this, SLOT(shutdownResult()));
    connect(executor, SIGNAL(runawayException(QString)), this, SLOT(handleRunawayException(QString)));
    connect(this, SIGNAL(requestedInitialize()), executor, SLOT(initialize()));
    connect(this, SIGNAL(requestedShutdown()), executor, SLOT(shutdown()));
    connect(this, SIGNAL(requestedInitFinalize()), executor, SLOT(initFinalize()));
    /*  make sure executor object is deleted in its own thread */
    connect(this, SIGNAL(stopThread()), executor, SLOT(deleteLater()));
    connect(this, SIGNAL(stopThread()), coreThread, SLOT(quit()));

    coreThread->start();
}

void PAIcoinApplication::parameterSetup()
{
    InitLogging();
    InitParameterInteraction();
}

void PAIcoinApplication::requestInitialize()
{
    qDebug() << __func__ << ": Requesting initialize";
    startThread();
    Q_EMIT requestedInitialize();
}

void PAIcoinApplication::requestShutdown()
{
    // Show a simple window indicating shutdown status
    // Do this first as some of the steps may take some time below,
    // for example the RPC console may still be executing a command.
    shutdownWindow.reset(ShutdownWindow::showShutdownWindow(window));

    qDebug() << __func__ << ": Requesting shutdown";
    startThread();
    if (window != nullptr)
    {
        window->hide();
        window->setClientModel(nullptr);
    }
    if (pollShutdownTimer != nullptr)
        pollShutdownTimer->stop();

#ifdef ENABLE_WALLET
    if (window != nullptr)
        window->removeAllWallets();
    delete walletModel;
    walletModel = nullptr;
#endif
    delete clientModel;
    clientModel = nullptr;

    StartShutdown();

    // Request shutdown from core thread
    Q_EMIT requestedShutdown();
}

void PAIcoinApplication::initializeResult(bool success)
{
    qDebug() << __func__ << ": Initialization result: " << success;
    // Set exit result.
    returnValue = success ? EXIT_SUCCESS : EXIT_FAILURE;
    if(success)
    {
        createWindow(false);

        // Log this only after AppInitMain finishes, as then logging setup is guaranteed complete
        qWarning() << "Platform customization:" << platformStyle->getName();
#ifdef ENABLE_WALLET
        PaymentServer::LoadRootCAs();
        paymentServer->setOptionsModel(optionsModel);
#endif

        clientModel = new ClientModel(optionsModel);
        window->setClientModel(clientModel);

#ifdef ENABLE_WALLET
        // TODO: Expose secondary wallets
        if (!vpwallets.empty())
        {
            walletModel = new WalletModel(platformStyle, vpwallets[0], optionsModel);
            walletModel->connectAuthenticator();

            window->addWallet(PAIcoinGUI::DEFAULT_WALLET, walletModel);
            window->setCurrentWallet(PAIcoinGUI::DEFAULT_WALLET);

            connect(walletModel, SIGNAL(coinsSent(CWallet*,SendCoinsRecipient,QByteArray)),
                    paymentServer, SLOT(fetchPaymentACK(CWallet*,const SendCoinsRecipient&,QByteArray)));
        }
#endif

        // If -min option passed, start window minimized.
        if(gArgs.GetBoolArg("-min", false))
        {
            window->showMinimized();
        }
        else
        {
            window->show();
        }
        Q_EMIT splashFinished(window);

#ifdef ENABLE_WALLET
        // Now that initialization/startup is done, process any command-line
        // paicoin:// URIs or payment requests:
        connect(paymentServer, SIGNAL(receivedPaymentRequest(SendCoinsRecipient)),
                         window, SLOT(handlePaymentRequest(SendCoinsRecipient)));
        connect(window, SIGNAL(receivedURI(QString)),
                         paymentServer, SLOT(handleURIOrFile(QString)));
        connect(paymentServer, SIGNAL(message(QString,QString,unsigned int)),
                         window, SLOT(message(QString,QString,unsigned int)));
        QTimer::singleShot(100, paymentServer, SLOT(uiReady()));

        if (walletModel->isWalletEnabled()) {
            if (walletModel->getEncryptionStatus() == WalletModel::EncryptionStatus::Locked)
            {
                WalletModel::UnlockContext ctx(walletModel->requestUnlock());
                if (ctx.isValid())
                {
                    walletModel->decryptPaperKey();
                    walletModel->decryptPinCode();

                    walletModel->refreshInvestorKey();

                    window->interruptForPinRequest(AuthManager::getInstance().ShouldSet());
                }
            } else {
                window->interruptForPinRequest(AuthManager::getInstance().ShouldSet());
            }
        }
#endif
    } else {
        quit(); // Exit main loop
    }
}

void PAIcoinApplication::initializeFirstRun()
{
    qDebug() << __func__ << ": First run.";
    returnValue = EXIT_FIRST_RUN;

    // Log this only after AppInitMain finishes, as then logging setup is guaranteed complete
    qWarning() << "Platform customization:" << platformStyle->getName();

    createWindow(true);

    window->show();

    Q_EMIT splashFinished(window);
}

void PAIcoinApplication::createNewWallet()
{
    PaymentServer::LoadRootCAs();
    paymentServer->setOptionsModel(optionsModel);

    clientModel = new ClientModel(optionsModel);
    window->setClientModel(clientModel);

    if (!vpwallets.empty())
    {
        SecureString secPaperKey;
        secPaperKey = vpwallets[0]->GeneratePaperKey();
        std::string paperKey(secPaperKey.begin(), secPaperKey.end());
        walletPhrase = paperKey;

        Q_EMIT walletCreated(paperKey);
    }
}

void PAIcoinApplication::restoreWallet(std::string paperKey)
{
    // Restore wallet based on provided BIP39 phrase
    PaymentServer::LoadRootCAs();
    paymentServer->setOptionsModel(optionsModel);

    clientModel = new ClientModel(optionsModel);
    window->setClientModel(clientModel);

    if (!vpwallets.empty())
    {
        walletPhrase = paperKey;

        Q_EMIT walletRestored(paperKey);
    }
}

void PAIcoinApplication::completeNewWalletInitialization()
{
    // Now that initialization/startup is done, process any command-line
    // paicoin:// URIs or payment requests:
    connect(paymentServer, SIGNAL(receivedPaymentRequest(SendCoinsRecipient)),
            window, SLOT(handlePaymentRequest(SendCoinsRecipient)));
    connect(window, SIGNAL(receivedURI(QString)),
            paymentServer, SLOT(handleURIOrFile(QString)));
    connect(paymentServer, SIGNAL(message(QString,QString,unsigned int)),
            window, SLOT(message(QString,QString,unsigned int)));
    QTimer::singleShot(100, paymentServer, SLOT(uiReady()));

    window->createWalletFrame();

    enableWalletDisplay();

    Q_EMIT requestedInitFinalize();
    Q_EMIT completeUiWalletInitialization();
}

void PAIcoinApplication::enableWalletDisplay()
{
    walletModel = new WalletModel(platformStyle, vpwallets[0], optionsModel);
    bool bSuccess = walletModel->usePaperKey(SecureString(walletPhrase.begin(), walletPhrase.end()));
    assert(bSuccess || !"unable to use the provided paper key!");

    walletModel->connectAuthenticator();

    window->addWallet(PAIcoinGUI::DEFAULT_WALLET, walletModel);
    window->setCurrentWallet(PAIcoinGUI::DEFAULT_WALLET);

    connect(walletModel, SIGNAL(coinsSent(CWallet*,SendCoinsRecipient,QByteArray)),
            paymentServer, SLOT(fetchPaymentACK(CWallet*,const SendCoinsRecipient&,QByteArray)));
}

void PAIcoinApplication::shutdownResult()
{
    quit(); // Exit main loop after shutdown finished
}

void PAIcoinApplication::handleRunawayException(const QString &message)
{
    QMessageBox::critical(0, "Runaway exception", PAIcoinGUI::tr("A fatal error occurred. PAI Coin can no longer continue safely and will quit.") + QString("\n\n") + message);
    ::exit(EXIT_FAILURE);
}

WId PAIcoinApplication::getMainWinId() const
{
    if (!window)
        return 0;

    return window->winId();
}

const QString& PAIcoinApplication::getName() const
{
    return networkStyle->getAppName();
}

void PAIcoinApplication::message(const QString &title, const QString &message, unsigned int style, bool *ret)
{
    QString            strTitle;
    QMessageBox::Icon  nMBoxIcon;
    Notificator::Class nNotifyIcon;

    std::tie(strTitle,nMBoxIcon, nNotifyIcon) = GUIUtil::getMessageProperties(title,style);
    // Display message
    if (style & CClientUIInterface::MODAL) {
        GUIUtil::showMessageBox(nMBoxIcon,strTitle,message,nullptr,style,ret);
    }
}

#ifndef PAICOIN_QT_TEST
int main(int argc, char *argv[])
{

    SetupEnvironment();

    /// 1. Parse command-line options. These take precedence over anything else.
    // Command-line options take precedence:
    gArgs.ParseParameters(argc, argv);

    // Do not refer to data directory yet, this can be overridden by Intro::pickDataDirectory

    /// 2. Basic Qt initialization (not dependent on parameters or configuration)
#if QT_VERSION < 0x050000
    // Internal string conversion is all UTF-8
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
    QTextCodec::setCodecForCStrings(QTextCodec::codecForTr());
#endif

    Q_INIT_RESOURCE(paicoin);
    Q_INIT_RESOURCE(paicoin_locale);

#if QT_VERSION > 0x050100
    // Generate high-dpi pixmaps
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
#if QT_VERSION >= 0x050600
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
#ifdef Q_OS_MAC
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif
#if QT_VERSION >= 0x050500
    // Because of the POODLE attack it is recommended to disable SSLv3 (https://disablessl3.com/),
    // so set SSL protocols to TLS1.0+.
    QSslConfiguration sslconf = QSslConfiguration::defaultConfiguration();
    sslconf.setProtocol(QSsl::TlsV1_0OrLater);
    QSslConfiguration::setDefaultConfiguration(sslconf);
#endif

    // Register meta types used for QMetaObject::invokeMethod
    qRegisterMetaType< bool* >();
    //   Need to pass name here as CAmount is a typedef (see http://qt-project.org/doc/qt-5/qmetatype.html#qRegisterMetaType)
    //   IMPORTANT if it is no longer a typedef use the normal variant above
    qRegisterMetaType< CAmount >("CAmount");
    qRegisterMetaType< std::function<void(void)> >("std::function<void(void)>");

    /// 3. Application identification
    // must be set before OptionsModel is initialized or translations are loaded,
    // as it is used to locate QSettings
    QApplication::setOrganizationName(QAPP_ORG_NAME);
    QApplication::setOrganizationDomain(QAPP_ORG_DOMAIN);
    QApplication::setApplicationName(QAPP_APP_NAME_DEFAULT);
    GUIUtil::SubstituteFonts(GetLangTerritory());

    /// 4. Initialization of translations, so that intro dialog is in user's language
    // Now that QSettings are accessible, initialize translations
    QTranslator qtTranslatorBase, qtTranslator, translatorBase, translator;
    initTranslations(qtTranslatorBase, qtTranslator, translatorBase, translator);
    translationInterface.Translate.connect(Translate);

    /// 5. Determine network (and switch to network specific options)
    // - Do not call Params() before this step
    // - Do this after parsing the configuration file, as the network can be switched there
    // - QSettings() will use the new application name after this, resulting in network-specific settings
    // - Needs to be done before createOptionsModel

    // Check for -testnet or -regtest parameter (Params() calls are only valid after this clause)
    try {
        SelectParams(ChainNameFromCommandLine());
    } catch(std::exception &e) {
        QMessageBox::critical(0, QObject::tr(PACKAGE_NAME), QObject::tr("Error: %1").arg(e.what()));
        return EXIT_FAILURE;
    }
#ifdef ENABLE_WALLET
    // Parse URIs on command line -- this can affect Params()
    PaymentServer::ipcParseCommandLine(argc, argv);
#endif

    PAIcoinApplication app(argc, argv);
    // Allow for separate UI settings for testnets
    QApplication::setApplicationName(app.getName());
    // Re-initialize translations after changing application name (language in network-specific settings can be different)
    initTranslations(qtTranslatorBase, qtTranslator, translatorBase, translator);

    // Show help message immediately after parsing command-line options (for "-lang") and setting locale,
    // but before showing splash screen.
    if (gArgs.IsArgSet("-?") || gArgs.IsArgSet("-h") || gArgs.IsArgSet("-help") || gArgs.IsArgSet("-version"))
    {
        HelpMessageDialog help(nullptr, gArgs.IsArgSet("-version"));
        help.showOrPrint();
        return EXIT_SUCCESS;
    }

#ifdef ENABLE_WALLET
    /// 6. URI IPC sending
    // - Do this early as we don't want to bother initializing if we are just calling IPC
    // - Do this *after* setting up the data directory, as the data directory hash is used in the name
    // of the server.
    // - Do this after creating app and setting up translations, so errors are
    // translated properly.
    if (PaymentServer::ipcSendCommandLine())
        exit(EXIT_SUCCESS);

    // Start up the payment server early, too, so impatient users that click on
    // paicoin:// links repeatedly have their payment requests routed to this process:
    app.createPaymentServer();
#endif

    /// 7. Main GUI initialization
    // Install global event filter that makes sure that long tooltips can be word-wrapped
    app.installEventFilter(new GUIUtil::ToolTipToRichTextFilter(TOOLTIP_WRAP_THRESHOLD, &app));
#if QT_VERSION < 0x050000
    // Install qDebug() message handler to route to debug.log
    qInstallMsgHandler(DebugMessageHandler);
#else
#if defined(Q_OS_WIN)
    // Install global event filter for processing Windows session related Windows messages (WM_QUERYENDSESSION and WM_ENDSESSION)
    qApp->installNativeEventFilter(new WinShutdownMonitor());
#endif
    // Install qDebug() message handler to route to debug.log
    qInstallMessageHandler(DebugMessageHandler);
#endif
    // Allow parameter interaction before we create the options model
    app.parameterSetup();
    // Load GUI settings from QSettings
    app.createOptionsModel(gArgs.IsArgSet("-resetguisettings"));

    // Subscribe to global signals from core
    uiInterface.InitMessage.connect(InitMessage);

    if (gArgs.GetBoolArg("-splash", DEFAULT_SPLASHSCREEN) && !gArgs.GetBoolArg("-min", false))
        app.createSplashScreen();

    int rv = EXIT_SUCCESS;
    try
    {
        GUIUtil::subscribeToCoreSignals(&app);
        QSettings settings;
        /* 1) Default data directory for operating system */
        QString dataDir = Intro::getDefaultDataDirectory();
        /* 2) Allow QSettings to override default dir */
        dataDir = settings.value("strDataDir", dataDir).toString();
        // Perform base initialization before spinning up initialization/shutdown thread
        // This is acceptable because this function only contains steps that are quick to execute,
        // so the GUI thread won't be held up.
        if (PAIcoinCore::baseInitialize()) {
            app.requestInitialize();
#if defined(Q_OS_WIN) && QT_VERSION >= 0x050000
            WinShutdownMonitor::registerShutdownBlockReason(QObject::tr("%1 didn't yet exit safely...").arg(QObject::tr(PACKAGE_NAME)), (HWND)app.getMainWinId());
#endif
            app.exec();
            app.requestShutdown();
            app.exec();
            rv = app.getReturnValue();
            if (rv == EXIT_FIRST_RUN) {
                RemoveDataDirectory();
            }
        } else {
            // A dialog with detailed error will have been shown by InitError()
            rv = EXIT_FAILURE;
        }
    } catch (const std::exception& e) {
        PrintExceptionContinue(&e, "Runaway exception");
        app.handleRunawayException(QString::fromStdString(GetWarnings("gui")));
    } catch (...) {
        PrintExceptionContinue(nullptr, "Runaway exception");
        app.handleRunawayException(QString::fromStdString(GetWarnings("gui")));
    }
    return rv;
}
#endif // PAICOIN_QT_TEST
