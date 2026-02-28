#ifndef I24SESSION_H
#define I24SESSION_H

#pragma once
#include <QObject>
#include <QVector>
#include <QThread>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QDateTime>

#include <atomic>
#include <cstdio>

extern "C" {
#include "smpt_client.h"
#include "smpt_dl_client.h"
#include "smpt_dl_definitions_data_types.h"
#include "smpt_client_power.h"
#include "smpt_definitions.h"
#include "smpt_definitions_power.h"
#include "smpt_dl_packet_client.h"
}

class I24Session final : public QObject {
    Q_OBJECT
public:
    explicit I24Session(QObject* parent = nullptr);
    ~I24Session();

public slots:
    void start(const QString& portName);
    void stop();

signals:
    // status
    void statusChanged(const QString& status);
    void errorOccurred(const QString& error);

    // data
    void emgSamplesAvailable(const QVector<float>& ch1,
                             const QVector<float>& ch2,
                             const QVector<float>& ch3);
    void bioImpedanceAvailable(float ohms);

private:
    void run();
    char* get_command_response_text(Smpt_Cmd cmd);
    void wait_for_response(Smpt_device *const device, Smpt_Cmd cmd);
    void fill_dl_power_module_examples(Smpt_device *const device, Smpt_dl_power_module* const dl_power_module);
    void fill_dl_init_examples(Smpt_device *const device, Smpt_dl_init *const dl_init);
    void fill_dl_init_test_signal(Smpt_dl_init *const dl_init);
    void fill_dl_init_bi_simple(Smpt_dl_init *const dl_init);
    void handleLiveData(const Smpt_dl_send_live_data& live);



    std::atomic_bool running_{false};

    Smpt_device device_{};
    uint8_t packetCounter_ = 0;

    uint8_t nextPacketNumber() { return ++packetCounter_; }
    uint8_t lastDlStartPacket_ = 0;

    QFile csvFile_;
    QTextStream csvOut_;
    bool csvEnabled_ = false;

    bool openCsv(const QString& path);
    void closeCsv();
    void writeCsvHeader();
    void writeLiveToCsv(const Smpt_dl_send_live_data& live);

};

#endif // I24SESSION_H
