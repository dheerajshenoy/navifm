#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMimeDatabase>
#include <QFile>
#include <QTextEdit>
#include <QtConcurrent/QtConcurrent>
#include <QPromise>
#include <QFutureWatcher>
#include <QStackedWidget>
#include <QTimer>

#include "FilePreviewWorker.hpp"
// #include "TreeSitterTextEdit.hpp"
#include "TextEdit.hpp"
#include "ImageWidget.hpp"

class PreviewPanel : public QStackedWidget {
    Q_OBJECT

signals:
void visibilityChanged(const bool& state);
public:
    PreviewPanel(QWidget *parent = nullptr);
    ~PreviewPanel();

    void loadImage(const QString& filepath);
    void hide() noexcept {
        emit visibilityChanged(false);
        QWidget::hide();
    }

    void show() noexcept {
        emit visibilityChanged(true);
        m_img_widget->clear();
        QWidget::show();
    }

    inline void SetMaxPreviewThreshold(const qint64 &thresh) noexcept {
        m_worker->setMaxPreviewThreshold(thresh);
    }

    inline void SetSyntaxHighlighting(const bool &state) noexcept {
        m_syntax_highlighting_enabled = state;
        if (!state) {
            m_text_preview_widget->setSyntaxHighlighting(false);
        }
    }

    inline void ToggleSyntaxHighlight() noexcept {
        m_syntax_highlighting_enabled = !m_syntax_highlighting_enabled;
        m_text_preview_widget->setSyntaxHighlighting(m_syntax_highlighting_enabled);
    }

    void onFileSelected(const QString &filePath) noexcept;

private:
    void loadImageAfterDelay() noexcept;
    QString readFirstFewLines(const QString &filePath, int lineCount = 5) noexcept;
    QString getMimeType( const QString& filePath ) {
        return QMimeDatabase().mimeTypeForFile( filePath ).name();
    }

    ImageWidget *m_img_widget = new ImageWidget();
    TextEdit *m_text_preview_widget = new TextEdit();
    QWidget *m_empty_widget = new QWidget();

    FilePreviewWorker *m_worker = nullptr;
    QThread *m_workerThread = nullptr;
    QTimer *m_image_preview_timer = nullptr;
    QString m_image_filepath;

    void showImagePreview(const QImage &image) noexcept;
    void showTextPreview(const QString &text,
                         const SyntaxHighlighter::Language &language) noexcept;
    void clearPreview() noexcept;

    bool m_syntax_highlighting_enabled = false;

};