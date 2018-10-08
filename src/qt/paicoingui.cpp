// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/paicoin-config.h"
#endif

#include "paicoingui.h"

#include "paicoinunits.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "modaloverlay.h"
#include "networkstyle.h"
#include "notificator.h"
#include "openuridialog.h"
#include "optionsdialog.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "rpcconsole.h"
#include "utilitydialog.h"

#ifdef ENABLE_WALLET
#include "walletframe.h"
#include "walletmodel.h"
#include "walletselectionpage.h"
#include "restorewalletpage.h"
#include "paperkeyintropage.h"
#include "paperkeywritedownpage.h"
#include "paperkeycompletionpage.h"
#include "fundsinholdingdialog.h"
#include "confirmationdialog.h"
#include "setpinpage.h"
#include "authmanager.h"
#include "settingshelper.h"
#endif // ENABLE_WALLET

#include "welcomepage.h"
#include "intro.h"

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include "chainparams.h"
#include "init.h"
#include "ui_interface.h"
#include "util.h"

#include <iostream>
#include <memory>

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDesktopWidget>
#include <QDragEnterEvent>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressDialog>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QString>

#if QT_VERSION < 0x050000
#include <QTextDocument>
#include <QUrl>
#else
#include <QUrlQuery>
#endif

const std::string PAIcoinGUI::DEFAULT_UIPLATFORM =
#if defined(Q_OS_MAC)
        "macosx"
#elif defined(Q_OS_WIN)
        "windows"
#else
        "other"
#endif
        ;

/** Display name for default wallet name. Uses tilde to avoid name
 * collisions in the future with additional wallets */
const QString PAIcoinGUI::DEFAULT_WALLET = "~Default";

PAIcoinGUI::PAIcoinGUI(const PlatformStyle *_platformStyle, const NetworkStyle *_networkStyle, bool _firstRun, QWidget *parent) :
    QMainWindow(parent),
    enableWallet(false),
    clientModel(nullptr),
    walletFrame(nullptr),
    mainStackedWidget(nullptr),
    unitDisplayControl(nullptr),
    labelWalletEncryptionIcon(nullptr),
    labelWalletHDStatusIcon(nullptr),
    connectionsControl(nullptr),
    labelBlocksIcon(nullptr),
    progressBarLabel(nullptr),
    progressBar(nullptr),
    progressDialog(nullptr),
    tabGroup(nullptr),
    appMenuBar(nullptr),
    toolbar(nullptr),
    fileMenu(nullptr),
    settingsMenu(nullptr),
    helpMenu(nullptr),
    overviewAction(nullptr),
    historyAction(nullptr),
    quitAction(nullptr),
    sendCoinsAction(nullptr),
    sendCoinsMenuAction(nullptr),
    usedSendingAddressesAction(nullptr),
    usedReceivingAddressesAction(nullptr),
    signMessageAction(nullptr),
    verifyMessageAction(nullptr),
    aboutAction(nullptr),
    receiveCoinsAction(nullptr),
    receiveCoinsMenuAction(nullptr),
    optionsAction(nullptr),
    toggleHideAction(nullptr),
    #ifdef ENABLE_ENCRYPT_WALLET
    encryptWalletAction(nullptr),
    changePassphraseAction(nullptr),
    #endif
    backupWalletAction(nullptr),
    aboutQtAction(nullptr),
    openRPCConsoleAction(nullptr),
    openAction(nullptr),
    showHelpMessageAction(nullptr),
    viewInvestorKeyAction(nullptr),
    reviewPaperKeyAction(nullptr),
    trayIcon(nullptr),
    trayIconMenu(nullptr),
    notificator(nullptr),
    rpcConsole(nullptr),
    helpMessageDialog(nullptr),
    modalOverlay(nullptr),
    prevBlocks(0),
    spinnerFrame(0),
    firstRun(_firstRun),
    state(PAIcoinGUIState::Init),
    platformStyle(_platformStyle),
    networkStyle(_networkStyle),
    welcomePage(nullptr),
    walletSelectionPage(nullptr),
    restoreWalletPage(nullptr),
    paperKeyIntroPage(nullptr),
    setPinPage(nullptr),
    paperKeyWritedownPage(nullptr),
    paperKeyCompletionPage(nullptr)
{
    GUIUtil::restoreWindowGeometry("nWindow", QSize(850, 550), this);

    QString windowTitle = tr(PACKAGE_NAME);
#ifdef ENABLE_WALLET
    enableWallet = WalletModel::isWalletEnabled();
#endif // ENABLE_WALLET
    if(!enableWallet)
    {
        windowTitle += " - " + tr("Node");
        windowTitle += " " + networkStyle->getTitleAddText();
    }
#ifndef Q_OS_MAC
    QApplication::setWindowIcon(networkStyle->getTrayAndWindowIcon());
    setWindowIcon(networkStyle->getTrayAndWindowIcon());
#else
    MacDockIconHandler::instance()->setIcon(networkStyle->getAppIcon());
#endif
    setWindowTitle(windowTitle);

#if defined(Q_OS_MAC) && QT_VERSION < 0x050000
    // This property is not implemented in Qt 5. Setting it has no effect.
    // A replacement API (QtMacUnifiedToolBar) is available in QtMacExtras.
    setUnifiedTitleAndToolBarOnMac(true);
#endif

    rpcConsole = new RPCConsole(_platformStyle, 0);
    helpMessageDialog = new HelpMessageDialog(this, false);
#ifdef ENABLE_WALLET
    if (enableWallet)
    {
        setPinPage = new SetPinPage; // Used no matter if first run or not
        mainStackedWidget = new QStackedWidget;
        mainStackedWidget->addWidget(setPinPage);
        if (firstRun)
        {
            welcomePage = new WelcomePage;
            walletSelectionPage = new WalletSelectionPage;
            restoreWalletPage = new RestoreWalletPage;
            paperKeyIntroPage = new PaperKeyIntroPage;
            paperKeyWritedownPage = new PaperKeyWritedownPage;
            paperKeyCompletionPage = new PaperKeyCompletionPage;

            mainStackedWidget->addWidget(welcomePage);
            mainStackedWidget->addWidget(walletSelectionPage);
            mainStackedWidget->addWidget(restoreWalletPage);
            mainStackedWidget->addWidget(paperKeyIntroPage);
            mainStackedWidget->addWidget(paperKeyWritedownPage);
            mainStackedWidget->addWidget(paperKeyCompletionPage);
            mainStackedWidget->setCurrentWidget(welcomePage);
        }
        else
        {
            /** Create wallet frame and make it the central widget */
            walletFrame = new WalletFrame(_platformStyle, this);
            mainStackedWidget->addWidget(walletFrame);
        }
        setCentralWidget(mainStackedWidget);
    } else
#endif // ENABLE_WALLET
    {
        /* When compiled without wallet v -disablewallet is provided,
         * the central widget is the rpc console.
         */
        setCentralWidget(rpcConsole);
    }

    if (!firstRun)
    {
        // Create actions for the toolbar, menu bar and tray/dock icon
        // Needs walletFrame to be initialized
        createActions();

        // Accept D&D of URIs
        setAcceptDrops(true);

        // Create application menu bar
        createMenuBar();

        // Create the toolbars
        createToolBars();

        // Create system tray icon and notification
        createTrayIcon(networkStyle);

        // Create status bar
        statusBar();

        // Disable size grip because it looks ugly and nobody needs it
        statusBar()->setSizeGripEnabled(false);
    }

    // Status bar notification icons
    QFrame *frameBlocks = new QFrame();
    frameBlocks->setContentsMargins(0,0,0,0);
    frameBlocks->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    QHBoxLayout *frameBlocksLayout = new QHBoxLayout(frameBlocks);
    frameBlocksLayout->setContentsMargins(3,0,3,0);
    frameBlocksLayout->setSpacing(3);
    unitDisplayControl = new UnitDisplayStatusBarControl(platformStyle);
    labelWalletEncryptionIcon = new QLabel();
    labelWalletHDStatusIcon = new QLabel();
    connectionsControl = new GUIUtil::ClickableLabel();
    labelBlocksIcon = new GUIUtil::ClickableLabel();
    if(enableWallet)
    {
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(unitDisplayControl);
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(labelWalletEncryptionIcon);
        frameBlocksLayout->addWidget(labelWalletHDStatusIcon);
    }
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(connectionsControl);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelBlocksIcon);
    frameBlocksLayout->addStretch();

    // Progress bar and label for blocks download
    progressBarLabel = new QLabel();
    progressBarLabel->setVisible(false);
    progressBar = new GUIUtil::ProgressBar();
    progressBar->setAlignment(Qt::AlignCenter);
    progressBar->setVisible(false);

    // Override style sheet for progress bar for styles that have a segmented progress bar,
    // as they make the text unreadable (workaround for issue #1071)
    // See https://qt-project.org/doc/qt-4.8/gallery.html
    QString curStyle = QApplication::style()->metaObject()->className();
    if(curStyle == "QWindowsStyle" || curStyle == "QWindowsXPStyle")
    {
        progressBar->setStyleSheet("QProgressBar { background-color: #e8e8e8; border: 1px solid grey; border-radius: 7px; padding: 1px; text-align: center; } QProgressBar::chunk { background: QLinearGradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #FF8000, stop: 1 orange); border-radius: 7px; margin: 0px; }");
    }

    statusBar()->addWidget(progressBarLabel);
    statusBar()->addWidget(progressBar);
    statusBar()->addPermanentWidget(frameBlocks);

    if (firstRun)
    {
        statusBar()->hide();
    }
    else
    {
        // Initially wallet actions should be disabled
        setWalletActionsEnabled(false);
    }

    // Install event filter to be able to catch status tip events (QEvent::StatusTip)
    this->installEventFilter(this);

    // Subscribe to notifications from core
    GUIUtil::subscribeToCoreSignals(this);

    connect(connectionsControl, SIGNAL(clicked(QPoint)), this, SLOT(toggleNetworkActive()));

    modalOverlay = new ModalOverlay(this->centralWidget());

    if (firstRun)
    {
        connect(welcomePage, SIGNAL(goToWalletSelection()), this, SLOT(gotoWalletSelectionPage()));
        connect(welcomePage, SIGNAL(goToIntro()), this, SLOT(pickDataDirectory()));

        connect(walletSelectionPage, SIGNAL(goToRestoreWallet()), this, SLOT(gotoRestoreWalletPage()));
        connect(walletSelectionPage, SIGNAL(goToCreateNewWallet()), this, SLOT(processCreateWalletRequest()));

        connect(restoreWalletPage, SIGNAL(backToPreviousPage()), this, SLOT(gotoWalletSelectionPage()));
        connect(restoreWalletPage, SIGNAL(restoreWallet(std::string)), this, SLOT(restoreWallet(std::string)));

        connect(paperKeyIntroPage, SIGNAL(backToPreviousPage()), this, SLOT(gotoWalletSelectionPage()));
        connect(paperKeyIntroPage, SIGNAL(writeDownsClicked()), this, SLOT(createNewWallet()));

        connect(paperKeyWritedownPage, SIGNAL(backToPreviousPage()), this, SLOT(gotoPaperKeyIntroPage()));
        connect(paperKeyWritedownPage, SIGNAL(paperKeyWritten(QStringList)), this, SLOT(gotoPaperKeyCompletionPage(QStringList)));

        connect(paperKeyCompletionPage, SIGNAL(backToPreviousPage()), this, SLOT(gotoPaperKeyWritedownPage()));
        connect(paperKeyCompletionPage, SIGNAL(paperKeyProven()), this, SLOT(showPaperKeyCompleteDialog()));
    }

    connect(setPinPage, SIGNAL(pinReadyForConfirmation(const std::string&)), this, SLOT(setPinCode(const std::string&)));
    connect(setPinPage, SIGNAL(pinReadyForAuthentication(const std::string&)), &AuthManager::getInstance(), SLOT(RequestCheck(const std::string&)));
    connect(setPinPage, SIGNAL(pinValidationFailed()), this, SLOT(gotoSetPinPage()));
    connect(setPinPage, SIGNAL(backToPreviousPage()), this, SLOT(goBackFromSetPinPage()));

    connect(&AuthManager::getInstance(), SIGNAL(Authenticate(bool)), this, SLOT(checkResultAndInterrupt(bool)));
    connect(&AuthManager::getInstance(), SIGNAL(Authenticated()), this, SLOT(continueFromPinRequest()));
}

