// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Michael Bradeen and phzyxPhone contributors.
#include "MainWindow.h"

#include <QtWidgets>

// Column indices for the codec matrix table.
enum CodecCol {
    COL_ENABLED = 0, COL_CODEC, COL_CLOCK, COL_CH,
    COL_PRIO, COL_VAD, COL_PLC, COL_FPP, COL_BPS, COL_COUNT
};

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    core_ = new SipCore(this);

    connect(core_, &SipCore::logMessage,         this, &MainWindow::handleLog);
    connect(core_, &SipCore::registrationChanged,this, &MainWindow::handleReg);
    connect(core_, &SipCore::callStateChanged,   this, &MainWindow::handleCallState);
    connect(core_, &SipCore::mediaStateChanged,  this, &MainWindow::handleMedia);
    connect(core_, &SipCore::sdpCaptured,        this, &MainWindow::handleSdp);
    connect(core_, &SipCore::incomingCall,       this, &MainWindow::handleIncoming);
    connect(core_, &SipCore::errorOccurred,      this, &MainWindow::handleError);

    tabs_ = new QTabWidget(this);
    tabs_->addTab(buildAccountTab(), "Account / Transport");
    tabs_->addTab(buildPhoneTab(),   "Phone");
    tabs_->addTab(buildCodecTab(),   "Codecs");
    tabs_->addTab(buildCallTab(),    "Call");
    tabs_->addTab(buildSdpTab(),     "SDP");
    tabs_->addTab(buildLogTab(),     "Log");
    setCentralWidget(tabs_);

    setWindowTitle("phzyxPhone - PJSIP test softphone");
    resize(900, 680);
    statusBar()->showMessage("Idle. Configure an account and press Start.");
}

MainWindow::~MainWindow() = default;

// ===========================================================================
// Account / Transport tab
// ===========================================================================
QWidget *MainWindow::buildAccountTab() {
    auto *w = new QWidget;
    auto *outer = new QVBoxLayout(w);

    // Mode selector ----------------------------------------------------------
    accountModeCombo_ = new QComboBox;
    accountModeCombo_->addItems({"Simple", "Advanced"});
    auto *modeRow = new QHBoxLayout;
    modeRow->addWidget(new QLabel("<b>Mode</b>"));
    modeRow->addWidget(accountModeCombo_);
    modeRow->addStretch();
    outer->addLayout(modeRow);

    // Profile save/load (YAML) - shared across both modes --------------------
    auto *saveProfileBtn = new QPushButton("Save profile...");
    auto *loadProfileBtn = new QPushButton("Load profile...");
    connect(saveProfileBtn, &QPushButton::clicked, this, &MainWindow::onSaveProfile);
    connect(loadProfileBtn, &QPushButton::clicked, this, &MainWindow::onLoadProfile);
    auto *profileRow = new QHBoxLayout;
    profileRow->addWidget(new QLabel("Profile:"));
    profileRow->addWidget(saveProfileBtn);
    profileRow->addWidget(loadProfileBtn);
    profileRow->addStretch();
    outer->addLayout(profileRow);

    // Stacked Simple / Advanced pages ----------------------------------------
    accountStack_ = new QStackedWidget;
    accountStack_->addWidget(buildSimpleAccountPage());    // index 0
    accountStack_->addWidget(buildAdvancedAccountPage());  // index 1
    auto *scroll = new QScrollArea;
    scroll->setWidget(accountStack_);
    scroll->setWidgetResizable(true);
    outer->addWidget(scroll, 1);

    // Start / status - shared across both modes ------------------------------
    startBtn_ = new QPushButton("Start / Register");
    connect(startBtn_, &QPushButton::clicked, this, &MainWindow::onStartStop);
    regStatusLabel_ = new QLabel("Stopped");
    auto *startRow = new QHBoxLayout;
    startRow->addWidget(startBtn_);
    startRow->addWidget(regStatusLabel_, 1);
    outer->addLayout(startRow);

    // Default to Simple, seeded from the advanced defaults.
    syncAdvancedToSimple();
    accountStack_->setCurrentIndex(0);
    connect(accountModeCombo_,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onAccountModeChanged);

    return w;
}

// The Simple page: the handful of settings most users need.
QWidget *MainWindow::buildSimpleAccountPage() {
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);

    simpleAccountEdit_ = new QLineEdit;
    simpleAccountEdit_->setPlaceholderText("e.g. 1001 (the user part of your SIP address)");
    simpleLoginEdit_   = new QLineEdit;
    simplePassEdit_    = new QLineEdit;
    simplePassEdit_->setEchoMode(QLineEdit::Password);
    simpleRealmEdit_   = new QLineEdit("*");

    simpleTransportCombo_ = new QComboBox;
    simpleTransportCombo_->addItems({"UDP", "TCP", "TLS"});
    simpleLocalPortSpin_  = new QSpinBox;
    simpleLocalPortSpin_->setRange(0, 65535);
    simpleLocalPortSpin_->setValue(5060);

    simpleRemoteHostEdit_ = new QLineEdit;
    simpleRemoteHostEdit_->setPlaceholderText("server host, e.g. sip.example.com");
    simpleRemotePortSpin_ = new QSpinBox;
    simpleRemotePortSpin_->setRange(1, 65535);
    simpleRemotePortSpin_->setValue(5060);

    form->addRow(new QLabel("<b>Account</b>"));
    form->addRow("Account", simpleAccountEdit_);
    form->addRow("Login", simpleLoginEdit_);
    form->addRow("Password", simplePassEdit_);
    form->addRow("Realm", simpleRealmEdit_);
    form->addRow(new QLabel("<b>Transport</b>"));
    form->addRow("Transport", simpleTransportCombo_);
    form->addRow("Local port", simpleLocalPortSpin_);
    form->addRow(new QLabel("<b>Server</b>"));
    form->addRow("Remote host", simpleRemoteHostEdit_);
    form->addRow("Remote port", simpleRemotePortSpin_);

    return page;
}

