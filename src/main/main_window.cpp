#include "main_window.h"
#include "ui_main_window.h"
#include "settings_dialog.h"
#include "bitrate.h"
#include "../cannabus_library/cannabus_common.h"

#include <QCanBus>
#include <QCanBusFrame>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QTimer>

#define SUPER_CONNECT(sender, signal, receiver, slot) \
connect(sender, &std::remove_pointer<decltype(sender)>::type::signal, \
        receiver, &std::remove_pointer<decltype(receiver)>::type::slot)

#define CONNECT_FILTER(filter_name) SUPER_CONNECT(m_ui->filter##filter_name, stateChanged, this, set##filter_name##Filtrated);

using namespace cannabus;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::MainWindow),
    m_busStatusTimer(new QTimer(this)),
    m_logWindowUpdateTimer(new QTimer(this))

    // ************* Эмуляция общения между ведущим и ведомыми узлами *************

#if (EMULATION == ON)

    , m_sendMessageTimer(new QTimer(this))

#endif

    // ******************* Необходимо удалить после тестирования ******************
{
    m_ui->setupUi(this);

    m_settingsDialog = new SettingsDialog;

    m_status = new QLabel;
    m_ui->statusBar->addPermanentWidget(m_status);

    m_ui->actionDisconnect->setEnabled(false);

    m_ui->logWindow->clearLog();
    m_ui->contentFiltersList->clearList();

    // ************* Эмуляция общения между ведущим и ведомыми узлами *************

#if (EMULATION == ON)

    m_slave.resize(61);

    for (Slave &slave : m_slave)
    {
        slave.reg.fill(0x00, 256);
        slave.data.fill(0x00, 256);
    }

#endif

    // ******************* Необходимо удалить после тестирования ******************

    initActionsConnections();
}

MainWindow::~MainWindow()
{
    delete m_settingsDialog;
    delete m_ui;
}

void MainWindow::initActionsConnections()
{
    SUPER_CONNECT(m_ui->actionConnect, triggered, this, connectDevice);
    SUPER_CONNECT(m_ui->actionDisconnect, triggered, this, disconnectDevice);
    SUPER_CONNECT(m_ui->actionClearLog, triggered, m_ui->logWindow, clearLog);
    SUPER_CONNECT(m_ui->actionQuit, triggered, this, close);
    SUPER_CONNECT(m_ui->actionSettings, triggered, m_settingsDialog, show);
    SUPER_CONNECT(m_ui->actionResetFilterSettings, triggered, this, setDefaultFilterSettings);

    connect(m_settingsDialog, &QDialog::accepted, [this] {
        disconnectDevice();
        connectDevice();
    });

    SUPER_CONNECT(m_ui->filterRegs, editingFinished, this, setContentFiltrated);
    SUPER_CONNECT(m_ui->filterData, editingFinished, this, setContentFiltrated);

    SUPER_CONNECT(m_ui->contentFiltersList, removeFilterAtIndex, m_ui->logWindow, removeContentFilter);

    SUPER_CONNECT(m_ui->filterSlaveAddresses, editingFinished, this, setSlaveAddressesFiltrated);

    // Устанавливаем связь между чекбоксами настроек фильтра типов сообщений и
    // соответствующими методами-сеттерами (см. макросы)
    CONNECT_FILTER(HighPrioMaster);
    CONNECT_FILTER(HighPrioSlave);
    CONNECT_FILTER(Master);
    CONNECT_FILTER(Slave);
    CONNECT_FILTER(AllMsgTypes);

    // Устанавливаем связь между чекбоксами настроек фильтра F-кодов сообщений и
    // соответствующими методами-сеттерами (см. макросы)
    CONNECT_FILTER(ReadRegsRange);
    CONNECT_FILTER(ReadRegsSeries);
    CONNECT_FILTER(WriteRegsRange);
    CONNECT_FILTER(WriteRegsSeries);
    CONNECT_FILTER(DeviceSpecific_1);
    CONNECT_FILTER(DeviceSpecific_2);
    CONNECT_FILTER(DeviceSpecific_3);
    CONNECT_FILTER(DeviceSpecific_4);
    CONNECT_FILTER(AllFCodes);

    // Устанавливаем связь между таймерами и соответствующими событиями
    SUPER_CONNECT(m_busStatusTimer, timeout, this, busStatus);
    SUPER_CONNECT(m_logWindowUpdateTimer, timeout, this, processFramesReceived);

    // ************* Эмуляция общения между ведущим и ведомыми узлами *************

#if (EMULATION == ON)

    SUPER_CONNECT(m_sendMessageTimer, timeout, this, emulateSendMessage);

#endif

    // ******************* Необходимо удалить после тестирования ******************
}

