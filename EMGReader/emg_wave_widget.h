#ifndef EMG_WAVE_WIDGET_H
#define EMG_WAVE_WIDGET_H

#pragma once
#include <QWidget>
#include <QImage>
#include <QVector>

class EmgWaveWidget final : public QWidget {
    Q_OBJECT
public:
    explicit EmgWaveWidget(QWidget* parent=nullptr);

    void setWindowSamples(int n);
    void setScale(double uVPerFullScale);
    QImage snapshot() const;

public slots:
    void appendSamples(const QVector<float>& samples_uV);
    void setEnvelopeEnabled(bool on);
    void clear();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    void redraw();

    QImage img_;
    QVector<float> ring_;
    QVector<float> envelope_;
    int windowSamples_ = 4000;
    double fullScale_uV_ = 1000.0;
    bool envelopeEnabled_ = true;
};

#endif // EMG_WAVE_WIDGET_H