// The Advanced page: the full set of knobs (the original Account form).
QWidget *MainWindow::buildAdvancedAccountPage() {
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);

    idUriEdit_     = new QLineEdit("sip:1001@192.168.1.10");
    registrarEdit_ = new QLineEdit("sip:192.168.1.10");
    userEdit_      = new QLineEdit("1001");
    passEdit_      = new QLineEdit;
    passEdit_->setEchoMode(QLineEdit::Password);
    realmEdit_     = new QLineEdit("*");

    transportCombo_ = new QComboBox;
    transportCombo_->addItems({"UDP", "TCP", "TLS"});
    localPortSpin_  = new QSpinBox;
    localPortSpin_->setRange(0, 65535);
    localPortSpin_->setValue(5060);
    boundAddrEdit_  = new QLineEdit;
    boundAddrEdit_->setPlaceholderText("optional local bind / advertised addr");

    stunEdit_       = new QLineEdit;
    stunEdit_->setPlaceholderText("stun.example.com:3478 (optional)");
    iceCheck_       = new QCheckBox("Enable ICE");
    sdpRewriteCheck_= new QCheckBox("SDP NAT rewrite (use received origin)");
    turnCheck_      = new QCheckBox("Enable TURN");
    turnServerEdit_ = new QLineEdit;
    turnServerEdit_->setPlaceholderText("turn.example.com:3478");
    turnUserEdit_   = new QLineEdit;
    turnPassEdit_   = new QLineEdit;
    turnPassEdit_->setEchoMode(QLineEdit::Password);

    srtpCombo_ = new QComboBox;
    srtpCombo_->addItems({"Disabled", "Optional", "Mandatory"});
    srtpSignalCombo_ = new QComboBox;
    srtpSignalCombo_->addItems({"None", "Require TLS", "Require SIPS e2e"});

    logLevelCombo_ = new QComboBox;
    for (int i = 0; i <= 6; ++i) logLevelCombo_->addItem(QString::number(i));
    logLevelCombo_->setCurrentIndex(4);

    form->addRow(new QLabel("<b>Account</b>"));
    form->addRow("Account ID URI", idUriEdit_);
    form->addRow("Registrar URI", registrarEdit_);
    form->addRow("Auth username", userEdit_);
    form->addRow("Auth password", passEdit_);
    form->addRow("Auth realm", realmEdit_);
    form->addRow(new QLabel("<b>Transport</b>"));
    form->addRow("Transport", transportCombo_);
    form->addRow("Local port", localPortSpin_);
    form->addRow("Bound address", boundAddrEdit_);
    form->addRow(new QLabel("<b>NAT</b>"));
    form->addRow("STUN server", stunEdit_);
    form->addRow("", iceCheck_);
    form->addRow("", sdpRewriteCheck_);
    form->addRow("", turnCheck_);
    form->addRow("TURN server", turnServerEdit_);
    form->addRow("TURN user", turnUserEdit_);
    form->addRow("TURN password", turnPassEdit_);
    form->addRow(new QLabel("<b>Security & logging</b>"));
    form->addRow("SRTP", srtpCombo_);
    form->addRow("SRTP signalling", srtpSignalCombo_);
    form->addRow("Log level (0-6)", logLevelCombo_);

    return page;
}

// ===========================================================================
// Phone (dialpad) tab - a traditional phone face
// ===========================================================================
QWidget *MainWindow::buildPhoneTab() {
    auto *w = new QWidget;
    auto *outer = new QVBoxLayout(w);
    outer->setAlignment(Qt::AlignTop);

    auto *hint = new QLabel(
        "Enter an extension or number, then press Call. The host and port are "
        "filled in automatically from the Registrar URI on the Account / "
        "Transport tab. During a connected call, the dialpad sends DTMF tones.");
    hint->setWordWrap(true);
    outer->addWidget(hint);

    // Number display + backspace ---------------------------------------------
    auto *dispRow = new QHBoxLayout;
    phoneNumberEdit_ = new QLineEdit;
    phoneNumberEdit_->setPlaceholderText("extension or number, e.g. 1002");
    QFont big = phoneNumberEdit_->font();
    big.setPointSize(big.pointSize() + 8);
    phoneNumberEdit_->setFont(big);
    phoneNumberEdit_->setAlignment(Qt::AlignCenter);
    phoneNumberEdit_->setClearButtonEnabled(true);
    auto *backBtn = new QPushButton("\u232B");   // erase-left glyph
    backBtn->setFixedWidth(48);
    connect(backBtn, &QPushButton::clicked, this, &MainWindow::onPhoneBackspace);
    dispRow->addWidget(phoneNumberEdit_);
    dispRow->addWidget(backBtn);
    outer->addLayout(dispRow);

    // Dialpad -----------------------------------------------------------------
    auto *padW = new QWidget;
    auto *pad = new QGridLayout(padW);
    pad->setSpacing(6);
    struct Key { const char *digit; const char *sub; };
    static const Key keys[12] = {
        {"1", ""},    {"2", "ABC"}, {"3", "DEF"},
        {"4", "GHI"}, {"5", "JKL"}, {"6", "MNO"},
        {"7", "PQRS"},{"8", "TUV"}, {"9", "WXYZ"},
        {"*", ""},    {"0", "+"},   {"#", ""},
    };
    for (int i = 0; i < 12; ++i) {
        const QString d = keys[i].digit;
        const QString sub = keys[i].sub;
        auto *b = new QPushButton;
        b->setText(sub.isEmpty() ? d : QString("%1\n%2").arg(d, sub));
        b->setMinimumSize(72, 56);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QFont bf = b->font(); bf.setPointSize(bf.pointSize() + 4); b->setFont(bf);
        connect(b, &QPushButton::clicked, this, [this, d]() { onPhoneKey(d); });
        pad->addWidget(b, i / 3, i % 3);
    }
    outer->addWidget(padW);

    // Call / hang up ----------------------------------------------------------
    auto *actRow = new QHBoxLayout;
    phoneCallBtn_   = new QPushButton("Call");
    phoneHangupBtn_ = new QPushButton("Hang up");
    phoneCallBtn_->setMinimumHeight(44);
    phoneHangupBtn_->setMinimumHeight(44);
    phoneCallBtn_->setStyleSheet(
        "QPushButton{background:#2e7d32;color:white;font-weight:bold;border-radius:6px;}"
        "QPushButton:disabled{background:#9e9e9e;}");
    phoneHangupBtn_->setStyleSheet(
        "QPushButton{background:#c62828;color:white;font-weight:bold;border-radius:6px;}"
        "QPushButton:disabled{background:#9e9e9e;}");
    phoneHangupBtn_->setEnabled(false);
    connect(phoneCallBtn_,   &QPushButton::clicked, this, &MainWindow::onPhoneDial);
    connect(phoneHangupBtn_, &QPushButton::clicked, this, &MainWindow::onPhoneHangup);
    connect(phoneNumberEdit_, &QLineEdit::returnPressed, this, &MainWindow::onPhoneDial);
    actRow->addWidget(phoneCallBtn_);
    actRow->addWidget(phoneHangupBtn_);
    outer->addLayout(actRow);

    // Volume + microphone levels ---------------------------------------------
    auto *levelBox = new QGroupBox("Audio levels");
    auto *levelGrid = new QGridLayout(levelBox);

    speakerSlider_ = new QSlider(Qt::Horizontal);
    speakerSlider_->setRange(0, 200);        // 0% = mute .. 200% = +6 dB
    speakerSlider_->setValue(100);
    speakerSlider_->setToolTip("Speaker / earpiece volume");
    speakerValLabel_ = new QLabel("100%");
    speakerValLabel_->setMinimumWidth(44);
    speakerValLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    connect(speakerSlider_, &QSlider::valueChanged,
            this, &MainWindow::onSpeakerLevelChanged);
    speakerMuteBtn_ = new QPushButton("Mute");
    speakerMuteBtn_->setCheckable(true);
    speakerMuteBtn_->setFixedWidth(76);
    connect(speakerMuteBtn_, &QPushButton::toggled,
            this, &MainWindow::onSpeakerMuteToggled);
    levelGrid->addWidget(new QLabel("\U0001F50A Volume"), 0, 0);
    levelGrid->addWidget(speakerSlider_,   0, 1);
    levelGrid->addWidget(speakerValLabel_, 0, 2);
    levelGrid->addWidget(speakerMuteBtn_,  0, 3);

    micSlider_ = new QSlider(Qt::Horizontal);
    micSlider_->setRange(0, 200);
    micSlider_->setValue(100);
    micSlider_->setToolTip("Microphone gain");
    micValLabel_ = new QLabel("100%");
    micValLabel_->setMinimumWidth(44);
    micValLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    connect(micSlider_, &QSlider::valueChanged,
            this, &MainWindow::onMicLevelChanged);
    micMuteBtn_ = new QPushButton("Mute");
    micMuteBtn_->setCheckable(true);
    micMuteBtn_->setFixedWidth(76);
    connect(micMuteBtn_, &QPushButton::toggled,
            this, &MainWindow::onMicMuteToggled);
    levelGrid->addWidget(new QLabel("\U0001F3A4 Mic"), 1, 0);
    levelGrid->addWidget(micSlider_,   1, 1);
    levelGrid->addWidget(micValLabel_, 1, 2);
    levelGrid->addWidget(micMuteBtn_,  1, 3);
    levelGrid->setColumnStretch(1, 1);
    outer->addWidget(levelBox);

    // In-call DTMF method + status -------------------------------------------
    auto *dtmfRow = new QHBoxLayout;
    dtmfRow->addWidget(new QLabel("In-call DTMF:"));
    phoneDtmfMethodCombo_ = new QComboBox;
    phoneDtmfMethodCombo_->addItems({"RFC2833", "SIP INFO"});
    dtmfRow->addWidget(phoneDtmfMethodCombo_);
    dtmfRow->addStretch();
    outer->addLayout(dtmfRow);

    phoneStatusLabel_ = new QLabel("Idle.");
    phoneStatusLabel_->setStyleSheet("font-weight:bold;");
    outer->addWidget(phoneStatusLabel_);

    return w;
}