void MainWindow::processError(QCanBusDevice::CanBusError error) const
{
    switch(error)
    {
        case QCanBusDevice::ReadError:
        case QCanBusDevice::WriteError:
        case QCanBusDevice::ConnectionError:
        case QCanBusDevice::ConfigurationError:
        case QCanBusDevice::UnknownError:
        {
            m_status->setText(m_canDevice->errorString());
            break;
        }
        default:
        {
            break;
        }
    }
}

// ***************** Эмуляция общения между ведущим и ведомыми узлами *************

#if (EMULATION == ON)

void MainWindow::emulateSendMessage()
{
    static bool isResponse = false;

    static std::random_device randomNumber;
    static std::mt19937 gen(randomNumber());

    static std::uniform_int_distribution<> slaveAddressRange((uint32_t)IdAddresses::MIN_SLAVE_ADDRESS,
                                                             (uint32_t)IdAddresses::MAX_SLAVE_ADDRESS);

    static std::uniform_int_distribution<> fCodeRange((uint32_t)IdFCode::WRITE_REGS_RANGE,
                                                      (uint32_t)IdFCode::READ_REGS_SERIES);

    static std::uniform_int_distribution<> msgTypeRange((uint32_t)IdMsgTypes::HIGH_PRIO_MASTER,
                                                        (uint32_t)IdMsgTypes::SLAVE);

    static std::uniform_int_distribution<> rangeLenghtRange(2, 6);
    static std::uniform_int_distribution<> seriesLenghtRange(2, 4);

    static std::uniform_int_distribution<> regRange(0x00, 0xFF);
    static std::uniform_int_distribution<> byteRange(0x00, 0xFF);

    static IdAddresses slaveAddress;
    static IdFCode fCode;
    static IdMsgTypes msgType;



    static uint32_t id;

    static QByteArray data;

    if (isResponse == false)
    {
        data.clear();

        slaveAddress = IdAddresses(slaveAddressRange(gen));
        fCode = IdFCode(fCodeRange(gen));
        msgType = IdMsgTypes(msgTypeRange(gen));

        while (msgType == IdMsgTypes::HIGH_PRIO_SLAVE || msgType == IdMsgTypes::SLAVE)
        {
            msgType = IdMsgTypes(msgTypeRange(gen));
        }

        id = makeId(slaveAddress, fCode, msgType);

        switch(fCode)
        {
            case IdFCode::WRITE_REGS_RANGE:
            {
                uint8_t rangeLenght = rangeLenghtRange(gen);

                uint8_t regBegin = regRange(gen);
                uint8_t regEnd = regBegin + (rangeLenght - 1);

                if (regEnd < regBegin)
                {
                    regEnd = regBegin;
                    regBegin = regEnd - (rangeLenght - 1);
                }

                data.append(regBegin);
                data.append(regEnd);

                for (uint32_t byteNumber = 0; byteNumber < rangeLenght; byteNumber++)
                {
                    uint8_t byte = rand();
                    data.append(byte);
                }

                break;
            }
            case IdFCode::WRITE_REGS_SERIES:
            {
                uint8_t seriesLenght = seriesLenghtRange(gen);

                QVector<uint8_t> regAddress;

                for (uint32_t regNumber = 0; regNumber <= seriesLenght; regNumber++)
                {
                    uint8_t reg = regRange(gen);

                    while (regAddress.contains(reg) != false)
                    {
                        reg = regRange(gen);
                    }

                    regAddress.append(reg);;
                }

                for (uint32_t byteNumber = 0; byteNumber < seriesLenght; byteNumber++)
                {
                    uint8_t byte = byteRange(gen);

                    data.append(regAddress.takeFirst());
                    data.append(byte);
                }

                break;
            }
            case IdFCode::READ_REGS_RANGE:
            {
                uint8_t rangeLenght = rangeLenghtRange(gen);

                uint8_t regBegin = regRange(gen);
                uint8_t regEnd = regBegin + (rangeLenght - 1);

                if (regEnd < regBegin)
                {
                    regEnd = regBegin;
                    regBegin = regEnd - (rangeLenght - 1);
                }

                data.append(regBegin);
                data.append(regEnd);

                break;
            }
            case IdFCode::READ_REGS_SERIES:
            {
                uint8_t seriesLenght = seriesLenghtRange(gen);

                QVector<uint8_t> regAddress;

                for (uint32_t regNumber = 0; regNumber <= seriesLenght; regNumber++)
                {
                    uint8_t reg = regRange(gen);

                    while (regAddress.contains(reg) != false)
                    {
                        reg = regRange(gen);
                    }

                    regAddress.append(reg);;
                }

                for (uint32_t byteNumber = 0; byteNumber < seriesLenght; byteNumber++)
                {
                    data.append(regAddress.takeFirst());
                }

                break;
            }
            default:
            {
                break;
            }
        }

        isResponse = true;
    }
    else
    {
        msgType = IdMsgTypes::SLAVE;

        id = makeId(slaveAddress, fCode, msgType);

        switch (fCode)
        {
            case IdFCode::WRITE_REGS_RANGE:
            {
                uint32_t left = static_cast<uint8_t>(data[0]);
                uint32_t right = static_cast<uint8_t>(data[1]);

                for (uint32_t reg = left; reg <= right; reg++)
                {
                    uint32_t index = 2 + reg - left;
                    m_slave[(uint32_t)slaveAddress].data[reg] = static_cast<uint8_t>(data[index]);
                }

                data.clear();

                data.append(left);
                data.append(right);

                break;
            }
            case IdFCode::WRITE_REGS_SERIES:
            {
                QByteArray regAddress;

                for (int32_t index = 0; index < data.size(); index++)
                {
                    uint32_t reg = static_cast<uint8_t>(data[index]);
                    m_slave[(uint32_t)slaveAddress].data[reg] = static_cast<uint8_t>(data[++index]);

                    regAddress.append(reg);
                }

                data.clear();

                data = regAddress;

                break;
            }
            case IdFCode::READ_REGS_RANGE:
            {
                uint32_t left = static_cast<uint8_t>(data[0]);
                uint32_t right = static_cast<uint8_t>(data[1]);

                for (uint32_t reg = left; reg <= right; reg++)
                {
                    data.append(m_slave[(uint32_t)slaveAddress].data[reg]);
                }

                break;
            }
            case IdFCode::READ_REGS_SERIES:
            {
                QByteArray regAddress = data;

                data.clear();

                for (int32_t index = 0; index < regAddress.size(); index++)
                {
                    uint32_t reg = static_cast<uint8_t>(regAddress[index]);

                    data.append(reg);
                    data.append(m_slave[(uint32_t)slaveAddress].data[reg]);
                }

                break;
            }
            case IdFCode::DEVICE_SPECIFIC1:
            case IdFCode::DEVICE_SPECIFIC2:
            case IdFCode::DEVICE_SPECIFIC3:
            case IdFCode::DEVICE_SPECIFIC4:
            default:
            {
                break;
            }
        }

        isResponse = false;
    }

    auto frame = QCanBusFrame(id, data);

    m_queue.enqueue(frame);
}