PAIcoinGUI::~PAIcoinGUI()
{
    // Unsubscribe from notifications from core
    GUIUtil::unsubscribeFromCoreSignals(this);

    GUIUtil::saveWindowGeometry("nWindow", this);
    if(trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
    delete appMenuBar;
    appMenuBar = nullptr;
    delete toolbar;
    toolbar = nullptr;
#ifdef Q_OS_MAC
    MacDockIconHandler::cleanup();
#endif

    delete rpcConsole;
}

void PAIcoinGUI::createActions()
{
    tabGroup = new QActionGroup(this);

    overviewAction = new QAction(platformStyle->SingleColorIcon(":/icons/overview"), tr("&Overview"), this);
    overviewAction->setStatusTip(tr("Show general overview of wallet"));
    overviewAction->setToolTip(overviewAction->statusTip());
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);

    sendCoinsAction = new QAction(platformStyle->SingleColorIcon(":/icons/send"), tr("&Send"), this);
    sendCoinsAction->setStatusTip(tr("Send coins to a PAI Coin address"));
    sendCoinsAction->setToolTip(sendCoinsAction->statusTip());
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(sendCoinsAction);

    sendCoinsMenuAction = new QAction(platformStyle->TextColorIcon(":/icons/send"), sendCoinsAction->text(), this);
    sendCoinsMenuAction->setStatusTip(sendCoinsAction->statusTip());
    sendCoinsMenuAction->setToolTip(sendCoinsMenuAction->statusTip());

    receiveCoinsAction = new QAction(platformStyle->SingleColorIcon(":/icons/receiving_addresses"), tr("&Receive"), this);
    receiveCoinsAction->setStatusTip(tr("Request payments (generates QR codes and paicoin:// URIs)"));
    receiveCoinsAction->setToolTip(receiveCoinsAction->statusTip());
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    tabGroup->addAction(receiveCoinsAction);

    receiveCoinsMenuAction = new QAction(platformStyle->TextColorIcon(":/icons/receiving_addresses"), receiveCoinsAction->text(), this);
    receiveCoinsMenuAction->setStatusTip(receiveCoinsAction->statusTip());
    receiveCoinsMenuAction->setToolTip(receiveCoinsMenuAction->statusTip());

    historyAction = new QAction(platformStyle->SingleColorIcon(":/icons/history"), tr("&Transactions"), this);
    historyAction->setStatusTip(tr("Browse transaction history"));
    historyAction->setToolTip(historyAction->statusTip());
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    tabGroup->addAction(historyAction);

#ifdef ENABLE_WALLET
    // These showNormalIfMinimized are needed because Send Coins and Receive Coins
    // can be triggered from the tray menu, and need to show the GUI to be useful.
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(sendCoinsMenuAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsMenuAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(receiveCoinsMenuAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsMenuAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));
#endif // ENABLE_WALLET

    quitAction = new QAction(platformStyle->TextColorIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction->setStatusTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    aboutAction = new QAction(platformStyle->TextColorIcon(":/icons/about"), tr("&About %1").arg(tr(PACKAGE_NAME)), this);
    aboutAction->setStatusTip(tr("Show information about %1").arg(tr(PACKAGE_NAME)));
    aboutAction->setMenuRole(QAction::AboutRole);
    aboutAction->setEnabled(false);
    aboutQtAction = new QAction(platformStyle->TextColorIcon(":/icons/about_qt"), tr("About &Qt"), this);
    aboutQtAction->setStatusTip(tr("Show information about Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    optionsAction = new QAction(platformStyle->TextColorIcon(":/icons/options"), tr("&Options..."), this);
    optionsAction->setStatusTip(tr("Modify configuration options for %1").arg(tr(PACKAGE_NAME)));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    optionsAction->setEnabled(false);
    toggleHideAction = new QAction(platformStyle->TextColorIcon(":/icons/about"), tr("&Show / Hide"), this);
    toggleHideAction->setStatusTip(tr("Show or hide the main Window"));

#ifdef ENABLE_ENCRYPT_WALLET
    encryptWalletAction = new QAction(platformStyle->TextColorIcon(":/icons/lock_closed"), tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setStatusTip(tr("Encrypt the private keys that belong to your wallet"));
    encryptWalletAction->setCheckable(true);
    changePassphraseAction = new QAction(platformStyle->TextColorIcon(":/icons/key"), tr("&Change Passphrase..."), this);
    changePassphraseAction->setStatusTip(tr("Change the passphrase used for wallet encryption"));
#endif // ENABLE_ENCRYPT_WALLET
    backupWalletAction = new QAction(platformStyle->TextColorIcon(":/icons/filesave"), tr("&Backup Wallet..."), this);
    backupWalletAction->setStatusTip(tr("Backup wallet to another location"));
    signMessageAction = new QAction(platformStyle->TextColorIcon(":/icons/edit"), tr("Sign &message..."), this);
    signMessageAction->setStatusTip(tr("Sign messages with your PAI Coin addresses to prove you own them"));
    verifyMessageAction = new QAction(platformStyle->TextColorIcon(":/icons/verify"), tr("&Verify message..."), this);
    verifyMessageAction->setStatusTip(tr("Verify messages to ensure they were signed with specified PAI Coin addresses"));

    viewInvestorKeyAction = new QAction(tr("&View Investor Key"), this);
    viewInvestorKeyAction->setStatusTip(tr("View Investor Key"));

    reviewPaperKeyAction = new QAction(tr("&Review Paper Key"), this);
    reviewPaperKeyAction->setStatusTip(tr("Review Paper Key"));

    openRPCConsoleAction = new QAction(platformStyle->TextColorIcon(":/icons/debugwindow"), tr("&Debug window"), this);
    openRPCConsoleAction->setStatusTip(tr("Open debugging and diagnostic console"));
    // initially disable the debug window menu item
    openRPCConsoleAction->setEnabled(false);

    usedSendingAddressesAction = new QAction(platformStyle->TextColorIcon(":/icons/address-book"), tr("&Sending addresses..."), this);
    usedSendingAddressesAction->setStatusTip(tr("Show the list of used sending addresses and labels"));
    usedReceivingAddressesAction = new QAction(platformStyle->TextColorIcon(":/icons/address-book"), tr("&Receiving addresses..."), this);
    usedReceivingAddressesAction->setStatusTip(tr("Show the list of used receiving addresses and labels"));

    openAction = new QAction(platformStyle->TextColorIcon(":/icons/open"), tr("Open &URI..."), this);
    openAction->setStatusTip(tr("Open a paicoin:// URI or payment request"));

    showHelpMessageAction = new QAction(platformStyle->TextColorIcon(":/icons/info"), tr("&Command-line options"), this);
    showHelpMessageAction->setMenuRole(QAction::NoRole);
    showHelpMessageAction->setStatusTip(tr("Show the %1 help message to get a list with possible PAI Coin command-line options").arg(tr(PACKAGE_NAME)));

    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(showHelpMessageAction, SIGNAL(triggered()), this, SLOT(showHelpMessageClicked()));
    connect(openRPCConsoleAction, SIGNAL(triggered()), this, SLOT(showDebugWindow()));
    // prevents an open debug window from becoming stuck/unusable on client shutdown
    connect(quitAction, SIGNAL(triggered()), rpcConsole, SLOT(hide()));

#ifdef ENABLE_WALLET
    if(walletFrame)
    {
#ifdef ENABLE_ENCRYPT_WALLET
        connect(encryptWalletAction, SIGNAL(triggered(bool)), walletFrame, SLOT(encryptWallet(bool)));
        connect(changePassphraseAction, SIGNAL(triggered()), walletFrame, SLOT(changePassphrase()));
#endif // ENABLE_ENCRYPT_WALLET
        connect(backupWalletAction, SIGNAL(triggered()), walletFrame, SLOT(backupWallet()));
        connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
        connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
        connect(usedSendingAddressesAction, SIGNAL(triggered()), walletFrame, SLOT(usedSendingAddresses()));
        connect(usedReceivingAddressesAction, SIGNAL(triggered()), walletFrame, SLOT(usedReceivingAddresses()));
        connect(openAction, SIGNAL(triggered()), this, SLOT(openClicked()));
        connect(viewInvestorKeyAction, SIGNAL(triggered()), walletFrame, SLOT(viewInvestorKey()));
        connect(reviewPaperKeyAction, SIGNAL(triggered()), walletFrame, SLOT(reviewPaperKey()));
    }
#endif // ENABLE_WALLET

    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_C), this, SLOT(showDebugWindowActivateConsole()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_D), this, SLOT(showDebugWindow()));

    tabGroup->setVisible(!firstRun);
}

void PAIcoinGUI::createMenuBar()
{
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif

    // Configure the menus
    fileMenu = appMenuBar->addMenu(tr("&File"));
    if(walletFrame)
    {
        fileMenu->addAction(openAction);
        fileMenu->addAction(backupWalletAction);
        fileMenu->addAction(signMessageAction);
        fileMenu->addAction(verifyMessageAction);
        fileMenu->addSeparator();
        fileMenu->addAction(usedSendingAddressesAction);
        fileMenu->addAction(usedReceivingAddressesAction);
        fileMenu->addSeparator();
    }
    fileMenu->addAction(quitAction);

    settingsMenu = appMenuBar->addMenu(tr("&Settings"));
    if(walletFrame)
    {
#ifdef ENABLE_ENCRYPT_WALLET
        settingsMenu->addAction(encryptWalletAction);
        settingsMenu->addAction(changePassphraseAction);
#endif // ENABLE_ENCRYPT_WALLET
        settingsMenu->addAction(viewInvestorKeyAction);
        settingsMenu->addAction(reviewPaperKeyAction);
        settingsMenu->addSeparator();
    }
    settingsMenu->addAction(optionsAction);

    helpMenu = appMenuBar->addMenu(tr("&Help"));
    if(walletFrame)
    {
        helpMenu->addAction(openRPCConsoleAction);
    }
    helpMenu->addAction(showHelpMessageAction);
    helpMenu->addSeparator();
    helpMenu->addAction(aboutAction);
    helpMenu->addAction(aboutQtAction);
}

void PAIcoinGUI::updateMenuBar(bool locked)
{
    if (locked)
    {
        if (walletFrame)
        {
            // Remove sensitive items from File menu
            fileMenu->removeAction(openAction);
            fileMenu->removeAction(backupWalletAction);
            fileMenu->removeAction(signMessageAction);
            fileMenu->removeAction(verifyMessageAction);
            fileMenu->removeAction(usedSendingAddressesAction);
            fileMenu->removeAction(usedReceivingAddressesAction);

            // Remove sensitive items from Help menu
            helpMenu->removeAction(openRPCConsoleAction);
        }

        // Delete Settings menu (a workaround for Qt menu misbehaving on Unity and OSX environments)
        delete settingsMenu;
        settingsMenu = nullptr;

        // Hide application menu from main window
        appMenuBar->hide();
    }
    else
    {
        if(walletFrame)
        {
            // Re-create File menu organization
            QAction* quitSeparator = fileMenu->insertSeparator(quitAction);
            fileMenu->insertAction(quitSeparator, usedReceivingAddressesAction);
            fileMenu->insertAction(usedReceivingAddressesAction, usedSendingAddressesAction);
            QAction* sendingAddressesSeparator = fileMenu->insertSeparator(usedSendingAddressesAction);
            fileMenu->insertAction(sendingAddressesSeparator, verifyMessageAction);
            fileMenu->insertAction(verifyMessageAction, signMessageAction);
            fileMenu->insertAction(signMessageAction, backupWalletAction);
            fileMenu->insertAction(backupWalletAction, openAction);

            // Re-create Help menu organization
            helpMenu->insertAction(showHelpMessageAction, openRPCConsoleAction);
        }

        // Re-create Settings menu
        if (settingsMenu == nullptr)
        {
            settingsMenu = new QMenu(tr("&Settings"));
            appMenuBar->insertMenu(helpMenu->menuAction(), settingsMenu);
        }
        if(walletFrame)
        {
#ifdef ENABLE_ENCRYPT_WALLET
            settingsMenu->addAction(encryptWalletAction);
            settingsMenu->addAction(changePassphraseAction);
#endif // ENABLE_ENCRYPT_WALLET
            settingsMenu->addAction(viewInvestorKeyAction);
            settingsMenu->addAction(reviewPaperKeyAction);
            settingsMenu->addSeparator();
        }
        settingsMenu->addAction(optionsAction);

        // Show application menu from main window
        appMenuBar->show();
    }
}

void PAIcoinGUI::createToolBars()
{
    if(walletFrame)
    {
        toolbar = addToolBar(tr("Tabs toolbar"));
        toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
        toolbar->setMovable(false);
        toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        toolbar->addAction(overviewAction);
        toolbar->addAction(sendCoinsAction);
        toolbar->addAction(receiveCoinsAction);
        toolbar->addAction(historyAction);
        overviewAction->setChecked(true);
    }
}

void PAIcoinGUI::setClientModel(ClientModel *_clientModel)
{
    this->clientModel = _clientModel;
    if(_clientModel)
    {
        // Create system tray menu (or setup the dock menu) that late to prevent users from calling actions,
        // while the client has not yet fully loaded
        createTrayIconMenu();

        // Keep up to date with client
        updateNetworkState();
        connect(_clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));
        connect(_clientModel, SIGNAL(networkActiveChanged(bool)), this, SLOT(setNetworkActive(bool)));

        modalOverlay->setKnownBestHeight(_clientModel->getHeaderTipHeight(), QDateTime::fromTime_t(_clientModel->getHeaderTipTime()));
        setNumBlocks(_clientModel->getNumBlocks(), _clientModel->getLastBlockDate(), _clientModel->getVerificationProgress(nullptr), false);
        connect(_clientModel, SIGNAL(numBlocksChanged(int,QDateTime,double,bool)), this, SLOT(setNumBlocks(int,QDateTime,double,bool)));

        // Receive and report messages from client model
        connect(_clientModel, SIGNAL(message(QString,QString,unsigned int)), this, SLOT(message(QString,QString,unsigned int)));

        // Show progress dialog
        connect(_clientModel, SIGNAL(showProgress(QString,int)), this, SLOT(showProgress(QString,int)));

        rpcConsole->setClientModel(_clientModel);
#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->setClientModel(_clientModel);
        }
#endif // ENABLE_WALLET
        unitDisplayControl->setOptionsModel(_clientModel->getOptionsModel());

        OptionsModel* optionsModel = _clientModel->getOptionsModel();
        if(optionsModel)
        {
            // be aware of the tray icon disable state change reported by the OptionsModel object.
            connect(optionsModel,SIGNAL(hideTrayIconChanged(bool)),this,SLOT(setTrayIconVisible(bool)));

            // initialize the disable state of the tray icon with the current value in the model.
            setTrayIconVisible(optionsModel->getHideTrayIcon());
        }
    } else {
        // Disable possibility to show main window via action
        if (toggleHideAction)
            toggleHideAction->setEnabled(false);
        if(trayIconMenu)
        {
            // Disable context menu on tray icon
            trayIconMenu->clear();
        }
        // Propagate cleared model to child objects
        rpcConsole->setClientModel(nullptr);
#ifdef ENABLE_WALLET
        if (walletFrame)
        {
            walletFrame->setClientModel(nullptr);
        }
#endif // ENABLE_WALLET
        unitDisplayControl->setOptionsModel(nullptr);
    }
}