// ===========================================================================
// Codec tab
// ===========================================================================
QWidget *MainWindow::buildCodecTab() {
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);

    auto *info = new QLabel(
        "Enable/disable each codec, set priority (0-255, higher = preferred), "
        "VAD, PLC, frames-per-packet (ptime) and average bitrate. "
        "Apply pushes changes to the PJSIP endpoint - do it before placing a call.");
    info->setWordWrap(true);
    v->addWidget(info);

    codecTable_ = new QTableWidget(0, COL_COUNT, w);
    codecTable_->setHorizontalHeaderLabels(
        {"On", "Codec", "Clock", "Ch", "Prio", "VAD", "PLC", "Frm/Pkt", "AvgBps"});
    codecTable_->horizontalHeader()->setStretchLastSection(true);
    codecTable_->setEditTriggers(QAbstractItemView::AllEditTriggers);
    v->addWidget(codecTable_);

    auto *h = new QHBoxLayout;
    auto *refresh = new QPushButton("Refresh from endpoint");
    auto *apply   = new QPushButton("Apply all");
    connect(refresh, &QPushButton::clicked, this, &MainWindow::onRefreshCodecs);
    connect(apply,   &QPushButton::clicked, this, &MainWindow::onApplyCodecs);
    h->addWidget(refresh);
    h->addWidget(apply);
    h->addStretch();
    v->addLayout(h);
    return w;
}