#endif

// *********************** Необходимо удалить после тестирования ******************

void MainWindow::connectDevice()
{
    // ************* Эмуляция общения между ведущим и ведомыми узлами *************

#if (EMULATION == ON)

    m_ui->actionConnect->setEnabled(false);
    m_ui->actionDisconnect->setEnabled(true);
    m_ui->logWindow->clearLog();

    m_logWindowUpdateTimer->start(log_window_update_timeout);
    m_sendMessageTimer->start(send_message_timeout);
    m_status->setText("Connected");
    return;

#endif

    // ******************* Необходимо удалить после тестирования ******************

    // Получаем указатель на настройки для адаптера
    const auto settings = m_settingsDialog->settings();

    // Сбрасываем настройки адаптера и настраиваем новый
    QString errorString;
    m_canDevice.reset(QCanBus::instance()->createDevice(
                        settings.pluginName,
                        settings.deviceInterfaceName,
                        &errorString));

    // Если не удалось подключиться, выводим ошибку
    if (m_canDevice == nullptr)
    {
        m_status->setText(tr("Error creating device '%1': %2")
                        .arg(settings.pluginName)
                        .arg(errorString));
        return;
    }

    // Очищаем окно лога
    m_ui->logWindow->clearLog();

    // Устанавливаем связь между сигналом возникновения ошибки
    // и функцией-обработчиком ошибок
    connect(m_canDevice.get(), &QCanBusDevice::errorOccurred,
            this, &MainWindow::processError);

    // Настраиваем параметры конфигурации адаптера (битрейт, обратная связь, etc.)
    for (const SettingsDialog::ConfigurationItem &item : qAsConst(settings.configurations))
    {
        m_canDevice->setConfigurationParameter(item.first, item.second);
    }

    // Пробуем подключиться к шине адаптера
    // Если не удалось, выводим ошибку и сбрасываем настройки
    if (m_canDevice->connectDevice() == false)
    {
        m_status->setText(tr("Connection error: %1").arg(m_canDevice->errorString()));

        m_canDevice.reset();

        return;
    }

    // Делаем кнопку Connect недоступной, а Disconnect — доступной
    m_ui->actionConnect->setEnabled(false);
    m_ui->actionDisconnect->setEnabled(true);

    // Определяем битрейт и выводим его в поле статуса подключения
    const QVariant bitRate = m_canDevice->configurationParameter(QCanBusDevice::BitRateKey);
    if (bitRate.isValid() != false)
    {
        const bool isCanFdEnabled = m_canDevice->configurationParameter(QCanBusDevice::CanFdKey).toBool();
        const QVariant dataBitRate = m_canDevice->configurationParameter(QCanBusDevice::DataBitRateKey);

        if (isCanFdEnabled != false && dataBitRate.isValid() != false)
        {
            m_status->setText(tr("Plugin '%1': connected to %2 at %3 / %4 with CAN FD")
                            .arg(settings.pluginName)
                            .arg(settings.deviceInterfaceName)
                            .arg(bitRateToString(bitRate.toUInt()))
                            .arg(bitRateToString(dataBitRate.toUInt())));
        }
        else
        {
            m_status->setText(tr("Plugin '%1': connected to %2 at %3")
                            .arg(settings.pluginName)
                            .arg(settings.deviceInterfaceName)
                            .arg(bitRateToString(bitRate.toUInt())));
        }
    }
    else
    {
        m_status->setText(tr("Plugin '%1': connected to %2")
                        .arg(settings.pluginName)
                        .arg(settings.deviceInterfaceName));
    }

    // Если есть возможность определить статус шины, запускаем таймеры
    // Иначе выводим сообщения о невозможности определить статус шины
    if (m_canDevice->hasBusStatus() != false)
    {
        m_busStatusTimer->start(bus_status_timeout);
        m_logWindowUpdateTimer->start(log_window_update_timeout);
    }
    else
    {
        m_ui->busStatus->setText(tr("No CAN bus status available"));
    }
}

