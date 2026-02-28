#include "i24session.h"

#define LIVE_DATA_TIME_OFFSET_EXPECTED  500
#define LIVE_DATA_TIME_OFFSET_EPS       10
#define MAX_LOG_ERRORS                  20

#define MAX_LIVE_DATA_OFFSET  (LIVE_DATA_TIME_OFFSET_EXPECTED + LIVE_DATA_TIME_OFFSET_EPS)
#define MIN_LIVE_DATA_OFFSET  (LIVE_DATA_TIME_OFFSET_EXPECTED - LIVE_DATA_TIME_OFFSET_EPS)

I24Session::I24Session(QObject* parent)
    : QObject(parent) {}

I24Session::~I24Session() {
    stop();
}

void handle_live_data(int *packetCounter,
                      uint8_t *lastPacketNumber,
                      int32_t *countSampleTimeOffsetErrors,
                      int32_t *countPacketNumberErrors,
                      Smpt_dl_send_live_data *const dl_live_data)
{
    int32_t sampleTimeOffset    = dl_live_data->time_offset;
    uint8_t packetNumber        = dl_live_data->packet_number;

    if ((*packetCounter) > 1)
    {
        if (packetNumber != (((*lastPacketNumber) + 1) % Smpt_Packet_Number_Modulo))
        {
            countPacketNumberErrors++;
        }
        if ((sampleTimeOffset < MIN_LIVE_DATA_OFFSET) ||
            (sampleTimeOffset > MAX_LIVE_DATA_OFFSET))
        {
            (*countSampleTimeOffsetErrors)++;
        }
    }
    *lastPacketNumber = packetNumber;
}

void I24Session::start(const QString& portName)
{
    if (running_) {
        return;
    }
    running_ = true;

    Smpt_device *const device = &device_;

    if (!smpt_open_serial_port(&device_, portName.toUtf8().constData())) {
        emit errorOccurred(QString("Failed to open serial port: %1").arg(portName));
        running_ = false;
        return;
    }
    emit statusChanged(QString("Serial port opened: %1").arg(portName));
    openCsv("emg_log.csv");

    /* Init communication */
    Smpt_dl_init                dl_init             = {0};
    Smpt_dl_power_module        dl_power_module     = {0};
    Smpt_dl_send_live_data      dl_live_data        = {0};

    // ---- (1) SET POWER: enable EMG ----
    emit statusChanged(QString("1. send dl_power_module to enable the power supply for the "
                               "\nmeasurement circuits\n\n"));
    fill_dl_power_module_examples(device, &dl_power_module);
    smpt_send_dl_power_module(device, &dl_power_module);
    wait_for_response(device, Smpt_Cmd_Dl_Power_Module_Ack);
    emit statusChanged(QString("You have to wait 2 seconds after receiving the dl_power_module_ack "
                               "\nbecause the circuits need time to get the power\n"));
    QThread::sleep(2);
    emit statusChanged(QString("Waiting done. \n\n"));

    // ---- (2) DL init ----
    emit statusChanged(QString("2. Send dl_init to initialize the measurement circuits with "
                               "\nmeasurement parameters\n"));
    fill_dl_init_examples(device, &dl_init);
    smpt_send_dl_init(device, &dl_init);
    wait_for_response(device, Smpt_Cmd_Dl_Init_Ack);

    // ---- (3) DL start ----
    emit statusChanged(QString("3. Send dl_start to start the measurement. The measurement device "
                               "\nis going to send dl_live_data commands immediately\n"));
    smpt_send_dl_start(device, smpt_packet_number_generator_next(device));
    wait_for_response(device, Smpt_Cmd_Dl_Start_Ack);

    // ---- (4) run loop ----
    run();
}

void I24Session::stop() {
    if (!running_) return;
    running_ = false;

    smpt_send_dl_stop(&device_, nextPacketNumber());
    smpt_close_serial_port(&device_);
    closeCsv();

    emit statusChanged("Measurement stopped");
}

