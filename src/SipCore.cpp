// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Michael Bradeen and phzyxPhone contributors.
#include "SipCore.h"

#include <QRegularExpression>
#include <QStringList>
#include <pjsua2.hpp>

using namespace pj;

// ===========================================================================
// Log writer: forwards every PJSIP log line to the GUI.
// ===========================================================================
class GuiLogWriter : public LogWriter {
public:
    explicit GuiLogWriter(SipCore *core) : core_(core) {}
    void write(const LogEntry &entry) override {
        // entry.msg already contains a trailing newline; trim it.
        std::string m = entry.msg;
        while (!m.empty() && (m.back() == '\n' || m.back() == '\r'))
            m.pop_back();
        core_->reportLog(QString::fromStdString(m));
    }
private:
    SipCore *core_;
};

// ===========================================================================
// MyCall: per-call callbacks
// ===========================================================================
class MyCall : public Call {
public:
    MyCall(Account &acc, SipCore *core, int callId = PJSUA_INVALID_ID)
        : Call(acc, callId), core_(core) {}

    // True when this is an RTT text-only session rather than an audio/video
    // media call. Set by makeCall/startTextSession and by the incoming-call
    // handler after inspecting the offered media.
    bool textOnly_ = false;

    void onCallState(OnCallStateParam &prm) override;
    void onCallMediaState(OnCallMediaStateParam &prm) override;
    void onCallSdpCreated(OnCallSdpCreatedParam &prm) override;
    void onDtmfDigit(OnDtmfDigitParam &prm) override;
    void onCallRxText(OnCallRxTextParam &prm) override;

private:
    SipCore *core_;
};

// ===========================================================================
// MyAccount: registration + incoming call callbacks
// ===========================================================================
class MyAccount : public Account {
public:
    explicit MyAccount(SipCore *core) : core_(core) {}

    void onRegState(OnRegStateParam &prm) override {
        AccountInfo ai = getInfo();
        core_->reportRegState(ai.regIsActive, prm.code,
                              QString::fromStdString(prm.reason));
    }

    void onIncomingCall(OnIncomingCallParam &prm) override {
        auto *call = new MyCall(*this, core_, prm.callId);
        CallInfo ci = call->getInfo();
        const QString peer = QString::fromStdString(ci.remoteUri);

        // Distinguish an RTT text session from an audio/video media call by
        // looking at the offered media: a text-only INVITE has a text stream
        // and no audio or video. These are handled completely separately - no
        // ringing, no Phone-tab Answer; the user accepts the conversation.
        const bool textOnly = (ci.remTextCount > 0 &&
                               ci.remAudioCount == 0 &&
                               ci.remVideoCount == 0);
        if (textOnly) {
            call->textOnly_ = true;
            core_->setCurrentTextCall(call);
            core_->reportIncomingText(call, peer);
            return;   // wait for the user to accept (acceptTextSession)
        }

        core_->setCurrentCall(call);
        core_->reportIncoming(call, peer);

        if (!core_->autoAnswerEnabled())
            core_->startRinging();

        if (core_->autoAnswerEnabled()) {
            CallOpParam op;
            op.statusCode = (pjsip_status_code)core_->autoAnswerCode();
            try { call->answer(op); } catch (Error &e) {
                core_->reportLog(QString("auto-answer failed: %1")
                                     .arg(QString::fromStdString(e.info())));
            }
        }
    }

private:
    SipCore *core_;
};

// --- MyCall method bodies (need full MyAccount/SipCore visibility) ---------

