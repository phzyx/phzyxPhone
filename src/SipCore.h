// SPDX-License-Identifier: GPL-2.0-or-later
// phzyxPhone - a testing-focused SIP softphone built on PJSIP/pjproject + Qt.
// Copyright (C) 2026 Michael Bradeen and phzyxPhone contributors.
#pragma once

// SipCore: a thin QObject wrapper around the PJSUA2 (pjproject) C++ API.
// It owns the Endpoint, transport, account and calls, and translates
// PJSUA2 callbacks (which fire on PJSIP worker threads) into Qt signals
// that the GUI can consume safely via queued connections.

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QPair>
#include <QVector>
#include <memory>

#include <pjsua2.hpp>

// ---------------------------------------------------------------------------
// Plain settings structs filled in by the GUI
// ---------------------------------------------------------------------------

struct AccountSettings {
    QString idUri;        // sip:user@domain
    QString registrar;    // sip:domain  (empty = no registration)
    QString username;
    QString password;
    QString realm = "*";  // auth realm, * = any

    QString transport = "UDP";   // UDP | TCP | TLS
    int     localPort = 5060;    // 0 = ephemeral
    QString boundAddress;        // optional local bind / advertised address

    // NAT traversal
    bool    iceEnabled   = false;
    QString stunServer;          // host[:port], empty = off
    bool    turnEnabled  = false;
    QString turnServer;          // host:port
    QString turnUser;
    QString turnPassword;
    bool    sdpNatRewrite = false;

    // Caller ID: outgoing INVITE identity headers (free-text, RFC 3325 / RPID)
    QString pAssertedIdentity;   // P-Asserted-Identity header value, empty = off
    QString remotePartyId;       // Remote-Party-ID header value, empty = off

    // SRTP: 0=disabled 1=optional 2=mandatory
    int     srtpUse = 0;
    // SRTP secure signalling requirement: 0=none 1=tls 2=end-to-end sips
    int     srtpSignaling = 0;
};

// A single codec row as shown / edited in the GUI codec matrix.
struct CodecRow {
    QString codecId;       // e.g. "PCMU/8000/1"
    QString desc;
    unsigned clockRate = 0;
    unsigned channels  = 0;
    int      priority  = 0;     // 0 = disabled
    bool     vad       = false;
    bool     plc       = true;
    unsigned framesPerPkt = 1;  // ptime multiplier
    unsigned avgBps    = 0;     // average bitrate (informational / editable)
};

// User-defined transformation applied to locally-created SDP before it is
// sent. This is the heart of the SDP test feature.
struct SdpOverride {
    bool enabled = false;
    bool replaceWhole = false;
    QString wholeSdp;                            // used when replaceWhole
    QList<QPair<QString, QString>> substitutions; // regex find -> replace
    QStringList appendLines;                     // extra lines appended to SDP
};

// ---------------------------------------------------------------------------
// SipCore
// ---------------------------------------------------------------------------

class MyAccount;   // pj::Account subclass, defined in SipCore.cpp
class MyCall;      // pj::Call subclass, defined in SipCore.cpp
class GuiLogWriter;

class SipCore : public QObject {
    Q_OBJECT
public:
    explicit SipCore(QObject *parent = nullptr);
    ~SipCore() override;

    // Lifecycle -----------------------------------------------------------
    // Initialise the endpoint with the given log level + STUN server.
    bool startEndpoint(int logLevel, const QString &stunServer, QString &err);
    // Create a SIP transport (UDP/TCP/TLS) on the given local port.
    bool createTransport(const QString &type, int port,
                         const QString &boundAddress, QString &err);
    // Start the library (must be called after transports are created).
    bool startLibrary(QString &err);
    // Create / re-create the account and (optionally) register.
    bool addAccount(const AccountSettings &s, QString &err);
    void shutdown();
    bool isRunning() const { return started_; }

    // Convenience: full bring-up from a single settings struct.
    bool bringUp(const AccountSettings &s, int logLevel, QString &err);

    // Codec matrix --------------------------------------------------------
    QVector<CodecRow> enumerateCodecs();
    void applyCodecRow(const CodecRow &row);