void I24Session::run()
{
    emit statusChanged("Entering run loop...");

    Smpt_device *const device = &device_;
    Smpt_dl_send_live_data dl_send_live_data;
    Smpt_ack ack = {0};
    int packetCounter = 0;
    clock_t beginTime, endTime;
    double timeSpent;
    int n = 1000;

    int32_t countSampleTimeOffsetErrors = 0;
    uint8_t lastPacketNumber = 0;
    int32_t countPacketNumberErrors = 0;

    beginTime = clock();
    while( packetCounter < n)
    {
        while (!smpt_new_packet_received(device)) { }
        packetCounter++;
        smpt_last_ack(device, &ack);

        if (ack.command_number == Smpt_Cmd_Dl_Send_Live_Data)
        {
            smpt_get_dl_send_live_data(device, &dl_send_live_data);
            handle_live_data(&packetCounter,
                             &lastPacketNumber,
                             &countSampleTimeOffsetErrors,
                             &countPacketNumberErrors,
                             &dl_send_live_data);
            writeLiveToCsv(dl_send_live_data);
            handleLiveData(dl_send_live_data);
        }
    }
    endTime = clock();
    timeSpent = (double)(endTime - beginTime) / CLOCKS_PER_SEC;
    printf("\nReceived %d live data samples in %f seconds\n"
           "Frequency is about %f Hz\n"
           "Number of wrong time offsets: %d \n"
           "Number of wrong packet numbers: %d\n\n",
           packetCounter, timeSpent,
           ((double)packetCounter) / timeSpent,
           countSampleTimeOffsetErrors,
           countPacketNumberErrors);

    smpt_close_serial_port(&device_);
    emit statusChanged("Serial port closed");
}

void I24Session::handleLiveData(const Smpt_dl_send_live_data& live)
{
    float emg1_uV = 0.0f;
    float emg2_uV = 0.0f;
    float emg3_uV = 0.0f;

    bool gotEmg1 = false;
    bool gotEmg2 = false;
    bool gotEmg3 = false;

    float bi_ohm = 0.0f;
    bool gotBi = false;

    for (uint8_t i = 0; i < live.n_channels; ++i) {
        const Smpt_dl_electrode_sample& s = live.electrode_samples[i];

        if (!qIsFinite(s.value)) {
            continue;
        }

        switch (s.signal_type) {

        case Smpt_Dl_Signal_Type_BI:
            // BI: milli-ohm → ohm
            bi_ohm = s.value * 0.001f;
            gotBi = true;
            break;

        case Smpt_Dl_Signal_Type_EMG1:
            // EMG1: milli-volt → micro-volt
            emg1_uV = s.value * 1000.0f;
            gotEmg1 = true;
            break;

        case Smpt_Dl_Signal_Type_EMG2:
            // EMG2: milli-volt → micro-volt
            emg2_uV = s.value * 1000.0f;
            gotEmg2 = true;
            break;

        // case Smpt_Dl_Signal_Type_EMG3:
        //     emg3_uV = s.value * 1000.0f;
        //     gotEmg3 = true;
        //     break;

        default:
            emg1_uV = s.value * 1000.0f;
            gotEmg1 = true;
            break;
        }
    }

    if (gotEmg1 || gotEmg2) {
        QVector<float> ch1(1), ch2(1), ch3(1);

        ch1[0] = emg1_uV;
        ch2[0] = emg2_uV;
        ch3[0] = 0;

        emit emgSamplesAvailable(ch1, ch2, ch3);
    }

    if (gotBi) {
        emit bioImpedanceAvailable(bi_ohm);
    }
}