void MyCall::onCallState(OnCallStateParam &prm) {
    (void)prm;
    CallInfo ci = getInfo();

    // Text sessions are reported on their own channel and never touch the
    // ringer or the Phone/Call tabs.
    if (textOnly_) {
        core_->reportTextCallState(QString::fromStdString(ci.stateText),
                                   QString::fromStdString(ci.remoteUri),
                                   QString::fromStdString(ci.lastReason));
        if (ci.state == PJSIP_INV_STATE_DISCONNECTED) {
            if (core_->currentTextCall() == this)
                core_->setCurrentTextCall(nullptr);
            delete this;
        }
        return;
    }

    core_->reportCallState(QString::fromStdString(ci.stateText),
                           QString::fromStdString(ci.remoteUri),
                           QString::fromStdString(ci.lastReason));
    // Local ringing stops as soon as the call is answered or torn down;
    // it continues through INCOMING / EARLY.
    if (ci.state == PJSIP_INV_STATE_CONNECTING ||
        ci.state == PJSIP_INV_STATE_CONFIRMED  ||
        ci.state == PJSIP_INV_STATE_DISCONNECTED)
        core_->stopRinging();
    if (ci.state == PJSIP_INV_STATE_DISCONNECTED) {
        if (core_->currentCall() == this)
            core_->setCurrentCall(nullptr);
        // Safe to delete the call object once disconnected.
        delete this;
    }
}

void MyCall::onCallMediaState(OnCallMediaStateParam &prm) {
    (void)prm;
    CallInfo ci = getInfo();
    for (unsigned i = 0; i < ci.media.size(); ++i) {
        if (ci.media[i].type == PJMEDIA_TYPE_AUDIO &&
            ci.media[i].status == PJSUA_CALL_MEDIA_ACTIVE) {
            try {
                AudioMedia am = getAudioMedia(i);
                AudDevManager &mgr = Endpoint::instance().audDevManager();
                mgr.getCaptureDevMedia().startTransmit(am);
                am.startTransmit(mgr.getPlaybackDevMedia());
                // Re-apply the user's chosen speaker/mic levels now that the
                // sound device is open for this call.
                core_->applyAudioLevels();
                core_->reportMediaState(
                    QString("audio active on stream %1").arg(i));
            } catch (Error &e) {
                core_->reportLog(QString("media connect error: %1")
                                     .arg(QString::fromStdString(e.info())));
            }
        }
    }
}

void MyCall::onCallSdpCreated(OnCallSdpCreatedParam &prm) {
    const bool weAreOfferer = prm.remSdp.wholeSdp.empty();
    const QString label = weAreOfferer ? "LOCAL OFFER" : "LOCAL ANSWER";
    QString original = QString::fromStdString(prm.sdp.wholeSdp);
    core_->reportSdp(label, original);
    if (!prm.remSdp.wholeSdp.empty())
        core_->reportSdp("REMOTE OFFER",
                         QString::fromStdString(prm.remSdp.wholeSdp));

    // Apply user-configured override before the SDP goes on the wire.
    if (core_->sdpOverride().enabled) {
        QString modified = core_->applySdpOverride(original);
        if (modified != original) {
            prm.sdp.wholeSdp = modified.toStdString();
            core_->reportSdp(label + " (OVERRIDDEN)", modified);
        }
    }
}

void MyCall::onDtmfDigit(OnDtmfDigitParam &prm) {
    core_->reportLog(QString("DTMF received: %1")
                         .arg(QString::fromStdString(prm.digit)));
}

void MyCall::onCallRxText(OnCallRxTextParam &prm) {
    // RFC 4103 text blocks can be empty (idle / keep-alive); ignore those.
    if (prm.text.empty()) return;
    CallInfo ci = getInfo();
    core_->reportRxText(this, QString::fromStdString(ci.remoteUri),
                        QString::fromStdString(prm.text));
}

// ===========================================================================
// SipCore
// ===========================================================================
SipCore::SipCore(QObject *parent) : QObject(parent) {}

SipCore::~SipCore() {
    shutdown();
}

void SipCore::registerThread() {
    try {
        if (!ep_.libIsThreadRegistered())
            ep_.libRegisterThread("qt-gui");
    } catch (...) { /* ignore */ }
}

