#include "FilePathWidget.hpp"

FilePathWidget::FilePathWidget(QWidget *parent)
: QWidget(parent) {

    m_layout->setContentsMargins(0, 0, 0, 0);
    this->setContentsMargins(0, 0, 0, 0);
    this->setLayout(m_layout);
    m_path_line->setReadOnly(true);
    m_layout->addWidget(m_path_line);
    m_path_line->setFrame(false);

    // Set up file system model for directory completion
    m_completer_model->setFilter(QDir::AllDirs | QDir::Hidden | QDir::NoDotAndDotDot);  // Show only directories
    m_completer_model->setRootPath(QDir::rootPath());
    m_completer = new CustomCompleter(m_completer_model, this);
    m_path_line->setCompleter(m_completer);

    connect(m_path_line, &QLineEdit::returnPressed, this, [&]() {
      m_path_line->setReadOnly(true);
      emit directoryChangeRequested(m_path_line->text());
    });

    this->setFocusPolicy(Qt::FocusPolicy::ClickFocus);

    connect(m_path_line, &FilePathLineEdit::tabPressed, this, [&]() {
        QModelIndex index = m_completer->currentIndex();
        m_completer->popup()->setCurrentIndex(index);
        int row = m_completer->currentRow();
        if (!m_completer->setCurrentRow(row + 1))
            m_completer->setCurrentRow(0);
    });
}

void FilePathWidget::setCurrentDir(const QString& path) noexcept {
    m_path_line->setText(path);
    m_completer_model->setRootPath(path);
}

void FilePathWidget::set_font_family(const QString &fontName) noexcept {
    QFont font = m_path_line->font();
    font.setFamily(fontName);
    m_path_line->setFont(font);
}