void I24Session::wait_for_response(Smpt_device *const device, Smpt_Cmd cmd)
{
    while (!smpt_new_packet_received(device)) { /* busy waits for response */}

    do
    {
        Smpt_ack ack = {0};
        smpt_last_ack(device, &ack);

        if (ack.command_number == cmd)
        {
            emit statusChanged(QString("Ack Received: %1\n Status: %2\n\n")
                                   .arg(get_command_response_text(cmd))
                                   .arg(ack.result));
            printf("Answer cmd found, cmd: %d %s\n\n", cmd, get_command_response_text(cmd));
            break;
        }
        else
        {
            printf("Unexpected cmd found, cmd: %d %s, \n\n",
                   ack.command_number,
                   get_command_response_text((Smpt_Cmd)ack.command_number));
            printf("Expected cmd was, cmd: %d %s, \n\n",
                   cmd,
                   get_command_response_text(cmd));
            QThread::sleep(2);
        }
    }
    while(smpt_new_packet_received(device));
}

char* I24Session::get_command_response_text(Smpt_Cmd cmd)
{
    char* command_text  = "";
    switch (cmd)
    {
    case Smpt_Cmd_Dl_Init_Ack :
        command_text = "Smpt_Cmd_Dl_Init_Ack";
        break;
    case Smpt_Cmd_Dl_Power_Module_Ack :
        command_text = "Smpt_Cmd_Dl_Power_Module_Ack";
        break;
    case Smpt_Cmd_Dl_Start_Ack :
        command_text = "Smpt_Cmd_Dl_Start_Ack";
        break;
    case Smpt_Cmd_Dl_Stop_Ack :
        command_text = "Smpt_Cmd_Dl_Stop_Ack";
        break;
    case Smpt_Cmd_Dl_Send_Live_Data:
        command_text = "Smpt_Cmd_Dl_Send_Live_Data";
        break;
    default:
        command_text = "Unknown command";
    }

    return command_text;
}

void I24Session::fill_dl_power_module_examples(Smpt_device *const device, Smpt_dl_power_module* const dl_power_module)
{
    dl_power_module->hardware_module = Smpt_Dl_Hardware_Module_Measurement;
    dl_power_module->packet_number = smpt_packet_number_generator_next(device);
    dl_power_module->switch_on_off = 1;
}

void I24Session::fill_dl_init_examples(Smpt_device *const device, Smpt_dl_init *const dl_init)
{
    smpt_clear_dl_init(dl_init);



    /* the call of the next line overwrites the dl_init parameters of the
       call of the previous line. I must do this because out compiler options
       force me to have no unused functions. Feel free to uncomment in your
       code as needed. */

    fill_dl_init_test_signal(dl_init);
    dl_init->packet_number = smpt_packet_number_generator_next(device);

}


bool I24Session::openCsv(const QString& path)
{
    csvFile_.setFileName(path);
    if (!csvFile_.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorOccurred("Failed to open CSV: " + path);
        return false;
    }
    csvOut_.setDevice(&csvFile_);
    csvEnabled_ = true;
    writeCsvHeader();
    return true;
}

void I24Session::closeCsv()
{
    if (!csvEnabled_) return;
    csvOut_.flush();
    csvFile_.close();
    csvEnabled_ = false;
}

void I24Session::writeCsvHeader()
{
    csvOut_ << "host_ts_ms,packet_number,time_offset_us,n_channels";
    for (int i = 0; i < SMPT_DL_MAX_CHANNELS; ++i) {
        csvOut_ << ",ch" << i << "_type"
                << ",ch" << i << "_status"
                << ",ch" << i << "_value";
    }
    csvOut_ << "\n";
    csvOut_.flush();
}

void I24Session::writeLiveToCsv(const Smpt_dl_send_live_data& live)
{
    if (!csvEnabled_) return;

    const qint64 hostTs = QDateTime::currentMSecsSinceEpoch();

    csvOut_ << hostTs
            << "," << (int)live.packet_number
            << "," << (qulonglong)live.time_offset
            << "," << (int)live.n_channels;

    for (int i = 0; i < SMPT_DL_MAX_CHANNELS; ++i) {
        if (i < (int)live.n_channels) {
            const auto& s = live.electrode_samples[i];
            csvOut_ << "," << (int)s.signal_type
                    << "," << (int)s.status
                    << "," << s.value;
        } else {
            csvOut_ << ",0,0,0";
        }
    }
    csvOut_ << "\n";
}