bool SipCore::startEndpoint(int logLevel, const QString &stunServer,
                            QString &err) {
    try {
        ep_.libCreate();
        created_ = true;

        EpConfig epCfg;
        epCfg.uaConfig.userAgent = "phzyxPhone/0.1 (PJSIP test softphone)";
        if (!stunServer.trimmed().isEmpty())
            epCfg.uaConfig.stunServer.push_back(stunServer.toStdString());

        // pjproject takes ownership of this writer in libInit() and deletes it
        // in libDestroy(); see the ownership note in SipCore.h. We hand over a
        // bare `new` and never delete it ourselves.
        logWriter_ = new GuiLogWriter(this);
        epCfg.logConfig.level = logLevel;
        epCfg.logConfig.consoleLevel = logLevel;
        epCfg.logConfig.writer = logWriter_;
        epCfg.logConfig.decor =
            PJ_LOG_HAS_SENDER | PJ_LOG_HAS_TIME | PJ_LOG_HAS_MICRO_SEC;

        ep_.libInit(epCfg);
        registerThread();
        return true;
    } catch (Error &e) {
        err = QString::fromStdString(e.info());
        return false;
    }
}

bool SipCore::createTransport(const QString &type, int port,
                              const QString &boundAddress, QString &err) {
    try {
        TransportConfig tcfg;
        tcfg.port = (port < 0) ? 0 : (unsigned)port;
        if (!boundAddress.trimmed().isEmpty())
            tcfg.boundAddress = boundAddress.toStdString();

        pjsip_transport_type_e tt = PJSIP_TRANSPORT_UDP;
        if (type.compare("TCP", Qt::CaseInsensitive) == 0)
            tt = PJSIP_TRANSPORT_TCP;
        else if (type.compare("TLS", Qt::CaseInsensitive) == 0)
            tt = PJSIP_TRANSPORT_TLS;

        ep_.transportCreate(tt, tcfg);
        return true;
    } catch (Error &e) {
        err = QString::fromStdString(e.info());
        return false;
    }
}

bool SipCore::startLibrary(QString &err) {
    try {
        ep_.libStart();
        started_ = true;
        return true;
    } catch (Error &e) {
        err = QString::fromStdString(e.info());
        return false;
    }
}

bool SipCore::addAccount(const AccountSettings &s, QString &err) {
    try {
        AccountConfig acfg;
        acfg.idUri = s.idUri.toStdString();
        if (!s.registrar.trimmed().isEmpty())
            acfg.regConfig.registrarUri = s.registrar.toStdString();

        if (!s.username.isEmpty()) {
            AuthCredInfo cred("digest", s.realm.toStdString(),
                              s.username.toStdString(), 0,
                              s.password.toStdString());
            acfg.sipConfig.authCreds.push_back(cred);
        }

        // Transport binding for this account's signalling.
        // (UDP/TCP/TLS already created globally; leave default routing.)

        // NAT traversal
        acfg.natConfig.iceEnabled = s.iceEnabled;
        acfg.natConfig.sdpNatRewriteUse = s.sdpNatRewrite ? PJ_TRUE : PJ_FALSE;
        if (s.turnEnabled) {
            acfg.natConfig.turnEnabled = true;
            acfg.natConfig.turnServer = s.turnServer.toStdString();
            if (!s.turnUser.isEmpty()) {
                acfg.natConfig.turnUserName = s.turnUser.toStdString();
                acfg.natConfig.turnPassword = s.turnPassword.toStdString();
                acfg.natConfig.turnPasswordType = 0; // plain
            }
        }

        // SRTP
        acfg.mediaConfig.srtpUse = (pjmedia_srtp_use)s.srtpUse;
        acfg.mediaConfig.srtpSecureSignaling = s.srtpSignaling;

        // Caller ID headers applied to outgoing INVITEs (see makeCall).
        callerPai_  = s.pAssertedIdentity;
        callerRpid_ = s.remotePartyId;

        account_ = std::make_unique<MyAccount>(this);
        account_->create(acfg, true /* make default */);
        return true;
    } catch (Error &e) {
        err = QString::fromStdString(e.info());
        return false;
    }
}