void MainWindow::disconnectDevice()
{
    // ************* Эмуляция общения между ведущим и ведомыми узлами *************

#if (EMULATION == ON)

    m_sendMessageTimer->stop();
    processFramesReceived();

    m_ui->actionConnect->setEnabled(true);
    m_ui->actionDisconnect->setEnabled(false);

    m_status->setText("Disconnected");
    return;

#endif

    // ******************* Необходимо удалить после тестирования ******************

    // Ничего не делаем, если отключать и так нечего
    if (m_canDevice == nullptr)
    {
        return;
    }

    // Останавливаем таймеры
    m_busStatusTimer->stop();
    m_logWindowUpdateTimer->stop();

    // Обрабатываем полученные, но необработанные кадры
    processFramesReceived();

    // Отключаем адаптер
    m_canDevice->disconnectDevice();

    // Выводим сообщение о невозможности определить статус шины
    m_ui->busStatus->setText(tr("No CAN bus status available."));

    // Делаем кнопку Connect доступной, а Disconnect — недоступной
    m_ui->actionConnect->setEnabled(true);
    m_ui->actionDisconnect->setEnabled(false);

    // Выводим в поле статуса сообщение об отключении
    m_status->setText("Disconnected");
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_settingsDialog->close();
    event->accept();
}

