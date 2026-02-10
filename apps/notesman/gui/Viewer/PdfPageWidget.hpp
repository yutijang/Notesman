#pragma once

#include <cstdint>
#include <memory>
#include <QWidget>
#include <QImage>
#include <QSize>
#include <poppler-page.h>

class PdfPageWidget final : public QWidget {
        Q_OBJECT

    public:
        explicit PdfPageWidget(std::unique_ptr<poppler::page> page, QWidget* parent = nullptr);
        ~PdfPageWidget() override = default;

        enum class RenderQuality : std::uint8_t { High, Fast };

        void renderIfNeeded(double dpi, RenderQuality quality);
        void releaseImage();

        void setScale(double scale);

        [[nodiscard]] double scale() const { return m_scale; }

        [[nodiscard]] QSize baseSize() const { return m_baseSize; }

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        std::unique_ptr<poppler::page> m_page;
        QImage m_image;
        bool m_rendered{};
        double m_currentDpi{0.0};

        QSize m_baseSize;    // size tại dpi gốc
        double m_scale{1.0}; // scale hiển thị hiện tại
};