#ifdef ENABLE_WALLET
bool PAIcoinGUI::addWallet(const QString& name, WalletModel *walletModel)
{
    if(!walletFrame)
        return false;
    setWalletActionsEnabled(true);
    return walletFrame->addWallet(name, walletModel);
}

bool PAIcoinGUI::setCurrentWallet(const QString& name)
{
    if(!walletFrame)
        return false;
    return walletFrame->setCurrentWallet(name);
}

void PAIcoinGUI::removeAllWallets()
{
    if(!walletFrame)
        return;
    setWalletActionsEnabled(false);
    walletFrame->removeAllWallets();
}
#endif // ENABLE_WALLET

void PAIcoinGUI::setWalletActionsEnabled(bool enabled)
{
    overviewAction->setEnabled(enabled);
    sendCoinsAction->setEnabled(enabled);
    sendCoinsMenuAction->setEnabled(enabled);
    receiveCoinsAction->setEnabled(enabled);
    receiveCoinsMenuAction->setEnabled(enabled);
    historyAction->setEnabled(enabled);
#ifdef ENABLE_ENCRYPT_WALLET
    encryptWalletAction->setEnabled(enabled);
    changePassphraseAction->setEnabled(enabled);
#endif // ENABLE_ENCRYPT_WALLET
    backupWalletAction->setEnabled(enabled);
    signMessageAction->setEnabled(enabled);
    verifyMessageAction->setEnabled(enabled);
    usedSendingAddressesAction->setEnabled(enabled);
    usedReceivingAddressesAction->setEnabled(enabled);
    openAction->setEnabled(enabled);
}