void MainWindow::processFramesReceived()
{
    // ************* Эмуляция общения между ведущим и ведомыми узлами *************

#if (EMULATION == ON)

    while (m_queue.isEmpty() == false)
    {
        auto frame = m_queue.dequeue();

        m_ui->logWindow->processDataFrame(frame);
    }

#endif

    // ******************* Необходимо удалить после тестирования ******************

    // Ничего не делаем, если принимать кадры не от кого
    if (m_canDevice == nullptr)
    {
        return;
    }

    // Обрабатываем кадры в цикле
    while (m_canDevice->framesAvailable() != false)
    {
        // Вынимаем кадр из очереди
        const auto frame = m_canDevice->readFrame();

        // Обработка кадров ошибок
        if (frame.frameType() == QCanBusFrame::FrameType::ErrorFrame)
        {
            const QString errorInfo = m_canDevice->interpretErrorFrame(frame);
            m_ui->logWindow->processErrorFrame(frame, errorInfo);

            continue;
        }

        // Обработка обычных кадров
        m_ui->logWindow->processDataFrame(frame);
    }
}

void MainWindow::busStatus()
{
    // Выводим сообщение о невозможности определить статус шины и
    // останавливаем таймеры, если определять статус не у чего или
    // определить его невозможно
    if (m_canDevice == nullptr || m_canDevice->hasBusStatus() == false)
    {
        m_ui->busStatus->setText(tr("No CAN bus status available."));
        m_busStatusTimer->stop();
        m_logWindowUpdateTimer->stop();
        return;
    }

    // Определяем статус шины
    QString status;

    switch(m_canDevice->busStatus())
    {
        case QCanBusDevice::CanBusStatus::Good:
        {
            status = "Good";
            break;
        }
        case QCanBusDevice::CanBusStatus::Warning:
        {
            status = "Warning";
            break;
        }
        case QCanBusDevice::CanBusStatus::Error:
        {
            status = "Error";
            break;
        }
        case QCanBusDevice::CanBusStatus::BusOff:
        {
            status = "Bus Off";
            break;
        }
        default:
        {
            status = "Unknown.";
            break;
        }
    }

    m_ui->busStatus->setText(tr("CAN bus status: %1.").arg(status));
}

void MainWindow::setSlaveAddressesFiltrated()
{
    // Принимаем фильтруемые адреса в виде строки и убираем пробелы
    QString ranges = m_ui->filterSlaveAddresses->text();
    ranges.remove(QChar(' '));

    QVector<uint32_t> slaveAddresses = rangesStringToVector(ranges);
    ranges = rangesVectorToString(slaveAddresses);

    // Обновляем текстовое поле с учётом поправок
    m_ui->filterSlaveAddresses->setText(ranges);

    // Если поле пустое, сбрасываем настройки фильтрации адресов к состоянию по-умолчанию
    if (ranges.isEmpty() != false)
    {
        m_ui->logWindow->fillSlaveAddressSettings(true);
        return;
    }

    // Иначе заполняем адресами из вектора с фильтруемыми адресами
    m_ui->logWindow->fillSlaveAddressSettings(false);

    for (uint32_t slaveAddress : slaveAddresses)
    {
        m_ui->logWindow->setSlaveAddressFiltrated(slaveAddress, true);
    }
}

void MainWindow::setContentFiltrated()
{
    QString regsRange = m_ui->filterRegs->text();
    QString dataRange = m_ui->filterData->text();

    if ((regsRange.isEmpty() && dataRange.isEmpty()) != false)
    {
        return;
    }

    regsRange.remove("0x");
    QVector<uint32_t> regs = rangesStringToVector(regsRange, 16);
    if (regsRange.isEmpty() == false && regs.isEmpty() != false)
    {
        return;
    }

    dataRange.remove("0x");
    QVector<uint32_t> data = rangesStringToVector(dataRange, 16);
    if (dataRange.isEmpty() == false && data.isEmpty() != false)
    {
        return;
    }

    m_ui->logWindow->setContentFiltrated(regs, data);

    m_ui->contentFiltersList->addNewContentFilter(regs, data);

    m_ui->filterRegs->clear();
    m_ui->filterData->clear();
}