bool SipCore::bringUp(const AccountSettings &s, int logLevel, QString &err) {
    if (!startEndpoint(logLevel, s.stunServer, err)) return false;
    if (!createTransport(s.transport, s.localPort, s.boundAddress, err))
        return false;
    if (!startLibrary(err)) return false;
    if (!addAccount(s, err)) return false;
    return true;
}

void SipCore::shutdown() {
    if (!created_) return;
    try {
        currentCall_ = nullptr;
        currentTextCall_ = nullptr;
        stopRinging();
        ringGen_.reset();   // release the tone generator before libDestroy
        account_.reset();
        ep_.libDestroy();   // this also `delete`s the log writer pjproject owns
    } catch (...) { /* ignore */ }
    logWriter_ = nullptr;   // pjproject already freed it; just forget the pointer
    created_ = false;
    started_ = false;
}

// --- Codec matrix ----------------------------------------------------------
QVector<CodecRow> SipCore::enumerateCodecs() {
    QVector<CodecRow> out;
    if (!created_) return out;
    registerThread();
    try {
        const CodecInfoVector2 codecs = ep_.codecEnum2();
        for (const CodecInfo &ci : codecs) {
            CodecRow row;
            row.codecId  = QString::fromStdString(ci.codecId);
            row.desc     = QString::fromStdString(ci.desc);
            row.priority = ci.priority;
            try {
                CodecParam p = ep_.codecGetParam(ci.codecId);
                row.clockRate    = p.info.clockRate;
                row.channels     = p.info.channelCnt;
                row.avgBps       = p.info.avgBps;
                row.vad          = p.setting.vad;
                row.plc          = p.setting.plc;
                row.framesPerPkt = p.setting.frmPerPkt;
            } catch (...) { /* some codecs have no param */ }
            out.push_back(row);
        }
    } catch (Error &e) {
        reportLog(QString("codecEnum failed: %1")
                      .arg(QString::fromStdString(e.info())));
    }
    return out;
}

void SipCore::applyCodecRow(const CodecRow &row) {
    if (!created_) return;
    registerThread();
    std::string id = row.codecId.toStdString();
    try {
        // priority 0 disables the codec entirely.
        ep_.codecSetPriority(id, (pj_uint8_t)row.priority);
        if (row.priority > 0) {
            CodecParam p = ep_.codecGetParam(id);
            p.setting.vad       = row.vad;
            p.setting.plc       = row.plc;
            if (row.framesPerPkt > 0) p.setting.frmPerPkt = row.framesPerPkt;
            if (row.avgBps > 0)       p.info.avgBps = row.avgBps;
            ep_.codecSetParam(id, p);
        }
    } catch (Error &e) {
        reportLog(QString("codec '%1' apply failed: %2")
                      .arg(row.codecId, QString::fromStdString(e.info())));
    }
}

// --- Calls -----------------------------------------------------------------
bool SipCore::makeCall(const QString &destUri,
                       const QList<QPair<QString, QString>> &extraHeaders,
                       QString &err) {
    if (!account_) { err = "No account configured"; return false; }
    registerThread();
    try {
        auto *call = new MyCall(*account_, this);
        CallOpParam prm(true /* use default call settings */);
        // A media call offers audio only (no RTT text stream - text sessions
        // are placed separately via startTextSession()).
        prm.opt.audioCount = 1;
        prm.opt.videoCount = 0;
        prm.opt.textCount  = 0;
        // Caller ID identity headers (only when configured).
        if (!callerPai_.isEmpty()) {
            SipHeader sh;
            sh.hName  = "P-Asserted-Identity";
            sh.hValue = callerPai_.toStdString();
            prm.txOption.headers.push_back(sh);
        }
        if (!callerRpid_.isEmpty()) {
            SipHeader sh;
            sh.hName  = "Remote-Party-ID";
            sh.hValue = callerRpid_.toStdString();
            prm.txOption.headers.push_back(sh);
        }
        for (const auto &h : extraHeaders) {
            SipHeader sh;
            sh.hName  = h.first.toStdString();
            sh.hValue = h.second.toStdString();
            prm.txOption.headers.push_back(sh);
        }
        call->makeCall(destUri.toStdString(), prm);
        currentCall_ = call;
        return true;
    } catch (Error &e) {
        err = QString::fromStdString(e.info());
        return false;
    }
}

