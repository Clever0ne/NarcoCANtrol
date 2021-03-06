#pragma once

#include <QComboBox>

QT_BEGIN_NAMESPACE

class QIntValidator;

QT_END_NAMESPACE

class BitRateBox : public QComboBox
{
public:
    explicit BitRateBox(QWidget *parent = nullptr);
    ~BitRateBox();

    uint32_t bitRate() const;

    bool isFlexibleDataRateEnabled() const;
    void setFlexibleDataRateEnabled(const bool isEnabled);

private slots:
    void checkCustomSpeedPolicy(const int32_t index);

private:
    void fillBitRates(uint32_t index);

    bool m_isFlexibleDataRateEnabled = false;
    QIntValidator *m_customSpeedValidator = nullptr;
};
