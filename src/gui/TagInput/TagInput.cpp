#include "TagInput.hpp"
#include <QHBoxLayout>
#include <QWidget>
#include <QLineEdit>
#include <QFont>
#include <QApplication>
#include <QString>
#include <QPushButton>
#include <QDebug>

TagInput::TagInput(QWidget* parent) : QWidget(parent) { // Implement constructor
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    createInput();
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_layout->setSizeConstraint(QLayout::SetMinimumSize);
    connect(m_input, &QLineEdit::textChanged, this, &TagInput::onTextChanged);
    connect(m_input, &QLineEdit::returnPressed, this, &TagInput::onReturnPressed);
}

void TagInput::createInput() {
    m_input = new QLineEdit(this);
    QFont appFont = QApplication::font();
    m_input->setFont(appFont);
    m_input->setObjectName("tagLineEdit"); // Bước 1: Set name để target specific
    m_input->setPlaceholderText(tr("Enter tags, separated by comma..."));
    m_input->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_layout->addWidget(m_input);

    // Bước 2: Reset và set style chỉ cho widget này (nếu cần custom style)
    m_input->style()->unpolish(m_input); // Reset style hiện tại (optional, nếu conflict)
    m_input->style()->polish(m_input);   // Re-apply Qt default trước custom
    // m_input->setStyleSheet("QLineEdit#tagLineEdit { background: white; border: 1px solid #ababab;
    // "
    //    "padding: 2px 4px; border-radius: 2px; }"); // Style specific
}

void TagInput::onTextChanged(const QString &text) { // Implement slot thiếu
    if (text.endsWith(',')) {
        QString tag = text.left(text.size() - 1).trimmed();
        if (!tag.isEmpty()) {
            addTag(tag);
            m_input->clear();
        } else {
            // Trường hợp người dùng chỉ gõ "," (không có text)
            // Ta xóa dấu phẩy để tránh việc người dùng không thể gõ tag.
            m_input->setText("");
        }
    }
}

void TagInput::addTag(const QString &tag) {
    const QString qssTagChip = R"(
    /* ================================================= */
    /* TRẠNG THÁI MẶC ĐỊNH (BASE STATE)                  */
    /* ================================================= */
    QPushButton {
        /* Gradient 3 màu, hướng từ trái sang phải */
        background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, 
                                    stop: 0 #0072ff, 
                                    stop: 1 #00c6ff);
        color: white; 
        border: 1px solid #0056b3; /* Viền xanh đậm */
        border-radius: 4px;
        padding: 2px 8px; /* Giảm padding cho tag chip nhỏ hơn */
    }
    
    /* ================================================= */
    /* HIỆU ỨNG HOVER (SIMULATED EFFECT)                 */
    /* ================================================= */
    QPushButton:hover {
        /* Thay vì di chuyển gradient (không thể làm trong QSS), 
           chúng ta đảo ngược gradient để tạo cảm giác chuyển đổi. */
        background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, 
                                    stop: 0 #00c6ff, 
                                    stop: 1 #0072ff);
        color: white; 
        text-decoration: none;
    }

    /* ================================================= */
    /* TRẠNG THÁI NHẤN (PRESSED)                         */
    /* ================================================= */
    QPushButton:pressed {
        background: #0056b3; /* Màu solid tối hơn */
    }
)";

    auto* btn = new QPushButton(tag, this); // QPushButton thay QLabel
    QFont appFont = QApplication::font();
    btn->setFont(appFont);
    btn->setStyleSheet(qssTagChip);
    btn->setFlat(true);                                                 // Flat style như chip
    connect(btn, &QPushButton::clicked, this, &TagInput::onTagClicked); // Connect clicked
    m_layout->insertWidget(static_cast<int>(m_tags.size()), btn);
    m_tags.append(btn);
    m_input->setFocus();
    emit tagAdded(tag);
}

void TagInput::onTagClicked() {
    auto* btn = qobject_cast<QPushButton*>(sender()); // Best practice: sender() cast safe
    if (btn != nullptr) {
        auto index = m_tags.indexOf(btn);
        if (index != -1) {
            m_layout->removeWidget(btn);
            btn->deleteLater(); // Qt ownership, an toàn hơn delete
            m_tags.removeAt(index);
        }
    }
}

// Thêm clearTags() nếu chưa (từ câu trước)
void TagInput::clearTags() {                  // Sửa warning
    for (auto* tag : std::as_const(m_tags)) { // Thay qAsConst → std::as_const (C++17)
        m_layout->removeWidget(tag);
        tag->deleteLater();
    }
    m_tags.clear();
    m_layout->invalidate();
    m_input->clear();
}

QStringList TagInput::getAllTags() const {
    QStringList tags;

    for (auto* btn : std::as_const(m_tags)) {
        QString tagText = btn->text();
        if (!tagText.isEmpty()) { tags.append(tagText); }
    }

    QString currentText = m_input->text().trimmed();
    if (!currentText.isEmpty()) {
        QStringList currentTags = currentText.split(',', Qt::SkipEmptyParts);
        for (const QString &tag : currentTags) {
            QString trimmedTag = tag.trimmed();
            if (!trimmedTag.isEmpty()) {
                if (!tags.contains(trimmedTag)) { tags.append(trimmedTag); }
            }
        }
    }

    return tags;
}

void TagInput::onReturnPressed() {
    QString text = m_input->text().trimmed();

    if (!text.isEmpty()) {
        // Tách input bằng dấu phẩy. Nếu không có dấu phẩy, nó sẽ trả về một QStringList chỉ chứa
        // toàn bộ nội dung.
        QStringList tags = text.split(',', Qt::SkipEmptyParts);

        for (const QString &tag : tags) {
            QString trimmedTag = tag.trimmed();
            if (!trimmedTag.isEmpty()) {
                addTag(trimmedTag); // Tạo chip tag
            }
        }

        m_input->clear(); // Xóa nội dung
    }
}

void TagInput::retranslateUi() {
    m_input->setPlaceholderText(tr("Enter tags, separated by comma..."));
}