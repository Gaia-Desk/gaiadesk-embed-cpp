// SPDX-License-Identifier: MIT
#include "HelpDeskWindow.h"

#include <QCheckBox>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>
#include <QWindow>

#include <cstdio>
#include <variant>

#if defined(Q_OS_MACOS)
#include <gaiadesk/embed_macos.hpp>
#elif defined(Q_OS_WIN)
#include <gaiadesk/embed_win32.hpp>
#endif

namespace ge = gaiadesk::embed;

static bool envFlag(const char* name) { return qEnvironmentVariable(name) == QLatin1String("1"); }

static void log(const QString& line) {
    std::printf("%s\n", line.toUtf8().constData());
    std::fflush(stdout);
}

HelpDeskWindow::HelpDeskWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Acme Desk"));
    auto* central = new QWidget(this);
    auto* col = new QVBoxLayout(central);
    col->setContentsMargins(24, 20, 24, 20);
    col->setSpacing(14);

    auto* title = new QLabel(QStringLiteral("<h2>Acme Desk</h2>Your account"), central);
    col->addWidget(title);

    // The "card": painted out of every frame the support team sees.
    card_ = new QFrame(central);
    card_->setObjectName(QStringLiteral("card"));
    card_->setStyleSheet(QStringLiteral("#card { background: #b3261e; border-radius: 10px; } QLabel { color: white; }"));
    card_->setMinimumHeight(90);
    auto* cardCol = new QVBoxLayout(card_);
    cardCol->addWidget(new QLabel(QStringLiteral("Card number"), card_));
    auto* number = new QLabel(QStringLiteral("<b>4242 4242 4242 4242</b>"), card_);
    number->setStyleSheet(QStringLiteral("font-size: 20px;"));
    cardCol->addWidget(number);
    col->addWidget(card_);

    // Something for a guided agent to click.
    auto* buyRow = new QHBoxLayout;
    auto* buy = new QPushButton(QStringLiteral("Buy"), central);
    bought_ = new QLabel(QStringLiteral("Bought 0 times"), central);
    connect(buy, &QPushButton::clicked, this, [this] {
        bought_->setText(QStringLiteral("Bought %1 times").arg(++buys_));
    });
    buyRow->addWidget(buy);
    buyRow->addWidget(bought_, 1);
    col->addLayout(buyRow);

    token_ = new QLineEdit(qEnvironmentVariable("GAIADESK_EMBED_TOKEN"), central);
    token_->setPlaceholderText(QStringLiteral("embed_token from your backend (gdemb_…)"));
    token_->setEchoMode(QLineEdit::Password);
    col->addWidget(token_);

    auto* actions = new QHBoxLayout;
    help_ = new QPushButton(QStringLiteral("Get help"), central);
    help_->setDefault(true);
    stop_ = new QPushButton(QStringLiteral("Stop sharing"), central);
    pause_ = new QCheckBox(QStringLiteral("Pause control"), central);
    connect(help_, &QPushButton::clicked, this, &HelpDeskWindow::getHelp);
    connect(stop_, &QPushButton::clicked, this, &HelpDeskWindow::stopHelp);
    connect(pause_, &QCheckBox::toggled, this, [this](bool on) { session_.set_paused(on); });
    actions->addWidget(help_);
    actions->addWidget(stop_);
    actions->addWidget(pause_);
    actions->addStretch(1);
    col->addLayout(actions);

    status_ = new QLabel(central);
    status_->setWordWrap(true);
    col->addWidget(status_);
    col->addStretch(1);

    setCentralWidget(central);
    resize(460, 380);
    setRunning(false);
    setStatus(QStringLiteral("libgaiadesk_embed %1").arg(QString::fromStdString(ge::version())));
}

HelpDeskWindow::~HelpDeskWindow() {
    // End the session before the widgets its events update go away.
    session_.stop();
}