void PAIcoinGUI::createTrayIcon(const NetworkStyle *networkStyle)
{
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    QString toolTip = tr("%1 client").arg(tr(PACKAGE_NAME)) + " " + networkStyle->getTitleAddText();
    trayIcon->setToolTip(toolTip);
    trayIcon->setIcon(networkStyle->getTrayAndWindowIcon());
    trayIcon->hide();
#endif

    notificator = new Notificator(QApplication::applicationName(), trayIcon, this);
}

void PAIcoinGUI::createTrayIconMenu()
{
#ifndef Q_OS_MAC
    // return if trayIcon is unset (only on non-Mac OSes)
    if (!trayIcon)
        return;

    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    dockIconHandler->setMainWindow((QMainWindow *)this);
    trayIconMenu = dockIconHandler->dockMenu();
#endif

    // Configuration of the tray icon (or dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(sendCoinsMenuAction);
    trayIconMenu->addAction(receiveCoinsMenuAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(signMessageAction);
    trayIconMenu->addAction(verifyMessageAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addAction(openRPCConsoleAction);
#ifndef Q_OS_MAC // This is built-in on Mac
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif
}

void PAIcoinGUI::updateTrayIconMenu(bool locked)
{
    if (locked)
    {
        // Remove sensitive items from Tray menu
        trayIconMenu->removeAction(sendCoinsMenuAction);
        trayIconMenu->removeAction(receiveCoinsMenuAction);
        trayIconMenu->removeAction(signMessageAction);
        trayIconMenu->removeAction(verifyMessageAction);
        trayIconMenu->removeAction(optionsAction);
        trayIconMenu->removeAction(openRPCConsoleAction);
    }
    else
    {
        // Re-create tray menu
#ifndef Q_OS_MAC
        QAction* quitSeparator = trayIconMenu->insertSeparator(quitAction);
        trayIconMenu->insertAction(quitSeparator, openRPCConsoleAction);
#else
        trayIconMenu->removeAction(toggleHideAction);
        trayIconMenu->addAction(openRPCConsoleAction);
#endif
        trayIconMenu->insertAction(openRPCConsoleAction, optionsAction);
        QAction* optionsSeparator = trayIconMenu->insertSeparator(optionsAction);
        trayIconMenu->insertAction(optionsSeparator, verifyMessageAction);
        trayIconMenu->insertAction(verifyMessageAction, signMessageAction);
        QAction* signMessageSeparator = trayIconMenu->insertSeparator(signMessageAction);
        trayIconMenu->insertAction(signMessageSeparator, receiveCoinsMenuAction);
        trayIconMenu->insertAction(receiveCoinsMenuAction, sendCoinsMenuAction);
        QAction* sendCoinsSeparator = trayIconMenu->insertSeparator(sendCoinsMenuAction);
#ifdef Q_OS_MAC
        trayIconMenu->insertAction(sendCoinsSeparator, toggleHideAction);
#endif
    }
}

#ifndef Q_OS_MAC
void PAIcoinGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger)
    {
        // Click on system tray icon triggers show/hide of the main window
        toggleHidden();
    }
}
#endif

