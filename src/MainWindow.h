// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Michael Bradeen and phzyxPhone contributors.
#pragma once

#include <QMainWindow>
#include <QPalette>
#include "SipCore.h"
#include "ConvStore.h"

class QLineEdit;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QSlider;
class QStackedWidget;
class QTableWidget;
class QPlainTextEdit;
class QLabel;
class QTabWidget;
class QSystemTrayIcon;
class QListWidget;
class QTextBrowser;

// MainWindow: the Qt front-end. Each tab exposes one family of testing knobs.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    enum ThemeMode { ThemeLight = 0, ThemeDark = 1, ThemeSystem = 2 };

private slots:
    // Account / transport
    void onStartStop();
    void onSaveProfile();
    void onLoadProfile();
    void onAccountModeChanged(int index);
    // Phone (dialpad) tab
    void onPhoneDial();
    void onPhoneHangup();
    void onPhoneBackspace();
    void onSpeakerLevelChanged(int percent);
    void onMicLevelChanged(int percent);
    void onSpeakerMuteToggled(bool muted);
    void onMicMuteToggled(bool muted);
    void onRingLevelChanged(int percent);
    void onRingMuteToggled(bool muted);
    void onPhoneAnswer();
    // Conversations (RTT) tab
    void onRttSend();
    void onNewConversation();
    void onAcceptTextCall();
    void onEndTextSession();
    void onRestartTextSession();
    void onConversationSelected();
    void onDeleteConversation();
    void onConvDepthChanged(int depth);
    // Codecs
    void onRefreshCodecs();
    void onApplyCodecs();
    // Calls
    void onDial();
    void onAnswer();
    void onHangup();
    void onHold();
    void onSendDtmf();
    void onRefreshStats();
    void onAutoAnswerToggled(bool on);
    // SDP
    void onApplySdpOverride();
    void onAddSubstitution();
    void onClearSdpLog();
    // Log
    void onClearLog();
    void onExportLog();

    // From SipCore
    void handleLog(const QString &line);
    void handleReg(bool active, int code, const QString &reason);
    void handleCallState(const QString &state, const QString &remote,
                         const QString &reason);
    void handleMedia(const QString &info);
    void handleSdp(const QString &label, const QString &sdp);
    void handleIncoming(const QString &remote);
    void handleError(const QString &msg);
    void handleRttReceived(const QString &peer, const QString &text);
    void handleRttSent(const QString &peer, const QString &text);
    void handleIncomingTextCall(const QString &peer);
    void handleTextCallState(const QString &state, const QString &peer,
                             const QString &reason);

