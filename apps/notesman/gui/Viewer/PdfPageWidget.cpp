#include "gui/Viewer/PdfPageWidget.hpp"

#include <QImage>
#include <QPainter>
#include <QSize>
#include <QWidget>
#include <QtAssert>
#include <QtTypes>
#include <cmath>
#include <memory>
#include <poppler-page-renderer.h>
#include <poppler-page.h>
#include <utility>

namespace {

double quantizeDpi(double dpi) noexcept {
    return std::round(dpi / 4.0) * 4.0; // NOLINT(readability-magic-numbers)
}

constexpr double K_PDF_POINTS_PER_INCH = 72.0;
} // namespace

PdfPageWidget::PdfPageWidget(std::unique_ptr<poppler::page> page, QWidget* parent)
    : QWidget(parent), m_page(std::move(page)) {
    Q_ASSERT(m_page);

    auto const rect = m_page->page_rect(); // points (1/72 inch)
    constexpr double dpi = 96.0;
    m_baseSize = QSize(static_cast<int>(rect.width() * dpi / K_PDF_POINTS_PER_INCH),
                       static_cast<int>(rect.height() * dpi / K_PDF_POINTS_PER_INCH));

    setFixedSize(m_baseSize);
}

void PdfPageWidget::setScale(double scale) {
    if (std::abs(m_scale - scale) < 0.001) {
        return;
    } // NOLINT(readability-magic-numbers)

    m_scale = scale;

    QSize const newSize(static_cast<int>(m_baseSize.width() * m_scale),
                        static_cast<int>(m_baseSize.height() * m_scale));

    setFixedSize(newSize);

    m_rendered = false;
    m_currentDpi = 0.0;
}

void PdfPageWidget::renderIfNeeded(double dpi, RenderQuality quality) {
    if (!m_page) {
        return;
    }

    double const targetDpi = quantizeDpi(dpi);

    constexpr double kDpiEpsilon = 0.25;
    if (m_rendered && std::abs(m_currentDpi - targetDpi) < kDpiEpsilon) {
        return;
    }

    if (width() <= 0 || height() <= 0) {
        return;
    }

    poppler::page_renderer renderer;

    if (quality != RenderQuality::Fast) {
        renderer.set_render_hint(poppler::page_renderer::antialiasing, true);
        renderer.set_render_hint(poppler::page_renderer::text_antialiasing, true);
    }

    auto img = renderer.render_page(m_page.get(), targetDpi, targetDpi);
    if (!img.is_valid()) {
        return;
    }

    m_image = QImage(reinterpret_cast<uchar const*>(img.data()),
                     img.width(),
                     img.height(),
                     img.bytes_per_row(),
                     QImage::Format_ARGB32)
                  .copy();

    m_currentDpi = targetDpi;
    m_rendered = true;

    update();
}

void PdfPageWidget::paintEvent(QPaintEvent* /*event*/) {
    if (!m_rendered || m_image.isNull()) {
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.drawImage(rect(), m_image);
}

void PdfPageWidget::releaseImage() {
    if (!m_rendered) {
        return;
    }

    m_image = QImage{};
    m_rendered = false;
    m_currentDpi = 0.0;
}