void SipCore::answerCurrent(int statusCode) {
    if (!currentCall_) return;
    registerThread();
    try {
        CallOpParam prm;
        prm.statusCode = (pjsip_status_code)statusCode;
        currentCall_->answer(prm);
    } catch (Error &e) {
        reportLog(QString("answer failed: %1")
                      .arg(QString::fromStdString(e.info())));
    }
}

void SipCore::hangupCurrent(int statusCode) {
    if (!currentCall_) return;
    registerThread();
    try {
        CallOpParam prm;
        prm.statusCode = (pjsip_status_code)statusCode;
        currentCall_->hangup(prm);
    } catch (Error &e) {
        reportLog(QString("hangup failed: %1")
                      .arg(QString::fromStdString(e.info())));
    }
}

void SipCore::holdCurrent() {
    if (!currentCall_) return;
    registerThread();
    try {
        CallOpParam prm;
        currentCall_->setHold(prm);
    } catch (Error &e) {
        reportLog(QString("hold failed: %1")
                      .arg(QString::fromStdString(e.info())));
    }
}

void SipCore::reinviteCurrent() {
    if (!currentCall_) return;
    registerThread();
    try {
        CallOpParam prm(true);
        prm.opt.flag |= PJSUA_CALL_UPDATE_CONTACT;
        currentCall_->reinvite(prm);
    } catch (Error &e) {
        reportLog(QString("re-INVITE failed: %1")
                      .arg(QString::fromStdString(e.info())));
    }
}

void SipCore::sendDtmf(const QString &digits, int method, unsigned durationMs) {
    if (!currentCall_) return;
    registerThread();
    try {
        CallSendDtmfParam p;
        // 0 = RFC2833, 1 = SIP INFO
        p.method = (method == 1) ? PJSUA_DTMF_METHOD_SIP_INFO
                                 : PJSUA_DTMF_METHOD_RFC2833;
        if (durationMs > 0) p.duration = durationMs;
        p.digits = digits.toStdString();
        currentCall_->sendDtmf(p);
    } catch (Error &e) {
        reportLog(QString("DTMF send failed: %1")
                      .arg(QString::fromStdString(e.info())));
    }
}

// --- Real-time text (RFC 4103 / T.140) -------------------------------------
// Start an outgoing text-only session: the INVITE offers a T.140 text stream
// and *no* audio or video, so it is negotiated entirely separately from a
// regular media call.
bool SipCore::startTextSession(const QString &destUri, QString &err) {
    if (!account_) { err = "No account configured"; return false; }
    registerThread();
    try {
        auto *call = new MyCall(*account_, this);
        call->textOnly_ = true;
        CallOpParam prm(true /* use default call settings */);
        prm.opt.audioCount = 0;
        prm.opt.videoCount = 0;
        prm.opt.textCount  = 1;
        // Caller ID identity headers (only when configured).
        if (!callerPai_.isEmpty()) {
            SipHeader sh;
            sh.hName  = "P-Asserted-Identity";
            sh.hValue = callerPai_.toStdString();
            prm.txOption.headers.push_back(sh);
        }
        if (!callerRpid_.isEmpty()) {
            SipHeader sh;
            sh.hName  = "Remote-Party-ID";
            sh.hValue = callerRpid_.toStdString();
            prm.txOption.headers.push_back(sh);
        }
        call->makeCall(destUri.toStdString(), prm);
        currentTextCall_ = call;
        return true;
    } catch (Error &e) {
        err = QString::fromStdString(e.info());
        return false;
    }
}

