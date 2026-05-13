#pragma once

#include "IResourceViewer.hpp"
#include "UiConstants.hpp"
#include "model.hpp"

#include <QString>
#include <cstdint>
#include <memory>

class QWidget;
class ResourceViewService;

class ResourceViewerFactory {
  public:
    ResourceViewerFactory() = delete;

    // path phải là absolute path — caller có trách nhiệm resolve trước khi gọi.
    // Trả về nullptr nếu type không hợp lệ hoặc viewer không khởi tạo được
    // (vd: thiếu WebView2 runtime).
    [[nodiscard]]
    static auto create(std::int64_t id,
                       ResourceType type,
                       QString const& title,
                       QString const& path,
                       QString const& url,
                       UiConst::Theme theme,
                       ResourceViewService& viewService,
                       QWidget* parent) -> std::unique_ptr<IResourceViewer>;
};