// ===========================================================================
// Call tab
// ===========================================================================
QWidget *MainWindow::buildCallTab() {
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);

    auto *dialForm = new QFormLayout;
    destUriEdit_ = new QLineEdit("sip:1002@192.168.1.10");
    dialForm->addRow("Destination URI", destUriEdit_);
    hdrNameEdit_  = new QLineEdit;
    hdrNameEdit_->setPlaceholderText("X-Test-Header");
    hdrValueEdit_ = new QLineEdit;
    hdrValueEdit_->setPlaceholderText("value");
    auto *hdrRow = new QHBoxLayout;
    hdrRow->addWidget(hdrNameEdit_);
    hdrRow->addWidget(hdrValueEdit_);
    auto *hdrW = new QWidget; hdrW->setLayout(hdrRow);
    dialForm->addRow("Custom INVITE header", hdrW);
    v->addLayout(dialForm);

    auto *btnRow = new QHBoxLayout;
    dialBtn_   = new QPushButton("Dial");
    answerBtn_ = new QPushButton("Answer");
    hangupBtn_ = new QPushButton("Hang up");
    holdBtn_   = new QPushButton("Hold/Unhold");
    connect(dialBtn_,   &QPushButton::clicked, this, &MainWindow::onDial);
    connect(answerBtn_, &QPushButton::clicked, this, &MainWindow::onAnswer);
    connect(hangupBtn_, &QPushButton::clicked, this, &MainWindow::onHangup);
    connect(holdBtn_,   &QPushButton::clicked, this, &MainWindow::onHold);
    btnRow->addWidget(dialBtn_);
    btnRow->addWidget(answerBtn_);
    btnRow->addWidget(hangupBtn_);
    btnRow->addWidget(holdBtn_);
    v->addLayout(btnRow);

    auto *autoRow = new QHBoxLayout;
    autoAnswerCheck_ = new QCheckBox("Auto-answer incoming with code");
    autoAnswerCodeSpin_ = new QSpinBox;
    autoAnswerCodeSpin_->setRange(100, 699);
    autoAnswerCodeSpin_->setValue(200);
    connect(autoAnswerCheck_, &QCheckBox::toggled,
            this, &MainWindow::onAutoAnswerToggled);
    autoRow->addWidget(autoAnswerCheck_);
    autoRow->addWidget(autoAnswerCodeSpin_);
    autoRow->addStretch();
    v->addLayout(autoRow);

    callStateLabel_ = new QLabel("Call state: idle");
    callStateLabel_->setStyleSheet("font-weight:bold;");
    v->addWidget(callStateLabel_);

    // DTMF
    auto *dtmfBox = new QGroupBox("DTMF");
    auto *dtmfRow = new QHBoxLayout(dtmfBox);
    dtmfEdit_ = new QLineEdit;
    dtmfEdit_->setPlaceholderText("digits e.g. 1234#");
    dtmfMethodCombo_ = new QComboBox;
    dtmfMethodCombo_->addItems({"RFC2833", "SIP INFO"});
    dtmfDurSpin_ = new QSpinBox;
    dtmfDurSpin_->setRange(0, 5000);
    dtmfDurSpin_->setValue(160);
    dtmfDurSpin_->setSuffix(" ms");
    auto *dtmfSend = new QPushButton("Send DTMF");
    connect(dtmfSend, &QPushButton::clicked, this, &MainWindow::onSendDtmf);
    dtmfRow->addWidget(dtmfEdit_);
    dtmfRow->addWidget(dtmfMethodCombo_);
    dtmfRow->addWidget(dtmfDurSpin_);
    dtmfRow->addWidget(dtmfSend);
    v->addWidget(dtmfBox);

    // Stats
    auto *statsBox = new QGroupBox("RTP / RTCP stream stats");
    auto *statsV = new QVBoxLayout(statsBox);
    statsView_ = new QPlainTextEdit;
    statsView_->setReadOnly(true);
    statsView_->setFont(QFont("monospace"));
    auto *statsBtn = new QPushButton("Refresh stats");
    connect(statsBtn, &QPushButton::clicked, this, &MainWindow::onRefreshStats);
    statsV->addWidget(statsView_);
    statsV->addWidget(statsBtn);
    v->addWidget(statsBox);

    return w;
}

// ===========================================================================
// SDP tab
// ===========================================================================
QWidget *MainWindow::buildSdpTab() {
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);

    auto *ctlBox = new QGroupBox("Outgoing SDP override");
    auto *cv = new QVBoxLayout(ctlBox);
    sdpEnableCheck_ = new QCheckBox("Enable override (applied to locally-created SDP)");
    sdpReplaceWholeCheck_ = new QCheckBox("Replace the entire SDP with the text below");
    cv->addWidget(sdpEnableCheck_);
    cv->addWidget(sdpReplaceWholeCheck_);

    cv->addWidget(new QLabel("Whole-SDP replacement:"));
    sdpWholeEdit_ = new QPlainTextEdit;
    sdpWholeEdit_->setPlaceholderText(
        "v=0\r\no=- 0 0 IN IP4 ...\r\n... (used only when 'Replace entire SDP' is checked)");
    sdpWholeEdit_->setFont(QFont("monospace"));
    sdpWholeEdit_->setMaximumHeight(120);
    cv->addWidget(sdpWholeEdit_);

    cv->addWidget(new QLabel("Regex substitutions (find -> replace), applied in order:"));
    sdpSubTable_ = new QTableWidget(0, 2, w);
    sdpSubTable_->setHorizontalHeaderLabels({"Find (regex)", "Replace"});
    sdpSubTable_->horizontalHeader()->setStretchLastSection(true);
    sdpSubTable_->setMaximumHeight(140);
    cv->addWidget(sdpSubTable_);
    auto *addSubBtn = new QPushButton("Add substitution row");
    connect(addSubBtn, &QPushButton::clicked, this, &MainWindow::onAddSubstitution);
    cv->addWidget(addSubBtn);

    cv->addWidget(new QLabel("Extra lines to append (one per line, e.g. a=ptime:30):"));
    sdpAppendEdit_ = new QPlainTextEdit;
    sdpAppendEdit_->setMaximumHeight(80);
    sdpAppendEdit_->setFont(QFont("monospace"));
    cv->addWidget(sdpAppendEdit_);

    auto *applyBtn = new QPushButton("Apply SDP override settings");
    connect(applyBtn, &QPushButton::clicked, this, &MainWindow::onApplySdpOverride);
    cv->addWidget(applyBtn);
    v->addWidget(ctlBox);

    auto *logBox = new QGroupBox("Captured SDP (offers / answers, live)");
    auto *lv = new QVBoxLayout(logBox);
    sdpLogView_ = new QPlainTextEdit;
    sdpLogView_->setReadOnly(true);
    sdpLogView_->setFont(QFont("monospace"));
    auto *clrBtn = new QPushButton("Clear captured SDP");
    connect(clrBtn, &QPushButton::clicked, this, &MainWindow::onClearSdpLog);
    lv->addWidget(sdpLogView_);
    lv->addWidget(clrBtn);
    v->addWidget(logBox);

    return w;
}

// ===========================================================================
// Log tab
// ===========================================================================
QWidget *MainWindow::buildLogTab() {
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    logView_ = new QPlainTextEdit;
    logView_->setReadOnly(true);
    logView_->setFont(QFont("monospace"));
    logView_->setMaximumBlockCount(20000);
    v->addWidget(logView_);
    auto *h = new QHBoxLayout;
    auto *clr = new QPushButton("Clear");
    auto *exp = new QPushButton("Export...");
    connect(clr, &QPushButton::clicked, this, &MainWindow::onClearLog);
    connect(exp, &QPushButton::clicked, this, &MainWindow::onExportLog);
    h->addWidget(clr);
    h->addWidget(exp);
    h->addStretch();
    v->addLayout(h);
    return w;
}

