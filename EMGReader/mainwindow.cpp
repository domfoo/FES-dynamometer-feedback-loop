#include "mainwindow.h"
#include "emg_metrics.h"
#include "emg_wave_widget.h"
#include "i24session.h"

#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QStatusBar>
#include <QThread>
#include <QDockWidget>
#include <QPlainTextEdit>
#include <QDateTime>
#include <QLineEdit>
#include <cmath>
#include <cstdlib>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();

    setupLogDock();
    appendLog("App started.");

    // ---- I24Session thread wiring (device mode) ----
    session_ = new I24Session;
    sessionThread_ = new QThread(this);
    session_->moveToThread(sessionThread_);

    connect(this, &MainWindow::startSessionRequested,
            session_, &I24Session::start,
            Qt::QueuedConnection);

    connect(this, &MainWindow::stopSessionRequested,
            session_, &I24Session::stop,
            Qt::QueuedConnection);

    connect(sessionThread_, &QThread::finished,
            session_, &QObject::deleteLater);

    connect(session_, &I24Session::statusChanged,
            this, [this](const QString& s){
                statusBar()->showMessage(s, 3000);
                appendLog("STATUS: " + s);
            });

    connect(session_, &I24Session::errorOccurred,
            this, [this](const QString& e){
                statusBar()->showMessage("ERROR: " + e, 6000);
                appendLog("ERROR: " + e);
            });

    connect(session_, &I24Session::emgSamplesAvailable,
            this, &MainWindow::onEmgSamples);

    connect(session_, &I24Session::bioImpedanceAvailable,
            this, &MainWindow::onBiValue);

    connect(sessionThread_, &QThread::finished, this, [this]{
        appendLog("sessionThread: finished (event loop stopped)");
    });

    sessionThread_->start();

    appendLog(QString("sessionThread ptr=0x%1").arg((quintptr)sessionThread_, 0, 16));
    appendLog(QString("session_->thread ptr=0x%1").arg((quintptr)session_->thread(), 0, 16));

    connect(sessionThread_, &QThread::started, this, [this]{
        appendLog("sessionThread: started (event loop should be running)");
    });
    appendLog(QString("session_ affinity thread ptr=0x%1")
                  .arg((quintptr)session_->thread(), 0, 16));


    statusBar()->showMessage("Ready.");
}

MainWindow::~MainWindow()
{
    // Stop session thread cleanly
    if (session_) {
        QMetaObject::invokeMethod(session_, [this]{
            session_->stop();
        }, Qt::QueuedConnection);
    }
    if (sessionThread_) {
        sessionThread_->quit();
        sessionThread_->wait();
    }
}

