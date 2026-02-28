#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QElapsedTimer>
#include <QVector>

class QLabel;
class QPushButton;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QThread;

class QDockWidget;
class QPlainTextEdit;
class QLineEdit;

class EmgWaveWidget;
class I24Session;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

signals:
    void startSessionRequested(const QString& portName);
    void stopSessionRequested();

private slots:
    void onStart();
    void onEnd();
    void onReset();
    void onSaveSnapshot();

    // Real device data (from I24Session)
    void onEmgSamples(const QVector<float>& ch1,
                      const QVector<float>& ch2,
                      const QVector<float>& ch3);
    void onBiValue(float ohms);

private:
    void buildUi();
    void applyDisplaySettings();

    // --- Log helpers ---
    void setupLogDock();
    void appendLog(const QString& line);

private:
    // ---- Session / thread (device mode) ----
    I24Session* session_ = nullptr;
    QThread* sessionThread_ = nullptr;

    // ---- Log dock ----
    QDockWidget* logDock_ = nullptr;
    QPlainTextEdit* logView_ = nullptr;

    // ---- Center display ----
    QWidget* wavesContainer_ = nullptr;
    EmgWaveWidget* waveCh1_ = nullptr;
    EmgWaveWidget* waveCh2_ = nullptr;
    EmgWaveWidget* waveCh3_ = nullptr;

    // ---- Controls ----
    QPushButton* btnStart_ = nullptr;
    QPushButton* btnEnd_   = nullptr;
    QPushButton* btnReset_ = nullptr;

    QLineEdit* editPort_ = nullptr;

    QSpinBox* spFs_ = nullptr;
    QDoubleSpinBox* spWinSec_ = nullptr;
    QDoubleSpinBox* spScale_ = nullptr;

    QCheckBox* cbEnvelope_ = nullptr;

    // ---- Metrics labels ----
    QLabel* lblState_ = nullptr;
    QLabel* lblFs_    = nullptr;
    QLabel* lblWin_   = nullptr;

    QLabel* lblRms_ = nullptr;
    QLabel* lblMav_ = nullptr;
    QLabel* lblP2P_ = nullptr;
    QLabel* lblEnv_ = nullptr;
    QLabel* lblFps_ = nullptr;

    // ---- Runtime ----
    QElapsedTimer fpsClock_;
    int frames_ = 0;

    bool running_ = false;
};

#endif // MAINWINDOW_H