void PAIcoinGUI::optionsClicked()
{
    if(!clientModel || !clientModel->getOptionsModel())
        return;


    OptionsDialog dlg(this, enableWallet);
    dlg.setModel(clientModel->getOptionsModel());
    dlg.exec();
}

void PAIcoinGUI::aboutClicked()
{
    if(!clientModel)
        return;

    HelpMessageDialog dlg(this, true);
    dlg.exec();
}

void PAIcoinGUI::showDebugWindow()
{
    rpcConsole->showNormal();
    rpcConsole->show();
    rpcConsole->raise();
    rpcConsole->activateWindow();
}

void PAIcoinGUI::showDebugWindowActivateConsole()
{
    rpcConsole->setTabFocus(RPCConsole::TAB_CONSOLE);
    showDebugWindow();
}

void PAIcoinGUI::showHelpMessageClicked()
{
    helpMessageDialog->show();
}

void PAIcoinGUI::setPinCode(const std::string &pin)
{
    AuthManager::getInstance().SetPinCode(pin);

    ConfirmationDialog *confirmationDialog = new ConfirmationDialog(tr("PIN Set"), this);
    confirmationDialog->exec();

    switch(state)
    {
    case PAIcoinGUIState::RestoreWallet:
    case PAIcoinGUIState::PaperKeyCompletion:
        Q_EMIT linkWalletToMainApp();
        break;
    case PAIcoinGUIState::Init:
        AuthManager::getInstance().RequestCheck(pin);
        break;
    default:
        assert(!"Unexpected state!");
        break;
    }
}

void PAIcoinGUI::pickDataDirectory()
{
    /// Ask user for data directory
    // User language is set up: pick a data directory
    if (Intro::pickDataDirectory())
    {
        bool shouldShutdown = false;

        /// Determine availability of data directory and parse paicoin.conf
        /// - Do not call GetDataDir(true) before this step finishes
        if (!fs::is_directory(GetDataDir(false)))
        {
            QMessageBox::critical(0,
                                  QObject::tr(PACKAGE_NAME),
                                  QObject::tr("Error: Specified data directory \"%1\" does not exist.")
                                  .arg(QString::fromStdString(gArgs.GetArg("-datadir", ""))));
            shouldShutdown = true;
        }

        try
        {
            gArgs.ReadConfigFile(gArgs.GetArg("-conf", PAICOIN_CONF_FILENAME));
        }
        catch (const std::exception& e)
        {
            QMessageBox::critical(0,
                                  QObject::tr(PACKAGE_NAME),
                                  QObject::tr("Error: Cannot parse configuration file: %1. Only use key=value syntax.")
                                  .arg(e.what()));
            shouldShutdown = true;
        }

        if (shouldShutdown)
            Q_EMIT shutdown();
        else
            gotoWalletSelectionPage();
    }
}

#ifdef ENABLE_WALLET
void PAIcoinGUI::openClicked()
{
    OpenURIDialog dlg(this);
    if(dlg.exec())
    {
        Q_EMIT receivedURI(dlg.getURI());
    }
}

