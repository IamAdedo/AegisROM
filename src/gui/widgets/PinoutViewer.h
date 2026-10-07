#pragma once
// PinoutViewer — visual DIP8/SOIC8/SOIC16 pinout widget.
//
// Phase 2 fills this in with IMSProg's SVG/PNG chip rendering assets; the
// geometry here (standard SPI NOR pinout: CS/MISO/WP/HOLD/VCC + GND/MOSI/SCK)
// already matches what the IMSProg artwork depicts, so the swap is graphical.
#include <QWidget>

class PinoutViewer : public QWidget {
  Q_OBJECT
 public:
  enum class Package { Dip8, Soic8, Soic16 };

  explicit PinoutViewer(QWidget* parent = nullptr);

  void setPackage(Package package);
  [[nodiscard]] Package package() const { return package_; }
  void setChipLabel(const QString& label);

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  Package package_ = Package::Soic8;
  QString chipLabel_ = "SPI NOR";
};