// Accept a pending incoming text session, answering with a text-only SDP.
void SipCore::acceptTextSession() {
    if (!currentTextCall_) return;
    registerThread();
    try {
        CallOpParam prm;
        prm.statusCode = (pjsip_status_code)200;
        prm.opt.audioCount = 0;
        prm.opt.videoCount = 0;
        prm.opt.textCount  = 1;
        currentTextCall_->answer(prm);
    } catch (Error &e) {
        reportLog(QString("accept text session failed: %1")
                      .arg(QString::fromStdString(e.info())));
    }
}

// Hang up / decline the current text session.
void SipCore::hangupTextSession(int statusCode) {
    if (!currentTextCall_) return;
    registerThread();
    try {
        CallOpParam prm;
        prm.statusCode = (pjsip_status_code)statusCode;
        currentTextCall_->hangup(prm);
    } catch (Error &e) {
        reportLog(QString("hangup text session failed: %1")
                      .arg(QString::fromStdString(e.info())));
    }
}

void SipCore::sendRtt(const QString &text) {
    if (!currentTextCall_ || text.isEmpty()) return;
    registerThread();
    try {
        CallInfo ci = currentTextCall_->getInfo();
        const QString peer = QString::fromStdString(ci.remoteUri);
        CallSendTextParam p;
        p.medIdx = -1;                 // first (default) text stream
        p.text   = text.toStdString();
        currentTextCall_->sendText(p);
        emit rttSent(peer, text);
    } catch (Error &e) {
        reportLog(QString("RTT send failed: %1")
                      .arg(QString::fromStdString(e.info())));
    }
}

// --- Audio levels ----------------------------------------------------------
void SipCore::setSpeakerLevel(float level) {
    speakerLevel_ = level;
    applyAudioLevels();
}

void SipCore::setMicLevel(float level) {
    micLevel_ = level;
    applyAudioLevels();
}

void SipCore::applyAudioLevels() {
    if (!started_) return;     // sound device only exists after libStart()
    registerThread();
    try {
        AudDevManager &mgr = ep_.audDevManager();
        // adjustTxLevel on the playback port scales what is heard (speaker);
        // adjustRxLevel on the capture port scales what is sent (microphone).
        mgr.getPlaybackDevMedia().adjustTxLevel(speakerLevel_);
        mgr.getCaptureDevMedia().adjustRxLevel(micLevel_);
    } catch (Error &e) {
        // The sound device may not be open yet (no active call); the levels
        // are stored and will be re-applied from onCallMediaState().
        reportLog(QString("audio level apply deferred: %1")
                      .arg(QString::fromStdString(e.info())));
    }
}

// ---------------------------------------------------------------------------
// Ringer: a looping tone played to the speaker while an incoming call rings.
// ---------------------------------------------------------------------------
void SipCore::startRinging() {
    if (!started_ || ringing_) return;
    registerThread();
    try {
        if (!ringGen_) {
            ringGen_ = std::make_unique<ToneGenerator>();
            ringGen_->createToneGenerator();   // default 16 kHz / mono
        }
        // Classic ring cadence: 440+480 Hz, 2 s on / 4 s off, looping.
        ToneDesc t;
        t.freq1   = 440;
        t.freq2   = 480;
        t.on_msec = 2000;
        t.off_msec= 4000;
        ToneDescVector tones;
        tones.push_back(t);
        ringGen_->play(tones, true /* loop */);
        ringGen_->startTransmit(ep_.audDevManager().getPlaybackDevMedia());
        ringGen_->adjustTxLevel(ringLevel_);
        ringing_ = true;
    } catch (Error &e) {
        reportLog(QString("ring start failed: %1")
                      .arg(QString::fromStdString(e.info())));
    }
}

void SipCore::stopRinging() {
    if (!ringing_) return;
    registerThread();
    try {
        if (ringGen_) {
            ringGen_->stopTransmit(ep_.audDevManager().getPlaybackDevMedia());
            ringGen_->stop();
        }
    } catch (Error &e) {
        reportLog(QString("ring stop failed: %1")
                      .arg(QString::fromStdString(e.info())));
    }
    ringing_ = false;
}