void PAIcoinGUI::interruptForPinRequest(bool newPin)
{
    previousState = state;

    if (previousState == PAIcoinGUIState::Init)
    {
        updateMenuBar(true);
        updateTrayIconMenu(true);
        toolbar->hide();
        tabGroup->setVisible(false);
        progressBar->hide();
        statusBar()->hide();
        modalOverlay->showHide(true, true);
    }

    if (newPin)
        setPinPage->initSetPinLayout();
    else
    {
        setPinPage->initPinRequiredLayout();
        installEventFilter(setPinPage);
    }
    gotoSetPinPage();
}

void PAIcoinGUI::checkResultAndInterrupt(bool previousPinInvalid)
{
    if (previousPinInvalid)
    {
        std::unique_ptr<ConfirmationDialog> confirmationDialog(
                    new ConfirmationDialog(tr("Invalid PIN, please try again"), this));
        confirmationDialog->exec();
    }
    interruptForPinRequest();
}

void PAIcoinGUI::continueFromPinRequest()
{
    switch(previousState)
    {
    case PAIcoinGUIState::Init:
        mainStackedWidget->setCurrentWidget(walletFrame);
        updateMenuBar();
        updateTrayIconMenu();
        toolbar->show();
        tabGroup->setVisible(true);
        if (walletFrame && walletFrame->isOutOfSync())
            modalOverlay->showHide();

        connect(walletFrame, SIGNAL(requestedSyncWarningInfo()), this, SLOT(showModalOverlay()));
        connect(labelBlocksIcon, SIGNAL(clicked(QPoint)), this, SLOT(showModalOverlay()));
        connect(progressBar, SIGNAL(clicked(QPoint)), this, SLOT(showModalOverlay()));
        break;
    default:
        assert(!"Unexpected state!");
        break;
    }
}

void PAIcoinGUI::gotoWalletSelectionPage()
{
    AuthManager::getInstance().Reset();
    state = PAIcoinGUIState::WalletSelection;
    mainStackedWidget->setCurrentWidget(walletSelectionPage);
}

void PAIcoinGUI::createNewWallet()
{
    Q_EMIT createNewWalletRequest();
}

void PAIcoinGUI::restoreWallet(std::string phrase)
{
    Q_EMIT restoreWalletRequest(phrase.c_str());
}

void PAIcoinGUI::gotoRestoreWalletPage()
{
    state = PAIcoinGUIState::RestoreWallet;
    restoreWalletPage->clear();
    mainStackedWidget->setCurrentWidget(restoreWalletPage);
}

void PAIcoinGUI::processCreateWalletRequest()
{
    state = PAIcoinGUIState::CreateWallet;
    gotoPaperKeyIntroPage();
}

void PAIcoinGUI::gotoPaperKeyIntroPage()
{
    state = PAIcoinGUIState::PaperKeyIntro;
    mainStackedWidget->setCurrentWidget(paperKeyIntroPage);
}

void PAIcoinGUI::gotoSetPinPage()
{
    mainStackedWidget->setCurrentWidget(setPinPage);
}

void PAIcoinGUI::gotoPaperKeyWritedownPage()
{
    state = PAIcoinGUIState::PaperKeyWritedown;
    mainStackedWidget->setCurrentWidget(paperKeyWritedownPage);
}

void PAIcoinGUI::gotoPaperKeyCompletionPage(const QStringList &phrase)
{
    paperKeyCompletionPage->setPaperKeyList(phrase);
    state = PAIcoinGUIState::PaperKeyCompletion;
    mainStackedWidget->setCurrentWidget(paperKeyCompletionPage);
}

void PAIcoinGUI::showPaperKeyCompleteDialog()
{
    // New wallet has been created and we need to display paper key complete dialog, and link wallet to the app itself
    ConfirmationDialog *confirmationDialog = new ConfirmationDialog(tr("Paper Key Complete"), this);
    confirmationDialog->exec();
    gotoSetPinPage();
}

void PAIcoinGUI::gotoOverviewPage()
{
    overviewAction->setChecked(true);
    if (walletFrame) walletFrame->gotoOverviewPage();
}

void PAIcoinGUI::gotoHistoryPage()
{
    historyAction->setChecked(true);
    if (walletFrame) walletFrame->gotoHistoryPage();
}

void PAIcoinGUI::gotoReceiveCoinsPage()
{
    receiveCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoReceiveCoinsPage();
}

void PAIcoinGUI::gotoSendCoinsPage(QString addr)
{
    sendCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoSendCoinsPage(addr);
}

void PAIcoinGUI::goBackFromSetPinPage()
{
    if (state == PAIcoinGUIState::RestoreWallet)
        gotoRestoreWalletPage();
    else
        gotoWalletSelectionPage();
}

void PAIcoinGUI::gotoSignMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoSignMessageTab(addr);
}

void PAIcoinGUI::gotoVerifyMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoVerifyMessageTab(addr);
}
#endif // ENABLE_WALLET