// ===========================================================================
// Settings collection
// ===========================================================================
AccountSettings MainWindow::collectAccountSettings() const {
    AccountSettings s;
    s.idUri        = idUriEdit_->text();
    s.registrar    = registrarEdit_->text();
    s.username     = userEdit_->text();
    s.password     = passEdit_->text();
    s.realm        = realmEdit_->text().isEmpty() ? "*" : realmEdit_->text();
    s.transport    = transportCombo_->currentText();
    s.localPort    = localPortSpin_->value();
    s.boundAddress = boundAddrEdit_->text();
    s.iceEnabled   = iceCheck_->isChecked();
    s.stunServer   = stunEdit_->text();
    s.turnEnabled  = turnCheck_->isChecked();
    s.turnServer   = turnServerEdit_->text();
    s.turnUser     = turnUserEdit_->text();
    s.turnPassword = turnPassEdit_->text();
    s.sdpNatRewrite= sdpRewriteCheck_->isChecked();
    s.srtpUse      = srtpCombo_->currentIndex();
    s.srtpSignaling= srtpSignalCombo_->currentIndex();
    return s;
}

// ===========================================================================
// Slots: account
// ===========================================================================
void MainWindow::onStartStop() {
    if (core_->isRunning()) {
        core_->shutdown();
        startBtn_->setText("Start / Register");
        regStatusLabel_->setText("Stopped");
        statusBar()->showMessage("Endpoint stopped.");
        return;
    }
    // If the user is editing in Simple mode, fold those values into the
    // canonical advanced widgets before reading them back.
    if (accountModeCombo_->currentIndex() == 0) syncSimpleToAdvanced();
    AccountSettings s = collectAccountSettings();
    int logLevel = logLevelCombo_->currentText().toInt();
    QString err;
    if (!core_->bringUp(s, logLevel, err)) {
        QMessageBox::critical(this, "Start failed", err);
        core_->shutdown();
        return;
    }
    startBtn_->setText("Stop");
    regStatusLabel_->setText("Started; registering...");
    statusBar()->showMessage("Endpoint started.");
    onRefreshCodecs();
}

// ===========================================================================
// Profile persistence (flat YAML)
// ===========================================================================
namespace {

// Emit a YAML scalar, quoting + escaping when the value could otherwise be
// misread by the simple line parser below (empty, leading/trailing space, or
// containing YAML-significant characters).
QString yamlScalar(const QString &v) {
    bool needQuote = v.isEmpty() || v != v.trimmed();
    if (!needQuote) {
        static const QString specials = ":#\"'{}[],&*!|>%@`?-";
        for (const QChar &c : v) {
            if (c < ' ' || specials.contains(c)) { needQuote = true; break; }
        }
    }
    if (!needQuote) return v;
    QString e = v;
    e.replace("\\", "\\\\");
    e.replace("\"", "\\\"");
    e.replace("\n", "\\n");
    e.replace("\t", "\\t");
    return "\"" + e + "\"";
}

// Parse the value portion of a `key: value` line, handling double-quoted
// (with escapes) and bare scalars (stripping inline ` #` comments).
QString yamlUnscalar(QString v) {
    v = v.trimmed();
    if (v.startsWith('"')) {
        QString out; bool esc = false;
        for (int i = 1; i < v.size(); ++i) {
            const QChar c = v[i];
            if (esc) {
                if (c == 'n') out += '\n';
                else if (c == 't') out += '\t';
                else out += c;
                esc = false;
            } else if (c == '\\') {
                esc = true;
            } else if (c == '"') {
                break;
            } else {
                out += c;
            }
        }
        return out;
    }
    const int hash = v.indexOf(" #");
    if (hash >= 0) v = v.left(hash).trimmed();
    return v;
}

QString boolStr(bool b) { return b ? "true" : "false"; }
bool    toBool(const QString &s) { return s.compare("true", Qt::CaseInsensitive) == 0
                                          || s == "1" || s.compare("yes", Qt::CaseInsensitive) == 0; }

QString writeProfileYaml(const AccountSettings &s, int logLevel) {
    QString o;
    o += "# phzyxPhone account profile\n";
    o += "# NOTE: credentials are stored in plain text.\n";
    o += "idUri: "          + yamlScalar(s.idUri)        + "\n";
    o += "registrar: "      + yamlScalar(s.registrar)    + "\n";
    o += "username: "       + yamlScalar(s.username)     + "\n";
    o += "password: "       + yamlScalar(s.password)     + "\n";
    o += "realm: "          + yamlScalar(s.realm)        + "\n";
    o += "transport: "      + yamlScalar(s.transport)    + "\n";
    o += "localPort: "      + QString::number(s.localPort)     + "\n";
    o += "boundAddress: "   + yamlScalar(s.boundAddress) + "\n";
    o += "iceEnabled: "     + boolStr(s.iceEnabled)      + "\n";
    o += "stunServer: "     + yamlScalar(s.stunServer)   + "\n";
    o += "turnEnabled: "    + boolStr(s.turnEnabled)     + "\n";
    o += "turnServer: "     + yamlScalar(s.turnServer)   + "\n";
    o += "turnUser: "       + yamlScalar(s.turnUser)     + "\n";
    o += "turnPassword: "   + yamlScalar(s.turnPassword) + "\n";
    o += "sdpNatRewrite: "  + boolStr(s.sdpNatRewrite)   + "\n";
    o += "srtpUse: "        + QString::number(s.srtpUse)        + "\n";
    o += "srtpSignaling: "  + QString::number(s.srtpSignaling)  + "\n";
    o += "logLevel: "       + QString::number(logLevel)         + "\n";
    return o;
}

// Returns true if at least one recognised key was parsed.
bool parseProfileYaml(const QString &text, AccountSettings &s, int &logLevel) {
    int matched = 0;
    const QStringList lines = text.split('\n');
    for (const QString &raw : lines) {
        QString line = raw;
        const QString t = line.trimmed();
        if (t.isEmpty() || t.startsWith('#') || t == "---") continue;
        const int colon = line.indexOf(':');
        if (colon < 0) continue;
        const QString key = line.left(colon).trimmed();
        const QString val = yamlUnscalar(line.mid(colon + 1));
        if      (key == "idUri")         s.idUri = val;
        else if (key == "registrar")     s.registrar = val;
        else if (key == "username")      s.username = val;
        else if (key == "password")      s.password = val;
        else if (key == "realm")         s.realm = val;
        else if (key == "transport")     s.transport = val;
        else if (key == "localPort")     s.localPort = val.toInt();
        else if (key == "boundAddress")  s.boundAddress = val;
        else if (key == "iceEnabled")    s.iceEnabled = toBool(val);
        else if (key == "stunServer")    s.stunServer = val;
        else if (key == "turnEnabled")   s.turnEnabled = toBool(val);
        else if (key == "turnServer")    s.turnServer = val;
        else if (key == "turnUser")      s.turnUser = val;
        else if (key == "turnPassword")  s.turnPassword = val;
        else if (key == "sdpNatRewrite") s.sdpNatRewrite = toBool(val);
        else if (key == "srtpUse")       s.srtpUse = val.toInt();
        else if (key == "srtpSignaling") s.srtpSignaling = val.toInt();
        else if (key == "logLevel")      logLevel = val.toInt();
        else continue;
        ++matched;
    }
    return matched > 0;
}

} // namespace