private:
    QWidget *buildAccountTab();
    QWidget *buildPhoneTab();
    QWidget *buildCodecTab();
    QWidget *buildCallTab();
    QWidget *buildSdpTab();
    QWidget *buildLogTab();
    QWidget *buildConversationsTab();
    AccountSettings collectAccountSettings() const;
    void applyAccountSettings(const AccountSettings &s, int logLevel);
    // Load a profile YAML from disk. When interactive, shows message boxes on
    // failure; otherwise stays quiet (used for the startup auto-load).
    bool loadProfileFile(const QString &fn, bool interactive);

    // Conversations / RTT persistence.
    QString defaultConvDbPath() const;       // within-install default location
    void    openConversationDb(const QString &path);  // (re)open + refresh tab
    void    refreshConvList();               // rebuild the left-hand peer list
    void    showConversation(const QString &peerKey);  // render transcript
    void    recordRtt(const QString &peer, const QString &dir, const QString &text);

    // Appearance / theming.
    QWidget *buildApplicationTab();
    void applyTheme(int mode);          // ThemeLight | ThemeDark | ThemeSystem
    bool systemPrefersDark() const;
    // Simple/Advanced account view: build the reduced "Simple" page, and keep
    // the two views consistent by deriving one set of fields from the other.
    QWidget *buildSimpleAccountPage();
    QWidget *buildAdvancedAccountPage();
    void syncSimpleToAdvanced();   // simple fields -> canonical advanced fields
    void syncAdvancedToSimple();   // advanced fields -> simple fields
    // Build a dial-able SIP URI from a user/extension, filling in host:port
    // from the Registrar URI (falling back to the account ID URI domain).
    QString buildDialUri(const QString &userPart) const;
    // A single dialpad key press: append to the number field when idle, or
    // send the digit as DTMF when a call is connected.
    void onPhoneKey(const QString &key);

    SipCore *core_;
    QTabWidget *tabs_;

    // Account tab - mode switch (Simple vs Advanced)
    QComboBox      *accountModeCombo_;
    QStackedWidget *accountStack_;
    // Simple-mode widgets (a reduced subset; values are mirrored into the
    // advanced widgets below, which remain the canonical source for
    // collectAccountSettings()).
    QLineEdit *simpleAccountEdit_, *simpleLoginEdit_, *simplePassEdit_,
              *simpleRealmEdit_, *simpleRemoteHostEdit_,
              *simplePaiEdit_, *simpleRpidEdit_;
    QComboBox *simpleTransportCombo_;
    QSpinBox  *simpleLocalPortSpin_, *simpleRemotePortSpin_;

    // Account tab - advanced (canonical) widgets
    QLineEdit *idUriEdit_, *registrarEdit_, *userEdit_, *passEdit_, *realmEdit_;
    QLineEdit *paiEdit_, *rpidEdit_;   // caller ID identity headers
    QComboBox *transportCombo_;
    QSpinBox  *localPortSpin_;
    QLineEdit *boundAddrEdit_;
    QCheckBox *iceCheck_, *turnCheck_, *sdpRewriteCheck_;
    QLineEdit *stunEdit_, *turnServerEdit_, *turnUserEdit_, *turnPassEdit_;
    QComboBox *srtpCombo_, *srtpSignalCombo_;
    QComboBox *logLevelCombo_;
    QPushButton *startBtn_;
    QLabel *regStatusLabel_;

    // Phone (dialpad) tab
    QLineEdit   *phoneNumberEdit_;
    QPushButton *phoneCallBtn_, *phoneAnswerBtn_, *phoneHangupBtn_;
    QComboBox   *phoneDtmfMethodCombo_;
    QLabel      *phoneStatusLabel_;
    QSlider     *speakerSlider_, *micSlider_, *ringSlider_;
    QLabel      *speakerValLabel_, *micValLabel_, *ringValLabel_;
    QPushButton *speakerMuteBtn_, *micMuteBtn_, *ringMuteBtn_;
    QSystemTrayIcon *trayIcon_ = nullptr;   // incoming-call notifications
    bool         incomingPending_ = false;  // a call is ringing, awaiting answer

    // Theme: remembered across runs; defaults captured at construction so the
    // light/native look can be restored exactly.
    int      themeMode_ = ThemeSystem;
    QPalette defaultPalette_;
    QString  defaultStyleName_;
    QComboBox *themeCombo_ = nullptr;
    bool         inCall_ = false;       // true when a call is CONFIRMED (media up)

    // Conversations (RTT) tab
    ConvStore   *convStore_ = nullptr;
    QListWidget *convList_ = nullptr;
    QTextBrowser*convView_ = nullptr;
    QLineEdit   *convInput_ = nullptr;
    QPushButton *convSendBtn_ = nullptr, *convDeleteBtn_ = nullptr,
                *convNewBtn_ = nullptr, *convAcceptBtn_ = nullptr,
                *convEndBtn_ = nullptr, *convRestartBtn_ = nullptr;
    QLabel      *convDbLabel_ = nullptr;
    QLabel      *convBannerLabel_ = nullptr;  // "incoming text session" banner
    QString      convCurrentPeer_;      // selected conversation key
    QString      convActivePeer_;       // peer of the active text session (if any)
    QString      convPendingPeer_;      // peer of a pending incoming text session
    QString      convDbPath_;           // configured DB path (from profile)
    QSpinBox    *convDepthSpin_ = nullptr;  // on the Application tab

    // Codec tab
    QTableWidget *codecTable_;

    // Call tab
    QLineEdit *destUriEdit_;
    QLineEdit *hdrNameEdit_, *hdrValueEdit_;
    QPushButton *dialBtn_, *answerBtn_, *hangupBtn_, *holdBtn_;
    QCheckBox *autoAnswerCheck_;
    QSpinBox  *autoAnswerCodeSpin_;
    QLabel    *callStateLabel_;
    QLineEdit *dtmfEdit_;
    QComboBox *dtmfMethodCombo_;
    QSpinBox  *dtmfDurSpin_;
    QPlainTextEdit *statsView_;

    // SDP tab
    QCheckBox *sdpEnableCheck_, *sdpReplaceWholeCheck_;
    QPlainTextEdit *sdpWholeEdit_;
    QTableWidget *sdpSubTable_;
    QPlainTextEdit *sdpAppendEdit_;
    QPlainTextEdit *sdpLogView_;

    // Log tab
    QPlainTextEdit *logView_;
};