void PAIcoinGUI::updateNetworkState()
{
    int count = clientModel->getNumConnections();
    QString icon;
    switch(count)
    {
    case 0: icon = ":/icons/connect_0"; break;
    case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
    case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
    case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
    default: icon = ":/icons/connect_4"; break;
    }

    QString tooltip;

    if (clientModel->getNetworkActive()) {
        tooltip = tr("%n active connection(s) to PAI Coin network", "", count) + QString(".<br>") + tr("Click to disable network activity.");
    } else {
        tooltip = tr("Network activity disabled.") + QString("<br>") + tr("Click to enable network activity again.");
        icon = ":/icons/network_disabled";
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");
    connectionsControl->setToolTip(tooltip);

    connectionsControl->setPixmap(platformStyle->SingleColorIcon(icon).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
}

void PAIcoinGUI::setNumConnections(int count)
{
    updateNetworkState();
}

void PAIcoinGUI::setNetworkActive(bool networkActive)
{
    updateNetworkState();
}

void PAIcoinGUI::updateHeadersSyncProgressLabel()
{
    int64_t headersTipTime = clientModel->getHeaderTipTime();
    int headersTipHeight = clientModel->getHeaderTipHeight();
    int estHeadersLeft = (GetTime() - headersTipTime) / Params().GetConsensus().nPowTargetSpacing;
    if (estHeadersLeft > HEADER_HEIGHT_DELTA_SYNC)
        progressBarLabel->setText(tr("Syncing Headers (%1%)...").arg(QString::number(100.0 / (headersTipHeight+estHeadersLeft)*headersTipHeight, 'f', 1)));
}

void PAIcoinGUI::enableDebugWindow()
{
    // enable the debug window when the main window shows up
    openRPCConsoleAction->setEnabled(true);
    aboutAction->setEnabled(true);
    optionsAction->setEnabled(true);
}

bool PAIcoinGUI::ShouldAuthenticate() const
{
    return AuthManager::getInstance().AuthRequested();
}

void PAIcoinGUI::setNumBlocks(int count, const QDateTime& blockDate, double nVerificationProgress, bool header)
{
    if (modalOverlay)
    {
        if (header)
            modalOverlay->setKnownBestHeight(count, blockDate);
        else
            modalOverlay->tipUpdate(count, blockDate, nVerificationProgress);
    }
    if (!clientModel)
        return;

    // Prevent orphan statusbar messages (e.g. hover Quit in main menu, wait until chain-sync starts -> garbled text)
    statusBar()->clearMessage();

    // Acquire current block source
    enum BlockSource blockSource = clientModel->getBlockSource();
    switch (blockSource) {
        case BLOCK_SOURCE_NETWORK:
            if (header) {
                updateHeadersSyncProgressLabel();
                return;
            }
            progressBarLabel->setText(tr("Synchronizing with network..."));
            updateHeadersSyncProgressLabel();
            break;
        case BLOCK_SOURCE_DISK:
            if (header) {
                progressBarLabel->setText(tr("Indexing blocks on disk..."));
            } else {
                progressBarLabel->setText(tr("Processing blocks on disk..."));
            }
            break;
        case BLOCK_SOURCE_REINDEX:
            progressBarLabel->setText(tr("Reindexing blocks on disk..."));
            break;
        case BLOCK_SOURCE_NONE:
            if (header) {
                return;
            }
            progressBarLabel->setText(tr("Connecting to peers..."));
            break;
    }

    QString tooltip;

    QDateTime currentDate = QDateTime::currentDateTime();
    qint64 secs = blockDate.secsTo(currentDate);

    tooltip = tr("Processed %n block(s) of transaction history.", "", count);

    // Set icon state: spinning if catching up, tick otherwise
    if(secs < 90*60)
    {
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
        labelBlocksIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->showOutOfSyncWarning(false);
            modalOverlay->showHide(true, true);
        }
#endif // ENABLE_WALLET

        progressBarLabel->setVisible(false);
        progressBar->setVisible(false);
    }
    else
    {
        QString timeBehindText = GUIUtil::formatNiceTimeOffset(secs);

        progressBarLabel->setVisible(true);
        progressBar->setFormat(tr("%1 behind").arg(timeBehindText));
        progressBar->setMaximum(1000000000);
        progressBar->setValue(nVerificationProgress * 1000000000.0 + 0.5);
        progressBar->setVisible(true);

        tooltip = tr("Catching up...") + QString("<br>") + tooltip;
        if(count != prevBlocks)
        {
            labelBlocksIcon->setPixmap(platformStyle->SingleColorIcon(QString(
                ":/movies/spinner-%1").arg(spinnerFrame, 3, 10, QChar('0')))
                .pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
            spinnerFrame = (spinnerFrame + 1) % SPINNER_FRAMES;
        }
        prevBlocks = count;

#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->showOutOfSyncWarning(true);
        }
#endif // ENABLE_WALLET

        tooltip += QString("<br>");
        tooltip += tr("Last received block was generated %1 ago.").arg(timeBehindText);
        tooltip += QString("<br>");
        tooltip += tr("Transactions after this will not yet be visible.");
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    labelBlocksIcon->setToolTip(tooltip);
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
}

void PAIcoinGUI::message(const QString &title, const QString &message, unsigned int style, bool *ret)
{
    QString            strTitle;
    QMessageBox::Icon  nMBoxIcon;
    Notificator::Class nNotifyIcon;

    std::tie(strTitle,nMBoxIcon, nNotifyIcon) = GUIUtil::getMessageProperties(title,style);
    // Display message
    if (style & CClientUIInterface::MODAL) {
        showNormalIfMinimized();
        GUIUtil::showMessageBox(nMBoxIcon,strTitle,message,this,style,ret);
    }
    else
        notificator->notify(nNotifyIcon, strTitle, message);
}

void PAIcoinGUI::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if(e->type() == QEvent::WindowStateChange)
    {
        if(clientModel && clientModel->getOptionsModel() && clientModel->getOptionsModel()->getMinimizeToTray())
        {
            QWindowStateChangeEvent *wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if(!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized())
            {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
#endif
}

void PAIcoinGUI::closeEvent(QCloseEvent *event)
{
    if (firstRun)
    {
#ifndef Q_OS_MAC // Ignored on Mac
        QApplication::quit();
    }
    else if(clientModel && clientModel->getOptionsModel())
    {
        if(!clientModel->getOptionsModel()->getMinimizeOnClose())
        {
            // close rpcConsole in case it was open to make some space for the shutdown window
            rpcConsole->close();

            QApplication::quit();
        }
        else
        {
            QMainWindow::showMinimized();
            event->ignore();
        }
    }
#else
    }
    QMainWindow::closeEvent(event);
#endif
}

void PAIcoinGUI::showEvent(QShowEvent *event)
{
    if (!firstRun)
        enableDebugWindow();
}

#ifdef ENABLE_WALLET
void PAIcoinGUI::incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type, const QString& address, const QString& label)
{
    // On new transaction, make an info balloon
    QString msg = tr("Date: %1\n").arg(date) +
                  tr("Amount: %1\n").arg(PAIcoinUnits::formatWithUnit(unit, amount, true)) +
                  tr("Type: %1\n").arg(type);
    if (!label.isEmpty())
        msg += tr("Label: %1\n").arg(label);
    else if (!address.isEmpty())
        msg += tr("Address: %1\n").arg(address);
    message((amount)<0 ? tr("Sent transaction") : tr("Incoming transaction"),
             msg, CClientUIInterface::MSG_INFORMATION);
}

void PAIcoinGUI::walletCreated(std::string phrase)
{
    // Navigate to paper key intro and continue with writedown
    paperKeyWritedownPage->setPhrase(phrase);
    gotoPaperKeyWritedownPage();
}

void PAIcoinGUI::walletRestored(std::string phrase)
{
    ConfirmationDialog *confirmationDialog = new ConfirmationDialog(tr("Wallet Restored"), this);
    confirmationDialog->exec();

    gotoSetPinPage();
}

void PAIcoinGUI::createWalletFrame()
{
    firstRun = false;
    Q_EMIT firstRunComplete();

    if (walletFrame != nullptr)
        mainStackedWidget->removeWidget(walletFrame);

    // Create wallet frame and make it the central widget
    walletFrame = new WalletFrame(platformStyle, this);
    walletFrame->setClientModel(clientModel);

    // Create actions for the toolbar, menu bar and tray/dock icon
    // Needs walletFrame to be initialized
    createActions();

    mainStackedWidget->addWidget(walletFrame);
    mainStackedWidget->setCurrentWidget(walletFrame);

    modalOverlay->setParent(this->centralWidget());
    modalOverlay->showHide();
}

void PAIcoinGUI::completeUiWalletInitialization()
{
    // Re-enable all UI features disabled during first run

    // Accept D&D of URIs
    setAcceptDrops(true);

    // Create application menu bar
    createMenuBar();

    // Create the toolbars
    createToolBars();

    // Create system tray icon and notification
    createTrayIcon(networkStyle);

    // Create status bar
    statusBar();

    // Disable size grip because it looks ugly and nobody needs it
    statusBar()->setSizeGripEnabled(false);

    statusBar()->show();

    enableDebugWindow();

    connect(walletFrame, SIGNAL(requestedSyncWarningInfo()), this, SLOT(showModalOverlay()));
    connect(labelBlocksIcon, SIGNAL(clicked(QPoint)), this, SLOT(showModalOverlay()));
    connect(progressBar, SIGNAL(clicked(QPoint)), this, SLOT(showModalOverlay()));
}

#endif // ENABLE_WALLET

void PAIcoinGUI::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept only URIs
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void PAIcoinGUI::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasUrls())
    {
        for (const QUrl &uri : event->mimeData()->urls())
        {
            Q_EMIT receivedURI(uri.toString());
        }
    }
    event->acceptProposedAction();
}