    // Calls ---------------------------------------------------------------
    bool makeCall(const QString &destUri,
                  const QList<QPair<QString, QString>> &extraHeaders,
                  QString &err);
    void answerCurrent(int statusCode = 200);
    void hangupCurrent(int statusCode = 603);
    void holdCurrent();
    void reinviteCurrent();
    void sendDtmf(const QString &digits, int method, unsigned durationMs);
    QString currentStreamStats();

    // Audio levels --------------------------------------------------------
    // level: 0.0 = mute, 1.0 = unchanged, >1.0 = amplify. Stored and
    // re-applied to the sound device whenever media becomes active.
    void  setSpeakerLevel(float level);   // playback / earpiece volume
    void  setMicLevel(float level);       // capture / microphone gain
    float speakerLevel() const { return speakerLevel_; }
    float micLevel() const { return micLevel_; }
    void  applyAudioLevels();             // push stored levels to the device

    // Ringer (local ringing for incoming calls) --------------------------
    // Uses a PJSIP tone generator played to the speaker. Ring level follows
    // the same convention as the other audio levels (1.0 = unchanged).
    void  startRinging();
    void  stopRinging();
    void  setRingLevel(float level);
    float ringLevel() const { return ringLevel_; }

    // Behaviour -----------------------------------------------------------
    void setAutoAnswer(bool on, int code) { autoAnswer_ = on; autoAnswerCode_ = code; }
    void setSdpOverride(const SdpOverride &o) { sdpOverride_ = o; }
    const SdpOverride &sdpOverride() const { return sdpOverride_; }

    // Apply the configured SDP override to an outgoing SDP string.
    QString applySdpOverride(const QString &in) const;

    // Internal hooks used by MyAccount / MyCall (they are not QObjects, so
    // they call these to emit signals on our behalf).
    void reportLog(const QString &line);
    void reportRegState(bool active, int code, const QString &reason);
    void reportCallState(const QString &state, const QString &remote,
                         const QString &reason);
    void reportMediaState(const QString &info);
    void reportSdp(const QString &label, const QString &sdp);
    void reportIncoming(MyCall *call, const QString &remote);
    bool autoAnswerEnabled() const { return autoAnswer_; }
    int  autoAnswerCode() const { return autoAnswerCode_; }
    void registerThread();        // register current thread with PJSIP
    void setCurrentCall(MyCall *c) { currentCall_ = c; }
    MyCall *currentCall() const { return currentCall_; }

signals:
    void logMessage(const QString &line);
    void registrationChanged(bool active, int code, const QString &reason);
    void callStateChanged(const QString &state, const QString &remote,
                          const QString &reason);
    void mediaStateChanged(const QString &info);
    void sdpCaptured(const QString &label, const QString &sdp);
    void incomingCall(const QString &remote);
    void errorOccurred(const QString &message);

private:
    pj::Endpoint ep_;
    std::unique_ptr<MyAccount> account_;
    // NOTE: ownership. Once passed to EpConfig.logConfig.writer and libInit()
    // runs, pjproject takes ownership of this object and deletes it inside
    // libDestroy() (pjsua2 endpoint.cpp: stores the pointer, then
    // `delete this->writer`). We must therefore NOT free it ourselves - doing
    // so is a double-free and crashes on shutdown. Hence a raw pointer, not a
    // unique_ptr, and we simply forget it (set to nullptr) after libDestroy().
    GuiLogWriter *logWriter_ = nullptr;
    MyCall *currentCall_ = nullptr;  // most recent / active call (owned by pj)

    bool created_ = false;
    bool started_ = false;
    bool autoAnswer_ = false;
    int  autoAnswerCode_ = 200;
    SdpOverride sdpOverride_;

    // Desired audio levels (1.0 = unchanged). Kept here so they survive
    // across calls and can be re-applied when the sound device opens.
    float speakerLevel_ = 1.0f;
    float micLevel_     = 1.0f;
    QString callerPai_;          // P-Asserted-Identity for outgoing calls
    QString callerRpid_;         // Remote-Party-ID for outgoing calls

    // Local ringer for incoming calls.
    std::unique_ptr<pj::ToneGenerator> ringGen_;
    float ringLevel_ = 0.8f;     // ringer volume (1.0 = unchanged)
    bool  ringing_   = false;
};
