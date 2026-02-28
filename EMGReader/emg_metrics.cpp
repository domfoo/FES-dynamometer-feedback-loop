#include "emg_metrics.h"
#include <cmath>
#include <algorithm>

EmgMetrics computeEmgMetrics(const QVector<float>& x) {
    EmgMetrics m;
    if (x.isEmpty()) return m;

    double sumSq = 0.0;
    double sumAbs = 0.0;
    float minV = x[0], maxV = x[0];

    for (float v : x) {
        sumSq += (double)v * (double)v;
        sumAbs += std::abs((double)v);
        minV = std::min(minV, v);
        maxV = std::max(maxV, v);
    }

    const double n = (double)x.size();
    m.rms_uV = std::sqrt(sumSq / n);
    m.mav_uV = sumAbs / n;
    m.p2p_uV = (double)(maxV - minV);

    const int win = std::min<int>(200, x.size());
    double env = 0.0;
    for (int i = x.size() - win; i < x.size(); ++i) env += std::abs((double)x[i]);
    m.envelope_uV = env / (double)win;

    return m;
}
