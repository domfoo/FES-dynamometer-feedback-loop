#ifndef EMG_METRICS_H
#define EMG_METRICS_H

#include <QVector>

struct EmgMetrics {
    double rms_uV = 0.0;
    double mav_uV = 0.0;
    double p2p_uV = 0.0;
    double envelope_uV = 0.0;
};

EmgMetrics computeEmgMetrics(const QVector<float>& samples_uV);
#endif // EMG_METRICS_H