QVector<uint32_t> MainWindow::rangesStringToVector(const QString ranges, const int32_t base)
{
    // Разбиваем строку на список подстрок с разделителями в виде запятых
    QStringList range = ranges.split(',', Qt::SkipEmptyParts);
    QVector<uint32_t> data;

    // Разбираем получившиеся подстроки
    for (QString currentRange : range)
    {
        currentRange = range.takeFirst();

        // Проверяем, не является ли подстрока диапазоном
        if (currentRange.contains(QChar('-')) != false)
        {
            // Разбиваем диапазон на левую и правую границу
            QStringList leftAndRight = currentRange.split('-');

            // При base = 0 конвертируем строки в беззнаковые целые числа с учетом соглашений языка Си:
            // Строка начинается с '0x' — шестнадцатеричное число
            // Строка начинается с '0' — восьмеричное число
            // Всё остально — десятичное число
            bool isConverted = true;
            uint32_t left = leftAndRight.takeFirst().toUInt(&isConverted, base);
            uint32_t right = leftAndRight.takeLast().toUInt(&isConverted, base);

            // Если не получилось конвертировать — диапазон в помойку
            if (isConverted == false)
            {
                continue;
            }

            // Если левая граница больше правой, возможно, пользователь всё напутал
            // Любезно поменяем границы местами без его согласия
            if (left > right)
            {
                std::swap(left, right);
            }

            // Добавляем все адреса из диапазона в вектор с фильтруемыми адресами
            for (uint32_t currentData = left; currentData <= right; currentData++)
            {
                data.append(currentData);
            }
        }
        else
        {
            // При base = 0 конвертируем строку в беззнаковое целое число с учетом соглашений языка Си:
            // Строка начинается с '0x' — шестнадцатеричное число
            // Строка начинается с '0' — восьмеричное число
            // Всё остально — десятичное число
            bool isConverted = true;
            uint32_t currentData = currentRange.toUInt(&isConverted, base);

            // Если не получилось конвертировать — адрес в помойку
            if (isConverted == false)
            {
                continue;
            }

            // Добавляем адрес в вектор с фильтруемыми адресами
            data.append(currentData);
        }
    }

    std::sort(data.begin(), data.end());

    return data;
}

QString MainWindow::rangesVectorToString(const QVector<uint32_t> ranges)
{
    QString data;

    if (ranges.isEmpty() != false)
    {
        return data;
    }

    uint32_t left = ranges.first();
    uint32_t right = ranges.first();

    for (uint32_t currentData : ranges)
    {
        if (currentData == right)
        {
            if (currentData != ranges.last())
            {
                continue;
            }
        }

        if (currentData == right + 1)
        {
            right = currentData;
            if (currentData != ranges.last())
            {
                continue;
            }
        }

        if (data.isEmpty() == false)
        {
            data += ", ";
        }

        if (left == right)
        {
            data += tr("%1").arg(left);
        }
        else
        {
            data += tr("%1-%2").arg(left).arg(right);
        }

        left = currentData;
        right = currentData;
    }

    return data;
}

void MainWindow::setAllMsgTypesFiltrated()
{
    const bool isFiltrated = m_ui->filterAllMsgTypes->isChecked();

    m_ui->filterHighPrioMaster->setChecked(isFiltrated);
    m_ui->filterHighPrioSlave->setChecked(isFiltrated);
    m_ui->filterMaster->setChecked(isFiltrated);
    m_ui->filterSlave->setChecked(isFiltrated);
}

void MainWindow::setHighPrioMasterFiltrated()
{
    const bool isFiltrated = m_ui->filterHighPrioMaster->isChecked();

    m_ui->logWindow->setMsgTypeFiltrated(cannabus::IdMsgTypes::HIGH_PRIO_MASTER, isFiltrated);
}

void MainWindow::setHighPrioSlaveFiltrated()
{
    const bool isFiltrated = m_ui->filterHighPrioSlave->isChecked();

    m_ui->logWindow->setMsgTypeFiltrated(cannabus::IdMsgTypes::HIGH_PRIO_SLAVE, isFiltrated);
}

