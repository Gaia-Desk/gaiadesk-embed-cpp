// "Acme Desk": a Qt 6 Widgets app with a Get help button.
// SPDX-License-Identifier: MIT
#pragma once

#include <gaiadesk/embed.hpp>

#include <QMainWindow>

#include <optional>

class QCheckBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;

class HelpDeskWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit HelpDeskWindow(QWidget* parent = nullptr);
    ~HelpDeskWindow() override;

public slots:
    void getHelp();
    void stopHelp();

protected:
    void resizeEvent(QResizeEvent* e) override;
    void moveEvent(QMoveEvent* e) override;
    void showEvent(QShowEvent* e) override;

private:
    void onEvent(const gaiadesk::embed::Event& e);
    void setStatus(const QString& text);
    void setRunning(bool running);
    void updateMasks();
    bool askConsent(bool guided);
    // The card widget as a mask rect of this window, in the library's units.
    gaiadesk::embed::Rect cardMaskRect() const;
    std::optional<std::uint64_t> windowId() const;

    QLineEdit* token_ = nullptr;
    QFrame* card_ = nullptr;
    QLabel* bought_ = nullptr;
    QPushButton* help_ = nullptr;
    QPushButton* stop_ = nullptr;
    QCheckBox* pause_ = nullptr;
    QLabel* status_ = nullptr;
    int buys_ = 0;
    gaiadesk::embed::Session session_;
};