// ===========================================================================
// Simple <-> Advanced account view
// ===========================================================================
namespace {
struct SipParts { QString user, host; int port = 0; };

// Best-effort parse of "sip:user@host:port;params?headers".
SipParts parseSipUri(const QString &uriIn) {
    SipParts p;
    QString u = uriIn.trimmed();
    if (u.startsWith("sips:", Qt::CaseInsensitive))      u = u.mid(5);
    else if (u.startsWith("sip:", Qt::CaseInsensitive))  u = u.mid(4);
    int cut = u.indexOf(';'); if (cut >= 0) u = u.left(cut);
    cut = u.indexOf('?');     if (cut >= 0) u = u.left(cut);
    int at = u.indexOf('@');
    if (at >= 0) { p.user = u.left(at); u = u.mid(at + 1); }
    int pc = u.lastIndexOf(':');
    if (pc >= 0) { p.host = u.left(pc); p.port = u.mid(pc + 1).toInt(); }
    else         { p.host = u; }
    return p;
}
} // namespace

void MainWindow::onAccountModeChanged(int index) {
    // Carry the user's edits across so neither view loses data.
    if (index == 0) syncAdvancedToSimple();   // -> Simple
    else            syncSimpleToAdvanced();    // -> Advanced
    accountStack_->setCurrentIndex(index);
}

// Push the reduced Simple fields into the canonical Advanced widgets.
void MainWindow::syncSimpleToAdvanced() {
    const QString acct = simpleAccountEdit_->text().trimmed();
    const QString host = simpleRemoteHostEdit_->text().trimmed();
    const int rport    = simpleRemotePortSpin_->value();

    QString hostport = host;
    if (!host.isEmpty() && rport > 0 && rport != 5060)
        hostport += ":" + QString::number(rport);

    if (host.isEmpty())
        idUriEdit_->setText(acct.isEmpty() ? QString() : "sip:" + acct);
    else
        idUriEdit_->setText("sip:" + acct + "@" + hostport);
    registrarEdit_->setText(host.isEmpty() ? QString() : "sip:" + hostport);

    userEdit_->setText(simpleLoginEdit_->text());
    passEdit_->setText(simplePassEdit_->text());
    realmEdit_->setText(simpleRealmEdit_->text());
    transportCombo_->setCurrentIndex(simpleTransportCombo_->currentIndex());
    localPortSpin_->setValue(simpleLocalPortSpin_->value());
}

// Derive the Simple fields from the canonical Advanced widgets.
void MainWindow::syncAdvancedToSimple() {
    simpleAccountEdit_->setText(parseSipUri(idUriEdit_->text()).user);
    simpleLoginEdit_->setText(userEdit_->text());
    simplePassEdit_->setText(passEdit_->text());
    simpleRealmEdit_->setText(realmEdit_->text());
    simpleTransportCombo_->setCurrentIndex(transportCombo_->currentIndex());
    simpleLocalPortSpin_->setValue(localPortSpin_->value());

    // Server = Registrar host:port, falling back to the ID URI's domain.
    SipParts r = parseSipUri(registrarEdit_->text());
    if (r.host.isEmpty()) r = parseSipUri(idUriEdit_->text());
    simpleRemoteHostEdit_->setText(r.host);
    simpleRemotePortSpin_->setValue(r.port > 0 ? r.port : 5060);
}

void MainWindow::applyAccountSettings(const AccountSettings &s, int logLevel) {
    idUriEdit_->setText(s.idUri);
    registrarEdit_->setText(s.registrar);
    userEdit_->setText(s.username);
    passEdit_->setText(s.password);
    realmEdit_->setText(s.realm);
    const int ti = transportCombo_->findText(s.transport, Qt::MatchFixedString);
    transportCombo_->setCurrentIndex(ti >= 0 ? ti : 0);
    localPortSpin_->setValue(s.localPort);
    boundAddrEdit_->setText(s.boundAddress);
    iceCheck_->setChecked(s.iceEnabled);
    stunEdit_->setText(s.stunServer);
    turnCheck_->setChecked(s.turnEnabled);
    turnServerEdit_->setText(s.turnServer);
    turnUserEdit_->setText(s.turnUser);
    turnPassEdit_->setText(s.turnPassword);
    sdpRewriteCheck_->setChecked(s.sdpNatRewrite);
    srtpCombo_->setCurrentIndex(qBound(0, s.srtpUse, srtpCombo_->count() - 1));
    srtpSignalCombo_->setCurrentIndex(qBound(0, s.srtpSignaling, srtpSignalCombo_->count() - 1));
    const int li = logLevelCombo_->findText(QString::number(logLevel));
    if (li >= 0) logLevelCombo_->setCurrentIndex(li);
    // Keep the Simple view in step with the freshly-loaded values.
    syncAdvancedToSimple();
}

void MainWindow::onSaveProfile() {
    QString fn = QFileDialog::getSaveFileName(
        this, "Save profile", "profile.yaml", "YAML (*.yaml *.yml)");
    if (fn.isEmpty()) return;
    if (!fn.endsWith(".yaml", Qt::CaseInsensitive) &&
        !fn.endsWith(".yml", Qt::CaseInsensitive))
        fn += ".yaml";
    QFile f(fn);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Save failed", "Could not write " + fn);
        return;
    }
    if (accountModeCombo_->currentIndex() == 0) syncSimpleToAdvanced();
    QTextStream(&f) << writeProfileYaml(collectAccountSettings(),
                                        logLevelCombo_->currentText().toInt());
    statusBar()->showMessage("Profile saved to " + fn, 5000);
}

void MainWindow::onLoadProfile() {
    const QString fn = QFileDialog::getOpenFileName(
        this, "Load profile", QString(), "YAML (*.yaml *.yml);;All files (*)");
    if (fn.isEmpty()) return;
    QFile f(fn);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Load failed", "Could not read " + fn);
        return;
    }
    const QString text = QTextStream(&f).readAll();
    AccountSettings s;      // defaults
    int logLevel = logLevelCombo_->currentText().toInt();
    if (!parseProfileYaml(text, s, logLevel)) {
        QMessageBox::warning(this, "Load failed",
                             "No recognised settings found in " + fn);
        return;
    }
    applyAccountSettings(s, logLevel);
    statusBar()->showMessage("Profile loaded from " + fn, 5000);
}