void SipCore::setRingLevel(float level) {
    ringLevel_ = level;
    if (ringing_ && ringGen_) {
        registerThread();
        try { ringGen_->adjustTxLevel(ringLevel_); }
        catch (Error &) { /* device may be transitioning; ignore */ }
    }
}

QString SipCore::currentStreamStats() {
    if (!currentCall_) return "No active call.";
    registerThread();
    QString s;
    try {
        CallInfo ci = currentCall_->getInfo();
        for (unsigned i = 0; i < ci.media.size(); ++i) {
            if (ci.media[i].type != PJMEDIA_TYPE_AUDIO) continue;
            StreamStat st = currentCall_->getStreamStat(i);
            const RtcpStreamStat &rx = st.rtcp.rxStat;
            const RtcpStreamStat &tx = st.rtcp.txStat;
            s += QString("Stream %1\n").arg(i);
            s += QString("  TX  pkts=%1  bytes=%2  loss=%3\n")
                     .arg(tx.pkt).arg(tx.bytes).arg(tx.loss);
            s += QString("  RX  pkts=%1  bytes=%2  loss=%3  dup=%4  reorder=%5\n")
                     .arg(rx.pkt).arg(rx.bytes).arg(rx.loss)
                     .arg(rx.dup).arg(rx.reorder);
            s += QString("  RTT(mean) = %1 us\n")
                     .arg(st.rtcp.rttUsec.mean);
            s += QString("  Jitter rx(mean) = %1 us\n")
                     .arg(rx.jitterUsec.mean);
        }
    } catch (Error &e) {
        s = QString("stats unavailable: %1")
                .arg(QString::fromStdString(e.info()));
    }
    return s.isEmpty() ? "No audio stream yet." : s;
}

// --- SDP override engine ---------------------------------------------------
QString SipCore::applySdpOverride(const QString &in) const {
    if (!sdpOverride_.enabled) return in;
    if (sdpOverride_.replaceWhole && !sdpOverride_.wholeSdp.trimmed().isEmpty())
        return sdpOverride_.wholeSdp;

    QString out = in;
    for (const auto &sub : sdpOverride_.substitutions) {
        if (sub.first.isEmpty()) continue;
        QRegularExpression re(sub.first);
        out.replace(re, sub.second);
    }
    if (!sdpOverride_.appendLines.isEmpty()) {
        QString normalized = out;
        normalized.replace("\r\n", "\n");
        QStringList lines = normalized.split('\n', Qt::SkipEmptyParts);
        for (const QString &l : sdpOverride_.appendLines) {
            if (!l.trimmed().isEmpty()) lines << l.trimmed();
        }
        out = lines.join("\r\n") + "\r\n";
    }
    return out;
}

// --- Signal forwarding (called from PJSIP worker threads) ------------------
void SipCore::reportLog(const QString &line)            { emit logMessage(line); }
void SipCore::reportRegState(bool a, int c, const QString &r) {
    emit registrationChanged(a, c, r);
}
void SipCore::reportCallState(const QString &st, const QString &rem,
                              const QString &reason) {
    emit callStateChanged(st, rem, reason);
}
void SipCore::reportMediaState(const QString &info) { emit mediaStateChanged(info); }
void SipCore::reportSdp(const QString &label, const QString &sdp) {
    emit sdpCaptured(label, sdp);
}
void SipCore::reportIncoming(MyCall *, const QString &remote) {
    emit incomingCall(remote);
}
void SipCore::reportRxText(MyCall *, const QString &peer, const QString &text) {
    emit rttReceived(peer, text);
}
void SipCore::reportIncomingText(MyCall *, const QString &peer) {
    emit incomingTextCall(peer);
}
void SipCore::reportTextCallState(const QString &state, const QString &peer,
                                  const QString &reason) {
    emit textCallStateChanged(state, peer, reason);
}
