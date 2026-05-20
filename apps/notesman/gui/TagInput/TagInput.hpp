#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QWidget>

class QHBoxLayout;
class QLineEdit;
class QPushButton;

class TagInput final : public QWidget {
    Q_OBJECT

  public:
    explicit TagInput(QWidget* parent = nullptr);

    void addTag(QString const& tag);
    [[nodiscard]] QStringList getAllTags() const;

    void retranslateUi();

    void clearTags();

  Q_SIGNALS:
    void tagAdded(QString const& tag);

  private:
    void createInput();
    void onTextChanged(QString const& text);
    void onTagClicked();
    void onReturnPressed();

    QHBoxLayout* m_layout{};
    QLineEdit* m_input{};
    QList<QPushButton*> m_tags;
};