void I24Session::fill_dl_init_test_signal(Smpt_dl_init *const dl_init)
{                                   /* 76543210  One byte */
    dl_init->ads129x.ch1set = 21;   /* 00010101 */
    dl_init->ads129x.ch2set = 21;   /* 00010101 */
    dl_init->ads129x.ch3set = 128;  /* 10000000 */
    dl_init->ads129x.ch4set = 128;  /* 10000000 */
    dl_init->ads129x.ch5set = 0;
    dl_init->ads129x.ch6set = 0;
    dl_init->ads129x.ch7set = 0;
    dl_init->ads129x.config1 = 3;   /* 00000011 */
    dl_init->ads129x.config2 = 16;  /* 00010000 */
    dl_init->ads129x.config3 = 192; /* 11000000 */
    dl_init->ads129x.config4 = 0;
    dl_init->ads129x.gpio    = 0;
    dl_init->ads129x.id      = 0;

    dl_init->ads129x.gpio       = 0;
    dl_init->ads129x.id         = 0;
    dl_init->ads129x.loff       = 0;
    dl_init->ads129x.loff_flip  = 0;
    dl_init->ads129x.loff_sensn = 0;
    dl_init->ads129x.loff_sensp = 0;
    dl_init->ads129x.loff_statn = 0;
    dl_init->ads129x.loff_statp = 0;
    dl_init->ads129x.pace       = 0;
    dl_init->ads129x.resp       = 0; /* 00000000 */
    dl_init->ads129x.rld_sensn  = 0;
    dl_init->ads129x.rld_sensp  = 0;
    dl_init->ads129x.wct1       = 0;
    dl_init->ads129x.wct2       = 0;

    dl_init->sync_signal            = (Smpt_Dl_Init_Sync_Signal)0;
    dl_init->mute_flag              = 0;
    dl_init->sd_storage_mode_flag   = false;
    dl_init->live_data_mode_flag    = true;
}

void I24Session::fill_dl_init_bi_simple(Smpt_dl_init *const dl_init)
{                                   /* 76543210  One byte */

    dl_init->ads129x.ch1set = 16;   /* 00010000 */
    dl_init->ads129x.ch2set = 96;   /* 01100000 */
    dl_init->ads129x.ch3set = 128;  /* 10000000 */
    dl_init->ads129x.ch4set = 0;
    dl_init->ads129x.ch5set = 0;
    dl_init->ads129x.ch6set = 0;
    dl_init->ads129x.ch7set = 0;
    dl_init->ads129x.config1 = 3;   /* 0000011 */
    dl_init->ads129x.config2 = 0;
    dl_init->ads129x.config3 = 192; /* 1100000 */
    dl_init->ads129x.config4 = 0;
    dl_init->ads129x.gpio    = 0;
    dl_init->ads129x.id      = 0;

    dl_init->ads129x.gpio       = 0;
    dl_init->ads129x.id         = 0;
    dl_init->ads129x.loff       = 0;
    dl_init->ads129x.loff_flip  = 0;
    dl_init->ads129x.loff_sensn = 0;
    dl_init->ads129x.loff_sensp = 0;
    dl_init->ads129x.loff_statn = 0;
    dl_init->ads129x.loff_statp = 0;
    dl_init->ads129x.pace       = 0;
    dl_init->ads129x.resp       = 246; /* 11110110 */
    dl_init->ads129x.rld_sensn  = 0;
    dl_init->ads129x.rld_sensp  = 0;
    dl_init->ads129x.wct1       = 0;
    dl_init->ads129x.wct2       = 0;

    dl_init->sync_signal            = (Smpt_Dl_Init_Sync_Signal)0;
    dl_init->mute_flag              = 0;
    dl_init->sd_storage_mode_flag   = false;
    dl_init->live_data_mode_flag    = true;

}