bool PAIcoinGUI::eventFilter(QObject *object, QEvent *event)
{
    // Catch status tip events
    if (event->type() == QEvent::StatusTip)
    {
        // Prevent adding text from setStatusTip(), if we currently use the status bar for displaying other stuff
        if (progressBarLabel->isVisible() || progressBar->isVisible())
            return true;
    }
    return QMainWindow::eventFilter(object, event);
}

#ifdef ENABLE_WALLET
bool PAIcoinGUI::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
    // URI has to be valid
    if (walletFrame && walletFrame->handlePaymentRequest(recipient))
    {
        showNormalIfMinimized();
        gotoSendCoinsPage();
        return true;
    }
    return false;
}

void PAIcoinGUI::setHDStatus(int hdEnabled)
{
    labelWalletHDStatusIcon->setPixmap(platformStyle->SingleColorIcon(hdEnabled ? ":/icons/hd_enabled" : ":/icons/hd_disabled").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelWalletHDStatusIcon->setToolTip(hdEnabled ? tr("HD key generation is <b>enabled</b>") : tr("HD key generation is <b>disabled</b>"));

    // eventually disable the QLabel to set its opacity to 50%
    labelWalletHDStatusIcon->setEnabled(hdEnabled);
}

#ifdef ENABLE_ENCRYPT_WALLET
void PAIcoinGUI::setEncryptionStatus(int status)
{
    switch(status)
    {
    case WalletModel::Unencrypted:
        labelWalletEncryptionIcon->hide();
        changePassphraseAction->setEnabled(false);
        encryptWalletAction->setChecked(false);
        encryptWalletAction->setEnabled(true);
        break;
    case WalletModel::Unlocked:
        labelWalletEncryptionIcon->show();
        labelWalletEncryptionIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/lock_open").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelWalletEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    case WalletModel::Locked:
        labelWalletEncryptionIcon->show();
        labelWalletEncryptionIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/lock_closed").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelWalletEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    }
}
#endif // ENABLE_ENCRYPT_WALLET

#endif // ENABLE_WALLET

void PAIcoinGUI::showNormalIfMinimized(bool fToggleHidden)
{
    if(!clientModel)
        return;

    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden())
    {
        show();
        activateWindow();
    }
    else if (isMinimized())
    {
        showNormal();
        activateWindow();
    }
    else if (GUIUtil::isObscured(this))
    {
        raise();
        activateWindow();
    }
    else if(fToggleHidden)
        hide();
}

void PAIcoinGUI::toggleHidden()
{
    showNormalIfMinimized(true);
}

void PAIcoinGUI::detectShutdown()
{
    if (ShutdownRequested())
    {
        if(rpcConsole)
            rpcConsole->hide();
        qApp->quit();
    }
}

void PAIcoinGUI::showProgress(const QString &title, int nProgress)
{
    if (nProgress == 0)
    {
        progressDialog = new QProgressDialog(title, "", 0, 100);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);
        progressDialog->setCancelButton(0);
        progressDialog->setAutoClose(false);
        progressDialog->setValue(0);
    }
    else if (nProgress == 100)
    {
        if (progressDialog)
        {
            progressDialog->close();
            progressDialog->deleteLater();
        }
    }
    else if (progressDialog)
        progressDialog->setValue(nProgress);
}

void PAIcoinGUI::setTrayIconVisible(bool fHideTrayIcon)
{
    if (trayIcon)
    {
        trayIcon->setVisible(!fHideTrayIcon);
    }
}

void PAIcoinGUI::showModalOverlay()
{
    if (modalOverlay && (progressBar->isVisible() || modalOverlay->isLayerVisible()) && !SettingsHelper::IsAuthRequested())
        modalOverlay->toggleVisibility();
}

void PAIcoinGUI::toggleNetworkActive()
{
    if (clientModel) {
        clientModel->setNetworkActive(!clientModel->getNetworkActive());
    }
}

UnitDisplayStatusBarControl::UnitDisplayStatusBarControl(const PlatformStyle *platformStyle) :
    optionsModel(0),
    menu(0)
{
    createContextMenu();
    setToolTip(tr("Unit to show amounts in. Click to select another unit."));
    QList<PAIcoinUnits::Unit> units = PAIcoinUnits::availableUnits();
    int max_width = 0;
    const QFontMetrics fm(font());
    for (const PAIcoinUnits::Unit unit : units)
    {
        max_width = qMax(max_width, fm.width(PAIcoinUnits::name(unit)));
    }
    setMinimumSize(max_width, 0);
    setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    setStyleSheet(QString("QLabel { color : %1 }").arg(platformStyle->SingleColor().name()));
}

/** So that it responds to button clicks */
void UnitDisplayStatusBarControl::mousePressEvent(QMouseEvent *event)
{
    onDisplayUnitsClicked(event->pos());
}

/** Creates context menu, its actions, and wires up all the relevant signals for mouse events. */
void UnitDisplayStatusBarControl::createContextMenu()
{
    menu = new QMenu(this);
    for (PAIcoinUnits::Unit u : PAIcoinUnits::availableUnits())
    {
        QAction *menuAction = new QAction(QString(PAIcoinUnits::name(u)), this);
        menuAction->setData(QVariant(u));
        menu->addAction(menuAction);
    }
    connect(menu,SIGNAL(triggered(QAction*)),this,SLOT(onMenuSelection(QAction*)));
}

/** Lets the control know about the Options Model (and its signals) */
void UnitDisplayStatusBarControl::setOptionsModel(OptionsModel *_optionsModel)
{
    if (_optionsModel)
    {
        this->optionsModel = _optionsModel;

        // be aware of a display unit change reported by the OptionsModel object.
        connect(_optionsModel,SIGNAL(displayUnitChanged(int)),this,SLOT(updateDisplayUnit(int)));

        // initialize the display units label with the current value in the model.
        updateDisplayUnit(_optionsModel->getDisplayUnit());
    }
}

/** When Display Units are changed on OptionsModel it will refresh the display text of the control on the status bar */
void UnitDisplayStatusBarControl::updateDisplayUnit(int newUnits)
{
    setText(PAIcoinUnits::name(newUnits));
}

/** Shows context menu with Display Unit options by the mouse coordinates */
void UnitDisplayStatusBarControl::onDisplayUnitsClicked(const QPoint& point)
{
    QPoint globalPos = mapToGlobal(point);
    menu->exec(globalPos);
}

/** Tells underlying optionsModel to update its current display unit. */
void UnitDisplayStatusBarControl::onMenuSelection(QAction* action)
{
    if (action)
    {
        optionsModel->setDisplayUnit(action->data());
    }
}
