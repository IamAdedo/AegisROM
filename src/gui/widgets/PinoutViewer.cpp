// PinoutViewer implementation — QPainter rendering of SPI NOR packages.
#include "PinoutViewer.h"

#include <QPainter>
#include <QPaintEvent>

PinoutViewer::PinoutViewer(QWidget* parent) : QWidget(parent) {
  setMinimumSize(280, 220);
}

void PinoutViewer::setPackage(Package package) {
  package_ = package;
  update();
}

void PinoutViewer::setChipLabel(const QString& label) {
  chipLabel_ = label;
  update();
}

void PinoutViewer::paintEvent(QPaintEvent* event) {
  (void)event;
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const int pins = (package_ == Package::Soic16) ? 16 : 8;
  const int perSide = pins / 2;
  const QRect body(90, 30, 100, 40 + perSide * 18);
  p.setBrush(QColor(35, 38, 37));
  p.setPen(Qt::white);
  p.drawRoundedRect(body, 8, 8);
  p.drawText(body, Qt::AlignCenter, chipLabel_);

  // Standard SPI NOR pin names (left: CS,MISO,WP,HOLD,VCC... right mirrored).
  const char* left8[] = {"CS", "MISO", "WP", "GND"};
  const char* right8[] = {"VCC", "HOLD", "SCK", "MOSI"};
  for (int i = 0; i < perSide; ++i) {
    const int y = body.top() + 18 + i * 18;
    p.setBrush(QColor(106, 201, 255));
    p.drawRect(70, y, 20, 10);               // left pin
    p.drawRect(body.right(), y, 20, 10);      // right pin
    p.setPen(Qt::black);
    if (package_ != Package::Soic16) {
      p.drawText(20, y + 9, QString::fromUtf8(left8[i % 4]));
      p.drawText(body.right() + 24, y + 9, QString::fromUtf8(right8[i % 4]));
    } else {
      p.drawText(20, y + 9, QString("P%1").arg(i + 1));
      p.drawText(body.right() + 24, y + 9, QString("P%1").arg(pins - i));
    }
    p.setPen(Qt::white);
  }
}