bool HelpDeskWindow::askConsent(bool guided) {
    if (envFlag("GAIADESK_DEMO_TEST_CONSENT")) return true;  // automated tests only: never in an app
    // The library's dialog (macOS, Windows), or ours where it has none (Linux).
    if (auto answer = ge::request_consent("Acme", ge::Capture::window, guided)) return *answer;
    QString text = QStringLiteral(
        "Acme support will see this window, not the rest of your screen. You can stop at any time.");
    if (guided) text += QStringLiteral("\n\nThey will also be able to click and type in this app while it is in front.");
    return QMessageBox::question(this, QStringLiteral("Share this window with Acme support?"), text,
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
}

void HelpDeskWindow::getHelp() {
    if (session_.active()) return;
    const bool guided = envFlag("GAIADESK_DEMO_GUIDED");
    if (!askConsent(guided)) {
        setStatus(QStringLiteral("Not shared."));
        return;
    }

    ge::Options options;
    options.embed_token = token_->text().trimmed().toStdString();
    options.server = qEnvironmentVariable("GAIADESK_SERVER").toStdString();
    options.company = "Acme";
    options.window_id = windowId();
    options.guided = guided;
    options.masks = {cardMaskRect()};  // in place before the first frame
    const auto& m = options.masks.front();
    log(QStringLiteral("window %1, mask %2,%3 %4x%5")
            .arg(options.window_id ? QString::number(*options.window_id) : QStringLiteral("(frontmost)"))
            .arg(m.x).arg(m.y).arg(m.w).arg(m.h));

    // Events come on the library's threads: hop to this window's (the UI)
    // thread. A queued call to a deleted object is dropped.
    QPointer<HelpDeskWindow> self(this);
    ge::Post toUi = [self](std::function<void()> f) {
        if (self) QMetaObject::invokeMethod(self, std::move(f), Qt::QueuedConnection);
    };
    auto started = ge::Session::try_start(options, /*consent_granted=*/true,
                                          [this](const ge::Event& e) { onEvent(e); }, toUi);
    if (!started) {
        const auto& why = started.error();
        log(QStringLiteral("refused %1").arg(QString::fromStdString(why.code)));
        setStatus(QStringLiteral("Could not start: %1 (%2)")
                      .arg(QString::fromStdString(why.message), QString::fromStdString(why.code)));
        return;
    }
    session_ = std::move(started).value();
    pause_->setChecked(false);
    pause_->setEnabled(guided);
    setRunning(true);
    setStatus(QStringLiteral("Starting…"));
}

void HelpDeskWindow::stopHelp() {
    session_.stop();  // "ended" (app_stopped) follows
}

void HelpDeskWindow::onEvent(const ge::Event& e) {
    // On the UI thread (see toUi).
    if (auto s = std::get_if<ge::StateEvent>(&e)) {
        log(QStringLiteral("event state %1").arg(QString::fromStdString(s->state)));
        if (s->state == "waiting") setStatus(QStringLiteral("Waiting for Acme support…"));
        if (s->state == "connected") setStatus(QStringLiteral("Connected."));
    } else if (auto c = std::get_if<ge::CodeEvent>(&e)) {
        log(QStringLiteral("event code %1 %2").arg(QString::fromStdString(c->code), QString::fromStdString(c->mode)));
        setStatus(QStringLiteral("Your support code is %1: read it to the agent.").arg(QString::fromStdString(c->code)));
    } else if (auto j = std::get_if<ge::JoinedEvent>(&e)) {
        const QString who = j->agent ? QString::fromStdString(*j->agent) : QStringLiteral("An agent");
        log(QStringLiteral("event joined %1").arg(who));
        setStatus(QStringLiteral("%1 is viewing this window.").arg(who));
    } else if (std::holds_alternative<ge::LeftEvent>(e)) {
        log(QStringLiteral("event left"));
        setStatus(QStringLiteral("The agent left. Still sharing until you stop."));
    } else if (auto ctl = std::get_if<ge::ControlEvent>(&e)) {
        log(QStringLiteral("event control %1").arg(ctl->allowed ? "allowed" : "paused"));
        const QSignalBlocker quiet(pause_);
        pause_->setChecked(!ctl->allowed);
    } else if (auto a = std::get_if<ge::ActionEvent>(&e)) {
        log(QStringLiteral("event action %1 %2 %3")
                .arg(QString::fromStdString(a->action), a->applied ? "applied" : "refused",
                     QString::fromStdString(a->reason.value_or(""))));
    } else if (auto err = std::get_if<ge::ErrorEvent>(&e)) {
        log(QStringLiteral("event error %1 %2").arg(QString::fromStdString(err->code), QString::fromStdString(err->message)));
        setStatus(QStringLiteral("Problem: %1").arg(QString::fromStdString(err->message)));
    } else if (auto end = std::get_if<ge::EndedEvent>(&e)) {
        log(QStringLiteral("event ended %1").arg(QString::fromStdString(end->reason)));
        session_ = ge::Session();  // frees the handle (the session is over)
        setRunning(false);
        setStatus(end->reason == "stopped" ? QStringLiteral("You stopped sharing.")
                                           : QStringLiteral("Sharing ended (%1).").arg(QString::fromStdString(end->reason)));
    } else if (auto u = std::get_if<ge::UnknownEvent>(&e)) {
        log(QStringLiteral("event %1").arg(QString::fromStdString(u->json)));
    }
}

void HelpDeskWindow::setStatus(const QString& text) { status_->setText(text); }

void HelpDeskWindow::setRunning(bool running) {
    help_->setEnabled(!running);
    stop_->setEnabled(running);
    token_->setEnabled(!running);
    if (!running) pause_->setEnabled(false);
}

void HelpDeskWindow::resizeEvent(QResizeEvent* e) {
    QMainWindow::resizeEvent(e);
    updateMasks();
}

void HelpDeskWindow::moveEvent(QMoveEvent* e) {
    QMainWindow::moveEvent(e);
    updateMasks();  // Windows: another display can mean another scale
}

void HelpDeskWindow::showEvent(QShowEvent* e) {
    QMainWindow::showEvent(e);
    if (auto* w = windowHandle())
        connect(w, &QWindow::screenChanged, this, &HelpDeskWindow::updateMasks, Qt::UniqueConnection);
}

void HelpDeskWindow::updateMasks() {
    if (!session_.active()) return;
    if (auto r = session_.try_set_mask_rects({cardMaskRect()}); !r)
        log(QStringLiteral("masks refused %1").arg(QString::fromStdString(r.error().code)));
}

ge::Rect HelpDeskWindow::cardMaskRect() const {
    // The card in this window's client (content) area, in Qt's
    // device-independent pixels.
    const QPoint p = card_->mapTo(this, QPoint(0, 0));
    const double x = p.x(), y = p.y(), w = card_->width(), h = card_->height();
#if defined(Q_OS_MACOS)
    // Points, relative to the window FRAME: add the title bar.
    const QPoint off = geometry().topLeft() - frameGeometry().topLeft();
    return {x + off.x(), y + off.y(), w, h};
#elif defined(Q_OS_WIN)
    // GetWindowRect units: Qt 6 is per-monitor DPI aware, so physical
    // pixels. Scale, then add the client area's offset in the window frame.
    const double s = devicePixelRatioF();
    if (QGuiApplication::platformName() == QLatin1String("windows"))
        if (auto r = ge::win32::mask_rect_from_client(reinterpret_cast<HWND>(winId()), x * s, y * s, w * s, h * s))
            return *r;
    return {x * s, y * s, w * s, h * s};
#else
    // X11: pixels of the app's own (client) window.
    const double s = devicePixelRatioF();
    return {x * s, y * s, w * s, h * s};
#endif
}

std::optional<std::uint64_t> HelpDeskWindow::windowId() const {
    // winId() is a native handle only on the native platform plugin (not,
    // say, QT_QPA_PLATFORM=offscreen): otherwise let the library pick this
    // app's frontmost window.
    const QString platform = QGuiApplication::platformName();
#if defined(Q_OS_MACOS)
    if (platform != QLatin1String("cocoa")) return std::nullopt;
    return ge::macos::window_id_of_nsview(reinterpret_cast<void*>(winId()));  // an NSView*
#elif defined(Q_OS_WIN)
    if (platform != QLatin1String("windows")) return std::nullopt;
    return ge::win32::window_id(reinterpret_cast<HWND>(winId()));
#else
    if (platform != QLatin1String("xcb")) return std::nullopt;  // X11 (or XWayland) only
    return static_cast<std::uint64_t>(winId());
#endif
}