// ===========================================================================
// Slots: phone (dialpad) tab
// ===========================================================================
QString MainWindow::buildDialUri(const QString &userIn) const {
    QString user = userIn.trimmed();
    if (user.isEmpty()) return QString();
    // Already a full URI, or carries its own host -> use verbatim.
    if (user.startsWith("sip:", Qt::CaseInsensitive) ||
        user.startsWith("sips:", Qt::CaseInsensitive) ||
        user.contains('@'))
        return user;

    // Derive scheme + host[:port] from the Registrar URI.
    QString scheme = "sip", hostport;
    QString reg = registrarEdit_->text().trimmed();
    if (!reg.isEmpty()) {
        if (reg.startsWith("sips:", Qt::CaseInsensitive)) { scheme = "sips"; reg = reg.mid(5); }
        else if (reg.startsWith("sip:", Qt::CaseInsensitive)) { scheme = "sip"; reg = reg.mid(4); }
        const int at = reg.indexOf('@');   if (at   >= 0) reg = reg.mid(at + 1);
        const int sc = reg.indexOf(';');   if (sc   >= 0) reg = reg.left(sc);
        const int qm = reg.indexOf('?');   if (qm   >= 0) reg = reg.left(qm);
        hostport = reg.trimmed();
    }
    // Fall back to the domain of the account ID URI.
    if (hostport.isEmpty()) {
        QString id = idUriEdit_->text().trimmed();
        const int at = id.indexOf('@');
        if (at >= 0) {
            QString h = id.mid(at + 1);
            const int sc = h.indexOf(';'); if (sc >= 0) h = h.left(sc);
            hostport = h.trimmed();
        }
    }
    if (hostport.isEmpty()) return QString();
    return QString("%1:%2@%3").arg(scheme, user, hostport);
}

void MainWindow::onPhoneKey(const QString &key) {
    if (inCall_) {
        // Connected call: dialpad keys are DTMF tones.
        core_->sendDtmf(key, phoneDtmfMethodCombo_->currentIndex(), 160);
        phoneStatusLabel_->setText("DTMF sent: " + key);
    } else {
        phoneNumberEdit_->setText(phoneNumberEdit_->text() + key);
    }
}

void MainWindow::onPhoneBackspace() {
    QString t = phoneNumberEdit_->text();
    if (!t.isEmpty()) phoneNumberEdit_->setText(t.left(t.size() - 1));
}

void MainWindow::onPhoneDial() {
    if (!core_->isRunning()) {
        QMessageBox::information(this, "Not running",
                                 "Start the endpoint on the Account / Transport tab first.");
        return;
    }
    const QString uri = buildDialUri(phoneNumberEdit_->text());
    if (uri.isEmpty()) {
        QMessageBox::warning(this, "Cannot dial",
                             "Enter a number, and set a Registrar URI (or an "
                             "account ID URI with a domain) so the host can be "
                             "filled in.");
        return;
    }
    QString err;
    if (!core_->makeCall(uri, {}, err)) {
        QMessageBox::warning(this, "Call failed", err);
        return;
    }
    phoneStatusLabel_->setText("Calling " + uri);
}

void MainWindow::onPhoneHangup() {
    core_->hangupCurrent(603);
}

void MainWindow::onSpeakerLevelChanged(int percent) {
    speakerValLabel_->setText(QString::number(percent) + "%");
    // While muted, the slider position is remembered but the applied level
    // stays at 0; unmuting restores it.
    if (!speakerMuteBtn_->isChecked())
        core_->setSpeakerLevel(percent / 100.0f);
}

void MainWindow::onMicLevelChanged(int percent) {
    micValLabel_->setText(QString::number(percent) + "%");
    if (!micMuteBtn_->isChecked())
        core_->setMicLevel(percent / 100.0f);
}

void MainWindow::onSpeakerMuteToggled(bool muted) {
    speakerMuteBtn_->setText(muted ? "Unmute" : "Mute");
    speakerSlider_->setEnabled(!muted);
    speakerValLabel_->setEnabled(!muted);
    core_->setSpeakerLevel(muted ? 0.0f : speakerSlider_->value() / 100.0f);
}

void MainWindow::onMicMuteToggled(bool muted) {
    micMuteBtn_->setText(muted ? "Unmute" : "Mute");
    micSlider_->setEnabled(!muted);
    micValLabel_->setEnabled(!muted);
    core_->setMicLevel(muted ? 0.0f : micSlider_->value() / 100.0f);
}

// ===========================================================================
// Slots: codecs
// ===========================================================================
void MainWindow::onRefreshCodecs() {
    if (!core_->isRunning()) {
        QMessageBox::information(this, "Not running",
                                 "Start the endpoint first.");
        return;
    }
    QVector<CodecRow> rows = core_->enumerateCodecs();
    codecTable_->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const CodecRow &row = rows[r];
        auto *on = new QTableWidgetItem;
        on->setCheckState(row.priority > 0 ? Qt::Checked : Qt::Unchecked);
        codecTable_->setItem(r, COL_ENABLED, on);

        auto *id = new QTableWidgetItem(row.codecId);
        id->setFlags(id->flags() & ~Qt::ItemIsEditable);
        codecTable_->setItem(r, COL_CODEC, id);
        codecTable_->setItem(r, COL_CLOCK,
                             new QTableWidgetItem(QString::number(row.clockRate)));
        codecTable_->setItem(r, COL_CH,
                             new QTableWidgetItem(QString::number(row.channels)));
        codecTable_->setItem(r, COL_PRIO,
                             new QTableWidgetItem(QString::number(row.priority)));
        auto *vad = new QTableWidgetItem;
        vad->setCheckState(row.vad ? Qt::Checked : Qt::Unchecked);
        codecTable_->setItem(r, COL_VAD, vad);
        auto *plc = new QTableWidgetItem;
        plc->setCheckState(row.plc ? Qt::Checked : Qt::Unchecked);
        codecTable_->setItem(r, COL_PLC, plc);
        codecTable_->setItem(r, COL_FPP,
                             new QTableWidgetItem(QString::number(row.framesPerPkt)));
        codecTable_->setItem(r, COL_BPS,
                             new QTableWidgetItem(QString::number(row.avgBps)));
    }
    codecTable_->resizeColumnsToContents();
}