void MainWindow::buildUi()
{
    auto* central = new QWidget(this);
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(10);

    // =========================
    // Left side: Metrics bar + 3 wave windows
    // =========================
    auto* leftWrap = new QVBoxLayout();
    leftWrap->setContentsMargins(0, 0, 0, 0);
    leftWrap->setSpacing(8);

    // ---- Metrics bar ----
    auto* metricsBar = new QWidget(this);
    auto* mg = new QGridLayout(metricsBar);
    mg->setContentsMargins(6, 6, 6, 6);
    mg->setHorizontalSpacing(10);
    mg->setVerticalSpacing(4);

    lblState_ = new QLabel("Stopped", this);
    lblFs_    = new QLabel("-", this);
    lblWin_   = new QLabel("-", this);

    lblRms_ = new QLabel("-", this);
    lblMav_ = new QLabel("-", this);
    lblP2P_ = new QLabel("-", this);
    lblEnv_ = new QLabel("-", this);
    lblFps_ = new QLabel("-", this);

    int r = 0;
    mg->addWidget(new QLabel("State:", this), r, 0); mg->addWidget(lblState_, r, 1);
    mg->addWidget(new QLabel("Fs:", this),    r, 2); mg->addWidget(lblFs_,    r, 3);
    mg->addWidget(new QLabel("Win:", this),   r, 4); mg->addWidget(lblWin_,   r, 5);

    r = 1;
    mg->addWidget(new QLabel("RMS:", this), r, 0); mg->addWidget(lblRms_, r, 1);
    mg->addWidget(new QLabel("MAV:", this), r, 2); mg->addWidget(lblMav_, r, 3);
    mg->addWidget(new QLabel("P2P:", this), r, 4); mg->addWidget(lblP2P_, r, 5);
    mg->addWidget(new QLabel("Env:", this), r, 6); mg->addWidget(lblEnv_, r, 7);
    mg->addWidget(new QLabel("FPS:", this), r, 8); mg->addWidget(lblFps_, r, 9);

    leftWrap->addWidget(metricsBar);

    // ---- 3-channel wave container ----
    wavesContainer_ = new QWidget(this);
    auto* wavesLayout = new QVBoxLayout(wavesContainer_);
    wavesLayout->setContentsMargins(0, 0, 0, 0);
    wavesLayout->setSpacing(8);

    auto makeChannelBox = [&](const QString& title, EmgWaveWidget*& outWidget) {
        auto* box = new QGroupBox(title, this);
        auto* v = new QVBoxLayout(box);
        v->setContentsMargins(6, 10, 6, 6);

        outWidget = new EmgWaveWidget(this);
        outWidget->setMinimumHeight(180);
        v->addWidget(outWidget);

        return box;
    };

    wavesLayout->addWidget(makeChannelBox("CH1 (EMG)", waveCh1_));
    wavesLayout->addWidget(makeChannelBox("CH2 (EMG)", waveCh2_));
    wavesLayout->addWidget(makeChannelBox("CH3 (EMG)", waveCh3_));

    leftWrap->addWidget(wavesContainer_, 1);

    // =========================
    // Right side: Controls panel
    // =========================
    auto* panel = new QGroupBox("Controls", this);
    auto* pv = new QVBoxLayout(panel);
    pv->setSpacing(8);

    btnStart_ = new QPushButton("Start", this);
    btnEnd_   = new QPushButton("End", this);
    btnReset_ = new QPushButton("Reset", this);

    btnEnd_->setEnabled(false);
    btnReset_->setEnabled(false);

    connect(btnStart_, &QPushButton::clicked, this, &MainWindow::onStart);
    connect(btnEnd_,   &QPushButton::clicked, this, &MainWindow::onEnd);
    connect(btnReset_, &QPushButton::clicked, this, &MainWindow::onReset);

    // ---- Port input ----
    editPort_ = new QLineEdit(this);
    editPort_->setMaximumWidth(160);

    pv->addWidget(new QLabel("Port:", this));
    pv->addWidget(editPort_);
    pv->addSpacing(8);


    spFs_ = new QSpinBox(this);
    spFs_->setRange(50, 32000);
    spFs_->setValue(2000);
    spFs_->setSuffix(" Hz");

    spWinSec_ = new QDoubleSpinBox(this);
    spWinSec_->setRange(0.2, 10.0);
    spWinSec_->setSingleStep(0.2);
    spWinSec_->setValue(2.0);
    spWinSec_->setSuffix(" s");

    spScale_ = new QDoubleSpinBox(this);
    spScale_->setRange(50.0, 10000.0);
    spScale_->setValue(1000.0);
    spScale_->setSuffix(" uV (±)");
    spScale_->setDecimals(0);

    cbEnvelope_ = new QCheckBox("Show Envelope", this);
    cbEnvelope_->setChecked(true);
    connect(cbEnvelope_, &QCheckBox::toggled, this, [this](bool on){
        if (waveCh1_) waveCh1_->setEnvelopeEnabled(on);
        if (waveCh2_) waveCh2_->setEnvelopeEnabled(on);
        if (waveCh3_) waveCh3_->setEnvelopeEnabled(on);
    });

    auto* form = new QGridLayout();
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(6);
    form->addWidget(new QLabel("Sample Rate:", this), 0, 0);
    form->addWidget(spFs_, 0, 1);
    form->addWidget(new QLabel("Window:", this), 1, 0);
    form->addWidget(spWinSec_, 1, 1);
    form->addWidget(new QLabel("Scale:", this), 2, 0);
    form->addWidget(spScale_, 2, 1);

    pv->addWidget(btnStart_);
    pv->addWidget(btnEnd_);
    pv->addWidget(btnReset_);
    pv->addSpacing(8);
    pv->addLayout(form);
    pv->addWidget(cbEnvelope_);
    pv->addSpacing(8);
    pv->addStretch(1);

    // =========================
    // Assemble root layout
    // =========================
    root->addLayout(leftWrap, 1);
    root->addWidget(panel);

    setCentralWidget(central);
    setWindowTitle("EMGReader - 3-Channel UI");
    resize(1300, 820);
}

void MainWindow::setupLogDock()
{
    logDock_ = new QDockWidget("Log", this);
    logDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);

    logView_ = new QPlainTextEdit(this);
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(5000);
    logView_->setPlaceholderText("Logs will appear here...");

    logDock_->setWidget(logView_);

    addDockWidget(Qt::RightDockWidgetArea, logDock_);
}

