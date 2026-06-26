// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Michael Bradeen and phzyxPhone contributors.
#pragma once

#include <QMainWindow>
#include "SipCore.h"

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

// MainWindow: the Qt front-end. Each tab exposes one family of testing knobs.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

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

private:
    QWidget *buildAccountTab();
    QWidget *buildPhoneTab();
    QWidget *buildCodecTab();
    QWidget *buildCallTab();
    QWidget *buildSdpTab();
    QWidget *buildLogTab();
    AccountSettings collectAccountSettings() const;
    void applyAccountSettings(const AccountSettings &s, int logLevel);
    // Load a profile YAML from disk. When interactive, shows message boxes on
    // failure; otherwise stays quiet (used for the startup auto-load).
    bool loadProfileFile(const QString &fn, bool interactive);
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
    bool         inCall_ = false;       // true when a call is CONFIRMED (media up)

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