void MainWindow::setMasterFiltrated()
{
    const bool isFiltrated = m_ui->filterMaster->isChecked();

    m_ui->logWindow->setMsgTypeFiltrated(cannabus::IdMsgTypes::MASTER, isFiltrated);
}

void MainWindow::setSlaveFiltrated()
{
    const bool isFiltrated = m_ui->filterSlave->isChecked();

    m_ui->logWindow->setMsgTypeFiltrated(cannabus::IdMsgTypes::SLAVE, isFiltrated);
}

void MainWindow::setAllFCodesFiltrated()
{
    const bool isFiltrated = m_ui->filterAllFCodes->isChecked();

    m_ui->filterWriteRegsRange->setChecked(isFiltrated);
    m_ui->filterWriteRegsSeries->setChecked(isFiltrated);
    m_ui->filterReadRegsRange->setChecked(isFiltrated);
    m_ui->filterReadRegsSeries->setChecked(isFiltrated);
    m_ui->filterDeviceSpecific_1->setChecked(isFiltrated);
    m_ui->filterDeviceSpecific_2->setChecked(isFiltrated);
    m_ui->filterDeviceSpecific_3->setChecked(isFiltrated);
    m_ui->filterDeviceSpecific_4->setChecked(isFiltrated);
}

void MainWindow::setWriteRegsRangeFiltrated()
{
    const bool isFiltrated = m_ui->filterWriteRegsRange->isChecked();

    m_ui->logWindow->setFCodeFiltrated(cannabus::IdFCode::WRITE_REGS_RANGE, isFiltrated);
}

void MainWindow::setWriteRegsSeriesFiltrated()
{
    const bool isFiltrated = m_ui->filterWriteRegsSeries->isChecked();

    m_ui->logWindow->setFCodeFiltrated(cannabus::IdFCode::WRITE_REGS_SERIES, isFiltrated);
}

void MainWindow::setReadRegsRangeFiltrated()
{
    const bool isFiltrated = m_ui->filterReadRegsRange->isChecked();

    m_ui->logWindow->setFCodeFiltrated(cannabus::IdFCode::READ_REGS_RANGE, isFiltrated);
}

void MainWindow::setReadRegsSeriesFiltrated()
{
    const bool isFiltrated = m_ui->filterReadRegsSeries->isChecked();

    m_ui->logWindow->setFCodeFiltrated(cannabus::IdFCode::READ_REGS_SERIES, isFiltrated);
}

void MainWindow::setDeviceSpecific_1Filtrated()
{
    const bool isFiltrated = m_ui->filterDeviceSpecific_1->isChecked();

    m_ui->logWindow->setFCodeFiltrated(cannabus::IdFCode::DEVICE_SPECIFIC1, isFiltrated);
}

void MainWindow::setDeviceSpecific_2Filtrated()
{
    const bool isFiltrated = m_ui->filterDeviceSpecific_2->isChecked();

    m_ui->logWindow->setFCodeFiltrated(cannabus::IdFCode::DEVICE_SPECIFIC2, isFiltrated);
}

void MainWindow::setDeviceSpecific_3Filtrated()
{
    const bool isFiltrated = m_ui->filterDeviceSpecific_3->isChecked();

    m_ui->logWindow->setFCodeFiltrated(cannabus::IdFCode::DEVICE_SPECIFIC3, isFiltrated);
}

void MainWindow::setDeviceSpecific_4Filtrated()
{
    const bool isFiltrated = m_ui->filterDeviceSpecific_4->isChecked();

    m_ui->logWindow->setFCodeFiltrated(cannabus::IdFCode::DEVICE_SPECIFIC4, isFiltrated);
}

void MainWindow::setDefaultFilterSettings()
{
    m_ui->filterSlaveAddresses->setText("");
    m_ui->filterSlaveAddresses->editingFinished();

    m_ui->filterAllMsgTypes->setChecked(false);
    m_ui->filterAllMsgTypes->setChecked(true);

    m_ui->filterAllFCodes->setChecked(false);
    m_ui->filterAllFCodes->setChecked(true);

    m_ui->contentFiltersList->clearList();
}

#undef CONNECT_FILTER
#undef SUPER_CONNECT
