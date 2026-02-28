#include "emg_wave_widget.h"
#include <QPainter>
#include <algorithm>
#include <cmath>

EmgWaveWidget::EmgWaveWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(800, 360);
    ring_.reserve(windowSamples_);
    envelope_.reserve(windowSamples_);
}

void EmgWaveWidget::setWindowSamples(int n) {
    windowSamples_ = std::max(200, n);
    if (ring_.size() > windowSamples_) {
        ring_.erase(ring_.begin(), ring_.begin() + (ring_.size() - windowSamples_));
        envelope_.erase(envelope_.begin(), envelope_.begin() + (envelope_.size() - windowSamples_));
    }
    redraw();
    update();
}

void EmgWaveWidget::setScale(double uVPerFullScale) {
    fullScale_uV_ = std::max(1.0, uVPerFullScale);
    redraw();
    update();
}

void EmgWaveWidget::setEnvelopeEnabled(bool on) {
    envelopeEnabled_ = on;
    redraw();
    update();
}

QImage EmgWaveWidget::snapshot() const {
    return img_;
}

void EmgWaveWidget::appendSamples(const QVector<float>& samples_uV) {
    if (samples_uV.isEmpty()) return;

    for (float v : samples_uV) ring_.push_back(v);

    float env = envelope_.isEmpty() ? 0.0f : envelope_.back();
    const float alpha = 0.06f;
    for (float v : samples_uV) {
        const float a = std::abs(v);
        env = (1.0f - alpha) * env + alpha * a;
        envelope_.push_back(env);
    }

    if ((int)ring_.size() > windowSamples_) {
        const int drop = (int)ring_.size() - windowSamples_;
        ring_.erase(ring_.begin(), ring_.begin() + drop);
    }
    if ((int)envelope_.size() > windowSamples_) {
        const int drop = (int)envelope_.size() - windowSamples_;
        envelope_.erase(envelope_.begin(), envelope_.begin() + drop);
    }

    redraw();
    update();
}

void EmgWaveWidget::resizeEvent(QResizeEvent*) {
    redraw();
}

void EmgWaveWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    if (!img_.isNull()) p.drawImage(0, 0, img_);
}

void EmgWaveWidget::redraw() {
    const int w = std::max(1, width());
    const int h = std::max(1, height());
    img_ = QImage(w, h, QImage::Format_RGB32);
    img_.fill(Qt::black);

    QPainter p(&img_);
    p.setRenderHint(QPainter::Antialiasing, true);

    p.setPen(QPen(QColor(50, 50, 50), 1));
    p.drawLine(0, h/2, w-1, h/2);
    for (int i=1; i<8; ++i) {
        const int y = i * h / 8;
        p.drawLine(0, y, w-1, y);
    }

    if (ring_.size() < 2) return;

    const int n = ring_.size();
    auto mapY = [&](float uV) -> int {
        const double norm = (double)uV / fullScale_uV_; // [-1,1] 近似
        const double y = (0.5 - 0.45 * norm) * (h - 1);
        return (int)std::clamp(y, 0.0, (double)(h-1));
    };

    QPolygon poly;
    poly.reserve(n);
    for (int i=0; i<n; ++i) {
        const double x = (double)i / (double)(n-1) * (w-1);
        poly << QPoint((int)x, mapY(ring_[i]));
    }
    p.setPen(QPen(Qt::green, 2));
    p.drawPolyline(poly);

    if (envelopeEnabled_ && envelope_.size() == ring_.size()) {
        QPolygon envUp, envDn;
        envUp.reserve(n);
        envDn.reserve(n);
        for (int i=0; i<n; ++i) {
            const double x = (double)i / (double)(n-1) * (w-1);
            envUp << QPoint((int)x, mapY(+envelope_[i]));
            envDn << QPoint((int)x, mapY(-envelope_[i]));
        }
        p.setPen(QPen(QColor(255, 200, 0), 1));
        p.drawPolyline(envUp);
        p.drawPolyline(envDn);
    }

    p.setPen(QPen(QColor(180, 180, 180), 1));
    p.drawText(10, 20, QString("Scale: ±%1 uV").arg(fullScale_uV_, 0, 'f', 0));
}

void EmgWaveWidget::clear() {
    ring_.clear();
    envelope_.clear();
    img_ = QImage();
    update();
}