void MainWindow::appendLog(const QString& line)
{
    if (!logView_) return;

    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    logView_->appendPlainText(QString("[%1] %2").arg(ts, line));
}


void MainWindow::applyDisplaySettings()
{
    const int fs = spFs_->value();
    const double winSec = spWinSec_->value();
    const int winSamples = (int)std::round(winSec * fs);

    auto applyToAll = [&](auto fn){
        if (waveCh1_) fn(waveCh1_);
        if (waveCh2_) fn(waveCh2_);
        if (waveCh3_) fn(waveCh3_);
    };

    applyToAll([&](EmgWaveWidget* w){
        w->setWindowSamples(winSamples);
        w->setScale(spScale_->value());
        w->setEnvelopeEnabled(cbEnvelope_->isChecked());
    });

    lblFs_->setText(QString("%1 Hz").arg(fs));
    lblWin_->setText(QString("%1 s").arg(winSec, 0, 'f', 1));
}

void MainWindow::onStart()
{
    if (running_) return;
    running_ = true;

    btnStart_->setEnabled(false);
    btnEnd_->setEnabled(true);
    btnReset_->setEnabled(true);

    applyDisplaySettings();

    lblState_->setText("Running");
    frames_ = 0;
    fpsClock_.restart();

    const QString port = editPort_->text().trimmed();
    if (port.isEmpty()) {
        appendLog("ERROR: Port is empty.");
        statusBar()->showMessage("Please enter a port name (e.g. COM3)", 5000);
        running_ = false;
        return;
    }

    appendLog(QString("UI: Start acquisition on %1").arg(port));
    emit startSessionRequested(port);
}

void MainWindow::onEnd()
{
    if (!running_) return;
    running_ = false;

    appendLog("UI: Stop acquisition");
    emit stopSessionRequested();

    btnStart_->setEnabled(true);
    btnEnd_->setEnabled(false);
    btnReset_->setEnabled(true);

    lblState_->setText("Stopped");
}


void MainWindow::onReset()
{
    if (waveCh1_) waveCh1_->clear();
    if (waveCh2_) waveCh2_->clear();
    if (waveCh3_) waveCh3_->clear();

    lblRms_->setText("--");
    lblMav_->setText("--");
    lblP2P_->setText("--");
    lblEnv_->setText("--");
    lblFps_->setText("--");

    frames_ = 0;
    fpsClock_.invalidate();

    statusBar()->showMessage("Reset done.", 2000);
}

void MainWindow::onEmgSamples(const QVector<float>& ch1,
                              const QVector<float>& ch2,
                              const QVector<float>& ch3)
{
    if (!running_) return;

    if (waveCh1_) waveCh1_->appendSamples(ch1);
    if (waveCh2_) waveCh2_->appendSamples(ch2);
    if (waveCh3_) waveCh3_->appendSamples(ch3);

    if (!ch1.isEmpty()) {
        const auto m = computeEmgMetrics(ch1);
        lblRms_->setText(QString("%1 uV").arg(m.rms_uV, 0, 'f', 1));
        lblMav_->setText(QString("%1 uV").arg(m.mav_uV, 0, 'f', 1));
        lblP2P_->setText(QString("%1 uV").arg(m.p2p_uV, 0, 'f', 1));
        lblEnv_->setText(QString("%1 uV").arg(m.envelope_uV, 0, 'f', 1));
    }

    frames_++;
    if (!fpsClock_.isValid()) fpsClock_.restart();
    if (fpsClock_.elapsed() >= 500) {
        const double fps = frames_ * 1000.0 / fpsClock_.elapsed();
        lblFps_->setText(QString("%1").arg(fps, 0, 'f', 1));
        frames_ = 0;
        fpsClock_.restart();
    }

    appendLog(QString("I24 EMG: n=%1").arg(ch1.size()));
}


void MainWindow::onBiValue(float ohms)
{
    statusBar()->showMessage(
        QString("BI: %1 Ω").arg(ohms, 0, 'f', 1),
        1000
        );

    appendLog(QString("I24 BI: %1 Ω").arg(ohms, 0, 'f', 1));
}

void MainWindow::onSaveSnapshot()
{
    const QString file = QFileDialog::getSaveFileName(
        this, "Save Snapshot", "emg_snapshot.png", "PNG Images (*.png)");
    if (file.isEmpty()) return;

    // Save the whole 3-channel container as one image
    const QPixmap pix = wavesContainer_ ? wavesContainer_->grab() : this->grab();
    if (!pix.isNull() && pix.save(file)) {
        statusBar()->showMessage("Saved: " + file, 3000);
    } else {
        statusBar()->showMessage("Save failed.", 3000);
    }
}