void MainWindow::onApplyCodecs() {
    if (!core_->isRunning()) return;
    for (int r = 0; r < codecTable_->rowCount(); ++r) {
        CodecRow row;
        row.codecId = codecTable_->item(r, COL_CODEC)->text();
        bool enabled = codecTable_->item(r, COL_ENABLED)->checkState() == Qt::Checked;
        int prio = codecTable_->item(r, COL_PRIO)->text().toInt();
        row.priority = enabled ? (prio > 0 ? prio : 128) : 0;
        row.vad = codecTable_->item(r, COL_VAD)->checkState() == Qt::Checked;
        row.plc = codecTable_->item(r, COL_PLC)->checkState() == Qt::Checked;
        row.framesPerPkt = codecTable_->item(r, COL_FPP)->text().toUInt();
        row.avgBps = codecTable_->item(r, COL_BPS)->text().toUInt();
        core_->applyCodecRow(row);
    }
    statusBar()->showMessage("Codec settings applied.", 4000);
    onRefreshCodecs();
}

// ===========================================================================
// Slots: calls
// ===========================================================================
void MainWindow::onDial() {
    if (!core_->isRunning()) {
        QMessageBox::information(this, "Not running", "Start the endpoint first.");
        return;
    }
    QList<QPair<QString, QString>> hdrs;
    if (!hdrNameEdit_->text().trimmed().isEmpty())
        hdrs.append({hdrNameEdit_->text().trimmed(), hdrValueEdit_->text()});
    QString err;
    if (!core_->makeCall(destUriEdit_->text(), hdrs, err))
        QMessageBox::warning(this, "Call failed", err);
}

void MainWindow::onAnswer()  { core_->answerCurrent(200); }
void MainWindow::onHangup()  { core_->hangupCurrent(603); }
void MainWindow::onHold()    { core_->holdCurrent(); }

void MainWindow::onSendDtmf() {
    core_->sendDtmf(dtmfEdit_->text(),
                    dtmfMethodCombo_->currentIndex(),
                    (unsigned)dtmfDurSpin_->value());
}

void MainWindow::onRefreshStats() {
    statsView_->setPlainText(core_->currentStreamStats());
}

void MainWindow::onAutoAnswerToggled(bool on) {
    core_->setAutoAnswer(on, autoAnswerCodeSpin_->value());
}

// ===========================================================================
// Slots: SDP
// ===========================================================================
void MainWindow::onAddSubstitution() {
    int r = sdpSubTable_->rowCount();
    sdpSubTable_->insertRow(r);
    sdpSubTable_->setItem(r, 0, new QTableWidgetItem(""));
    sdpSubTable_->setItem(r, 1, new QTableWidgetItem(""));
}

void MainWindow::onApplySdpOverride() {
    SdpOverride o;
    o.enabled      = sdpEnableCheck_->isChecked();
    o.replaceWhole = sdpReplaceWholeCheck_->isChecked();
    o.wholeSdp     = sdpWholeEdit_->toPlainText();
    for (int r = 0; r < sdpSubTable_->rowCount(); ++r) {
        QString find = sdpSubTable_->item(r, 0) ? sdpSubTable_->item(r, 0)->text() : "";
        QString repl = sdpSubTable_->item(r, 1) ? sdpSubTable_->item(r, 1)->text() : "";
        if (!find.isEmpty()) o.substitutions.append({find, repl});
    }
    const QStringList appends =
        sdpAppendEdit_->toPlainText().split('\n', Qt::SkipEmptyParts);
    o.appendLines = appends;
    core_->setSdpOverride(o);
    statusBar()->showMessage(
        o.enabled ? "SDP override enabled." : "SDP override disabled.", 4000);
}

void MainWindow::onClearSdpLog() { sdpLogView_->clear(); }

// ===========================================================================
// Slots: log
// ===========================================================================
void MainWindow::onClearLog() { logView_->clear(); }

void MainWindow::onExportLog() {
    QString fn = QFileDialog::getSaveFileName(this, "Export log", "phzyxphone.log",
                                              "Log files (*.log *.txt)");
    if (fn.isEmpty()) return;
    QFile f(fn);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream(&f) << logView_->toPlainText();
        statusBar()->showMessage("Log exported to " + fn, 5000);
    }
}

// ===========================================================================
// Slots: from SipCore (run on GUI thread via queued connection)
// ===========================================================================
void MainWindow::handleLog(const QString &line) {
    logView_->appendPlainText(line);
}

void MainWindow::handleReg(bool active, int code, const QString &reason) {
    regStatusLabel_->setText(
        QString("Registration: %1 (%2 %3)")
            .arg(active ? "ACTIVE" : "inactive").arg(code).arg(reason));
}

void MainWindow::handleCallState(const QString &state, const QString &remote,
                                 const QString &reason) {
    callStateLabel_->setText(
        QString("Call state: %1  [%2]  %3").arg(state, remote, reason));
    statusBar()->showMessage(QString("Call: %1").arg(state), 4000);

    // Keep the Phone tab in sync: a call is "active" until DISCONNECTED, and
    // the dialpad only sends DTMF once media is up (CONFIRMED).
    const bool disconnected = state.compare("DISCONNECTED", Qt::CaseInsensitive) == 0;
    const bool active = !disconnected && !state.isEmpty();
    inCall_ = state.compare("CONFIRMED", Qt::CaseInsensitive) == 0;
    if (phoneCallBtn_)   phoneCallBtn_->setEnabled(!active);
    if (phoneHangupBtn_) phoneHangupBtn_->setEnabled(active);
    if (phoneStatusLabel_)
        phoneStatusLabel_->setText(
            disconnected ? QString("Idle. %1").arg(reason).trimmed()
                         : QString("%1  %2").arg(state, remote).trimmed());
}

void MainWindow::handleMedia(const QString &info) {
    statusBar()->showMessage("Media: " + info, 4000);
}

void MainWindow::handleSdp(const QString &label, const QString &sdp) {
    sdpLogView_->appendPlainText(
        QString("===== %1 =====\n%2\n").arg(label, sdp));
}

void MainWindow::handleIncoming(const QString &remote) {
    callStateLabel_->setText("Incoming call from " + remote);
    statusBar()->showMessage("Incoming call from " + remote);
    if (!autoAnswerCheck_->isChecked())
        QApplication::beep();
}

void MainWindow::handleError(const QString &msg) {
    QMessageBox::warning(this, "Error", msg);
}
