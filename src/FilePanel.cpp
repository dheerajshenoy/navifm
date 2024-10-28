#include "FilePanel.hpp"
#include "FileSystemModel.hpp"

FilePanel::FilePanel(Inputbar *inputBar, QWidget *parent) : QWidget(parent), m_inputbar(inputBar) {
    this->setLayout(m_layout);
    m_layout->addWidget(m_table_view);
    m_table_view->setModel(m_model);
    this->show();

    // m_model->setNameFilterDisables(false);

    if (m_terminal.isEmpty() || m_terminal.isNull())
        m_terminal = "alacritty";

    m_terminal_args = QStringList() << "--working-directory" << "%d";

    initSignalsSlots();
    initKeybinds();
    initContextMenu();

    // Select the first item after the directory has loaded for the first time
    // after the application starts.
    // We disconnect the signal because we do not want to select the first item
    // when we come up the directory and so we handle it manually without the
    // signal
    setAcceptDrops(true);
}

void FilePanel::ResetFilter() noexcept {
    m_model->setNameFilters(QStringList() << "*");
    ForceUpdate();
}

void FilePanel::initContextMenu() noexcept {

    m_context_action_open = new QAction("Open");
    m_context_action_open_with = new QAction("Open With");
    m_context_action_open_terminal = new QAction("Open Terminal Here");
    m_context_action_cut = new QAction("Cut");
    m_context_action_copy = new QAction("Copy");
    m_context_action_paste = new QAction("Paste");
    m_context_action_delete = new QAction("Delete");
    m_context_action_trash = new QAction("Trash");
    m_context_action_properties = new QAction("Properties");

    connect(m_context_action_open_terminal, &QAction::triggered, this,
            [&]() { OpenTerminal(); });

    connect(m_context_action_open, &QAction::triggered, this,
            [&]() { SelectItem(); });

    connect(m_context_action_copy, &QAction::triggered, this, [&]() { CopyItems(); });
    connect(m_context_action_paste, &QAction::triggered, this,
            [&]() { PasteItems(); });

    connect(m_context_action_delete, &QAction::triggered, this,
            [&]() { DeleteItem(); });

    connect(m_context_action_trash, &QAction::triggered, this,
            [&]() { TrashItems(); });

    connect(m_context_action_properties, &QAction::triggered, this,
            [&]() { ItemProperty(); });
}

Result<bool> FilePanel::OpenTerminal(const QString &directory) noexcept {

    if (directory.isEmpty()) {
        QStringList args = m_terminal_args;
        bool res = QProcess::startDetached(m_terminal,
                                           args.replaceInStrings("%d", m_current_dir));
        return Result(res);
    } else {
        if (QDir(directory).exists()) {
            m_terminal_args[0].replace("%d", directory);
            // m_terminal_arg.replace("%d", directory);
            bool res = QProcess::startDetached(m_terminal, m_terminal_args);
            return Result(res);
        }
        return Result(false, "Directory doesn't exist");
    }
}

void FilePanel::initKeybinds() noexcept {}

void FilePanel::initSignalsSlots() noexcept {

    connect(m_model, &FileSystemModel::directoryLoaded, this, [&]() {
      selectFirstItem();
      disconnect(m_model, &FileSystemModel::directoryLoaded, 0, 0);
    });

    connect(m_model, &FileSystemModel::directoryLoadProgress, this,
            [&](const int &progress) {});

    connect(this, &FilePanel::dropCopyRequested, this,
            [&](const QStringList &sourceFilePaths) {
                CopyItems();
                PasteItems();
            });

    connect(this, &FilePanel::dropCutRequested, this,
            [&](const QStringList &sourceFilePaths) {
              CutItems();
              PasteItems();
            });

    connect(m_table_view, &QTableView::doubleClicked, this,

            &FilePanel::handleItemDoubleClicked);

    connect(m_table_view->selectionModel(), &QItemSelectionModel::currentChanged,
            this,
            [this](const QModelIndex &current, const QModelIndex &previous) {
                emit currentItemChanged(
                                        m_current_dir + QDir::separator() +
                                        m_model->data(current, Qt::DisplayRole).toString());
            });

    connect(this, &FilePanel::dropCopyRequested, this,
            &FilePanel::DropCopyRequested);

    connect(this, &FilePanel::dropCutRequested, this,
            &FilePanel::DropCutRequested);

    connect(m_model, &FileSystemModel::directoryLoaded, this, [&]() {
        // if (m_old_item_name.isEmpty() || m_old_item_name.isNull())
        //     return;
        // QModelIndex newIndex = m_model->getIndexFromString(m_old_item_name);
        // if (newIndex.isValid())
        //     m_table_view->setCurrentIndex(newIndex);
    });

    connect(m_model->getFileSystemWatcher(),
            &QFileSystemWatcher::directoryChanged, m_model,
            [&](const QString &path) {
                // QModelIndex oldIndex = m_table_view->currentIndex();
                // m_old_item_name = getCurrentItemBaseName();
                m_model->loadDirectory(path);
            });
}


void FilePanel::DropCopyRequested(const QStringList &sourcePaths) noexcept {
    QString destDir = m_current_dir;

    QThread *thread = new QThread();
    FileWorker *worker = new FileWorker(sourcePaths, destDir, FileOPType::COPY);
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker,
            &FileWorker::performOperation);

    connect(worker, &FileWorker::finished, this,
            [&]() { emit fileOperationDone(true); });

    connect(worker, &FileWorker::error, this,
            [&](const QString &reason) { emit fileOperationDone(false, reason); });
    // connect(worker, &FileWorker::canceled, this,
    // &FileCopyWidget::handleCanceled);

    // TODO: Add Progress
    // connect(worker, &FileCopyWorker::progress, this,
    // &FileCopyWidget::updateProgress);

    connect(worker, &FileWorker::finished, thread, &QThread::quit);
    connect(worker, &FileWorker::finished, worker, &FileWorker::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    thread->start();
}

void FilePanel::DropCutRequested(const QStringList &sourcePaths) noexcept {

    QString destDir = m_current_dir;

    QThread *thread = new QThread();
    FileWorker *worker = new FileWorker(sourcePaths, destDir, FileOPType::CUT);
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &FileWorker::performOperation);

    connect(worker, &FileWorker::finished, this,
            [&]() { emit fileOperationDone(true); });

    connect(worker, &FileWorker::error, this,
            [&](const QString &reason) { emit fileOperationDone(false, reason); });
    // connect(worker, &FileWorker::canceled, this,
    // &FileCopyWidget::handleCanceled);

    connect(worker, &FileWorker::finished, thread, &QThread::quit);
    connect(worker, &FileWorker::finished, worker, &FileWorker::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    thread->start();
}

QString FilePanel::getCurrentItem() noexcept {
    return m_current_dir + QDir::separator() +
           m_model->data(m_table_view->currentIndex()).toString();
}

QString FilePanel::getCurrentItemBaseName() noexcept {
    return m_model->data(m_table_view->currentIndex()).toString();
}

void FilePanel::selectFirstItem() noexcept {
    // QTimer::singleShot(0, [&]() {
    QModelIndex rootIndex = m_table_view->rootIndex();
    m_item_count = m_model->rowCount(rootIndex);

    // Select the first item if available
    if (m_item_count > 0) {
        QModelIndex firstItemIndex = m_model->index(0, 0);
        if (firstItemIndex.isValid()) {
            m_table_view->setCurrentIndex(firstItemIndex);
            m_table_view->scrollTo(firstItemIndex);
            m_table_view->selectionModel()->select(firstItemIndex,
                                                   QItemSelectionModel::Select);
        }
    }
    // Emit signal for number of items in this directory
    emit dirItemCount(m_item_count);
    // });
}

void FilePanel::handleItemDoubleClicked(const QModelIndex &index) noexcept {
    selectHelper(index, true);
}

void FilePanel::setCurrentDir(QString path, const bool &SelectFirstItem) noexcept {
    if (path.isEmpty())
        return;

    if (path.contains("~")) {
        path = path.replace("~", QDir::homePath());
    }

    if (utils::isValidPath(path)) {
        m_model->setRootPath(path);
        m_current_dir = path;

        if (SelectFirstItem)
            selectFirstItem();
        emit afterDirChange(m_current_dir);
    }
}

QString FilePanel::getCurrentDir() noexcept { return m_current_dir; }

FilePanel::~FilePanel() {}

///////////////////////////
// Interactive Functions //
///////////////////////////

void FilePanel::NextItem() noexcept {
    QModelIndex currentIndex = m_table_view->currentIndex();
    int nextRow = currentIndex.row() + 1;

    // Loop to start
    if (nextRow >= m_model->rowCount(m_table_view->rootIndex()))
        nextRow = 0;

    QModelIndex nextIndex = m_model->index(nextRow, 0);
    if (nextIndex.isValid()) {
        m_table_view->setCurrentIndex(nextIndex);
        m_table_view->scrollTo(nextIndex);
    }
    // TODO: Add hook
}

void FilePanel::PrevItem() noexcept {
    QModelIndex currentIndex = m_table_view->currentIndex();
    int prevRow = currentIndex.row() - 1;

    // // Loop
    if (prevRow < 0)
        prevRow = m_model->rowCount(m_table_view->rootIndex()) - 1;

    QModelIndex prevIndex = m_model->index(prevRow, 0);

    if (prevIndex.isValid()) {
        m_table_view->setCurrentIndex(prevIndex);
        m_table_view->scrollTo(prevIndex);
    }
    // TODO: Add hook
}

void FilePanel::selectHelper(const QModelIndex &index, const bool selectFirst) noexcept {
    QString filepath = m_model->filePath(index);
    if (m_model->isDir(index)) {
        setCurrentDir(filepath, selectFirst);
    } else {
        // TODO: handle File
        QDesktopServices::openUrl(QUrl::fromLocalFile(filepath));
    }
    // TODO: Add hook
}

void FilePanel::SelectItem() noexcept {
    QModelIndex currentIndex = m_table_view->currentIndex();
    selectHelper(currentIndex, true);
}

void FilePanel::SelectItemHavingString(const QString &itemName) noexcept {
    QModelIndex currentIndex = m_model->getIndexFromString(itemName);
    selectHelper(currentIndex, true);
}

void FilePanel::UpDirectory() noexcept {
    QString old_dir = m_current_dir;
    QDir currentDir(old_dir);

    if (currentDir.cdUp()) {
        setCurrentDir(currentDir.absolutePath());
        QModelIndex oldDirIndex = m_model->index(old_dir);
        m_table_view->setCurrentIndex(oldDirIndex);
        m_table_view->scrollTo(oldDirIndex);
        emit afterDirChange(m_current_dir);
    }
    // TODO: Add hook
}

void FilePanel::MarkOrUnmarkItems() noexcept {
    auto indexes = m_table_view->selectionModel()->selectedIndexes();

    if (!indexes.isEmpty()) {
        for (auto index : indexes) {
            if (m_model->data(index, static_cast<int>(Role::Marked)).toBool())
                {
                    m_model->setData(index, false, static_cast<int>(Role::Marked));
                    m_model->removeMarkedFile(index);
                } else
                m_model->setData(index, true, static_cast<int>(Role::Marked));
        }
    } else {
        QModelIndex currentIndex = m_table_view->currentIndex();
        if (m_model->data(currentIndex,
                          static_cast<int>(Role::Marked)).toBool()) {
            m_model->setData(currentIndex, false,
                             static_cast<int>(Role::Marked));
            m_model->removeMarkedFile(currentIndex);
        } else
            m_model->setData(currentIndex, true,
                             static_cast<int>(Role::Marked));
    }

}

void FilePanel::MarkItems() noexcept {
    if (m_table_view->selectionModel()->hasSelection()) {
        auto indexes = m_table_view->selectionModel()->selectedIndexes();
        for (auto index : indexes) {
            m_model->setData(index, true, static_cast<int>(Role::Marked));
        }
    } else {
        m_model->setData(m_table_view->currentIndex(), true,
                         static_cast<int>(Role::Marked));
    }
}

void FilePanel::UnmarkItem() noexcept {
    QModelIndex currentIndex = m_table_view->currentIndex();
    m_model->removeMarkedFile(currentIndex);
}

void FilePanel::UnmarkItemsGlobal() noexcept {
    if (m_model->hasMarks())
        m_model->removeMarkedFiles();
}

void FilePanel::UnmarkItemsLocal() noexcept {
    if (m_model->hasMarksLocal())
        m_model->clearMarkedFilesListLocal();
}

void FilePanel::GotoFirstItem() noexcept {
    m_table_view->setCurrentIndex(m_model->index(0, 0));
}

void FilePanel::GotoLastItem() noexcept {
    m_table_view->setCurrentIndex(m_model->index(m_model->rowCount(m_table_view->rootIndex()) - 1, 0));
}

void FilePanel::GotoItem(const uint &itemNum) noexcept {
    if (itemNum >=0 && itemNum <= m_model->rowCount(m_table_view->rootIndex()) - 1)
        m_table_view->setCurrentIndex(m_model->index(itemNum, 0));
}

bool FilePanel::TrashItems() noexcept {
    // TODO: trash items
    return true;
}

void FilePanel::Filters(const QString &filterString) noexcept {
    QStringList filterStringList = filterString.split(" ");
    if (filterStringList.isEmpty())
        filterStringList = {filterString};
    m_model->setNameFilters(filterStringList);
    ForceUpdate();
}

Result<bool> FilePanel::RenameItem() noexcept {
    // current selection single rename
    const QString &oldName = getCurrentItem();
    const QString &oldBaseName = getCurrentItemBaseName();
    QModelIndex currentIndex = m_table_view->currentIndex();
    QString newFileName = m_inputbar->getInput(QString("Rename (%1)")
                                                   .arg(oldBaseName), oldBaseName);

    // If user cancels or the new file name is empty
    if (newFileName.isEmpty() || newFileName.isNull())
        return Result(true, "Operation Cancelled");

    if (newFileName == oldName)
        return Result(false, "File names are same");

    bool state = QFile::rename(oldName,
                               m_current_dir + QDir::separator() + newFileName);
    if (state) {
        m_model->removeMarkedFile(oldName);
    }
    return Result(state);
}

Result<bool> FilePanel::BulkRename(const QStringList &filePaths) noexcept {
    QTemporaryFile tempFile;

    tempFile.setFileTemplate("Navi__Bulk_Rename_List__XXXXXXXXXX");

    if (!tempFile.open()) {
        return Result(false, "Could not create temporary file");
    }

    // Step 2: Write original filenames to temporary file with placeholders for new names
    QTextStream out(&tempFile);

    out << "# This is a comment line\n";
    out << "# Add your new file names to the right of the -> symbol\n";
    out << "# Once done, please save and quit the file\n";
    out << "# The files will hopefully be renamed\n";

    for (const QString &filePath : filePaths) {
        out << filePath << " -> " << "\n";  // Format: original_path -> new_name_placeholder
    }

    tempFile.close();

    // Step 3: Open the temporary file in the default text editor
    QProcess editor;

    editor.start("emacs", QStringList() << tempFile.fileName()); // Replace "gedit" with your text editor of choice
    editor.waitForFinished();

    // Step 4: Reopen the file to read user inputs
    if (!tempFile.open()) {
        QMessageBox::critical(nullptr, "Error", "Could not reopen temporary file.");
        return false;
    }

    // Step 5: Parse each line and rename files
    QTextStream in(&tempFile);
    bool success = true;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.startsWith("#")) continue; // skip "comments"
        QStringList parts = line.split("->", Qt::SkipEmptyParts);
        if (parts.size() == 2) {
            QString originalPath = parts[0].trimmed();
            QString newName = parts[1].trimmed();
            if (!newName.isEmpty()) {
                QString newFilePath = QFileInfo(originalPath).absolutePath() + "/" + newName;
                if (!QFile::rename(originalPath, newFilePath)) {
                    success = false;
                    QMessageBox::warning(nullptr, "Rename Failed",
                                         QString("Failed to rename %1").arg(originalPath));
                }
            }
        }
    }
    tempFile.close();

    return success;

}

Result<bool> FilePanel::RenameItemsGlobal() noexcept { return Result(true); }

Result<bool> FilePanel::RenameItemsLocal() noexcept {
    if (m_model->hasMarksLocal()) {
        auto localMarkedFiles = m_model->getMarkedFilesLocal();
        if (localMarkedFiles.size() > 1) {
            return BulkRename(localMarkedFiles);
        } else {
            QFileInfo fileInfo(localMarkedFiles.at(0));
            const QString &oldFilePath = fileInfo.filePath();
            const QString &oldName = fileInfo.fileName();
            QModelIndex currentIndex = m_table_view->currentIndex();
            QString newFileName = m_inputbar->getInput(QString("Rename (%1)").arg(oldName), oldName);

            // If user cancels or the new file name is empty
            if (newFileName.isEmpty() || newFileName.isNull())
                return Result(true, "Operation Cancelled");

            if (newFileName == oldName)
                return Result(false, "File names are same");

            bool state = QFile::rename(oldFilePath, m_current_dir + QDir::separator() + newFileName);
            if (state) {
                m_model->removeMarkedFile(oldFilePath);
            }

            return Result(state);
        }
    }
    return Result(false, "No marks found");
}

Result<bool> FilePanel::DeleteItem() noexcept {
    if (m_model->isDir(m_table_view->currentIndex())) {
        QDir dir(getCurrentItem());
        int confirm = QMessageBox::question(
                                            this, "Delete", "Do you really want to delete ?",
                                            QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No,
                                            QMessageBox::StandardButton::No);
        switch (confirm) {
        case QMessageBox::Yes:
            m_model->removeMarkedFile(dir.absolutePath());
            return dir.removeRecursively();
            break;

        case QMessageBox::No | QMessageBox::Cancel:
            return false;
            break;
        }
    } else {
        int confirm = QMessageBox::question(
                                            this, "Delete", "Do you really want to delete ?",
                                            QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No,
                                            QMessageBox::StandardButton::No);
        switch (confirm) {
        case QMessageBox::Yes: {
            auto currentItem = getCurrentItem();
            m_model->removeMarkedFile(currentItem);
            return QFile::remove(currentItem);
        }
            break;

        case QMessageBox::No | QMessageBox::Cancel:
            return Result(true, "Deletion cancelled");
            break;
        }
    }
    return Result(false);
}

Result<bool> FilePanel::DeleteItemsGlobal() noexcept {
    return true;
}

Result<bool> FilePanel::DeleteItemsLocal() noexcept {
    bool check = true;
    if (m_model->hasMarksLocal()) {
        auto _removeDir = [&](QDir dir, const QString &itemPath) {
            m_model->removeMarkedFile(itemPath);
            dir.removeRecursively();
        };

        auto _removeFile = [&](const QString &itemPath) {
            m_model->removeMarkedFile(itemPath);
            QFile::remove(itemPath);
        };

        auto markedLocalFiles = m_model->getMarkedFilesLocal();
        for(int i=0; i < markedLocalFiles.size(); i++) {
            QString itemPath = markedLocalFiles.at(i);
            QFileInfo item(itemPath);

            if (item.isDir()) {
                QDir dir(itemPath);

                if (check) {
                    int confirm = QMessageBox::question(
                                                        this, "Delete",
                                                        QString("Do you really want to delete %1?")
                    .arg(item.fileName()),
                                                        QMessageBox::StandardButton::YesToAll |
                                                        QMessageBox::StandardButton::Yes |
                                                        QMessageBox::StandardButton::No,
                                                        QMessageBox::StandardButton::No);
                    switch (confirm) {
                    case QMessageBox::Yes:
                        _removeDir(dir, itemPath);
                        break;

                    case QMessageBox::No | QMessageBox::Cancel:
                        return Result(true, "Cancelled");
                        break;

                    case QMessageBox::YesToAll:
                        check = false;
                    }
                } else {
                    _removeDir(dir, itemPath);
                }
            } else {

                if (check) {
                    int confirm = QMessageBox::question(this, "Delete", "Do you really want to delete ?",
                                                        QMessageBox::StandardButton::YesToAll |
                                                        QMessageBox::StandardButton::Yes |
                                                        QMessageBox::StandardButton::No,
                                                        QMessageBox::StandardButton::No);
                    switch (confirm) {
                    case QMessageBox::Yes:
                        _removeFile(itemPath);
                        break;

                    case QMessageBox::No | QMessageBox::Cancel:
                        return Result(true, "Cancelled");
                        break;

                    case QMessageBox::YesToAll:
                        check = false;
                    }
                } else
                    _removeFile(itemPath);
            }
        }
    }
    return Result(false, "No local marks found");
}

void FilePanel::ToggleHiddenFiles() noexcept {
    m_hidden_files_shown = !m_hidden_files_shown;
    if (m_hidden_files_shown)
        m_model->setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot |
                           QDir::Hidden);
    else
        m_model->setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    ForceUpdate();
}

void FilePanel::Search(const QString &searchExpression) noexcept {
    m_search_index_list = m_model->match(m_model->index(0, 0),
                                         Qt::DisplayRole, searchExpression, -1,
                                         Qt::MatchRegularExpression);
    if (m_search_index_list.isEmpty())
        return;
    m_table_view->setCurrentIndex(m_search_index_list.at(0));
    m_search_index_list_index = 0;
}

void FilePanel::SearchNext() noexcept {
    if (m_search_index_list.isEmpty())
        return;

    m_search_index_list_index++;
    if (m_search_index_list_index > m_search_index_list.size() - 1)
        m_search_index_list_index = 0;

    m_table_view->setCurrentIndex(m_search_index_list.at(m_search_index_list_index));
}

void FilePanel::SearchPrev() noexcept {
    if (m_search_index_list.isEmpty())
        return;

    m_search_index_list_index--;
    if (m_search_index_list_index < 0)
        m_search_index_list_index = m_search_index_list.size() - 1;
    m_table_view->setCurrentIndex(
                                  m_search_index_list.at(m_search_index_list_index));
}

void FilePanel::contextMenuEvent(QContextMenuEvent *event) {
    QMenu menu(this);
    if (m_model->hasMarks() || m_table_view->selectionModel()->hasSelection()) {
        menu.addAction(m_context_action_open);
        menu.addAction(m_context_action_open_terminal);
        menu.addAction(m_context_action_open_with);
        menu.addAction(m_context_action_cut);
        menu.addAction(m_context_action_copy);
        menu.addAction(m_context_action_paste);
        menu.addAction(m_context_action_delete);
        menu.addAction(m_context_action_trash);
        menu.addAction(m_context_action_properties);
    } else {
        menu.addAction(m_context_action_open_terminal);
        menu.addAction(m_context_action_paste);
        menu.addAction(m_context_action_properties);
    }
    menu.exec(event->globalPos());
}

void FilePanel::ForceUpdate() noexcept { setCurrentDir(m_current_dir, true); }

bool FilePanel::SetPermissions(const QString &filePath,
                               const QString &permissionString) noexcept {

    QFile file(filePath);
    QFile::Permissions permissions;

    // Parse owner, group, and others permissions from the string
    int ownerPerm = permissionString[0].digitValue();
    int groupPerm = permissionString[1].digitValue();
    int othersPerm = permissionString[2].digitValue();

    // Owner permissions
    if (ownerPerm & 4)
        permissions |= QFile::ReadOwner;
    if (ownerPerm & 2)
        permissions |= QFile::WriteOwner;
    if (ownerPerm & 1)
        permissions |= QFile::ExeOwner;

    // Group permissions
    if (groupPerm & 4)
        permissions |= QFile::ReadGroup;
    if (groupPerm & 2)
        permissions |= QFile::WriteGroup;
    if (groupPerm & 1)
        permissions |= QFile::ExeGroup;

    // Others permissions
    if (othersPerm & 4)
        permissions |= QFile::ReadOther;
    if (othersPerm & 2)
        permissions |= QFile::WriteOther;
    if (othersPerm & 1)
        permissions |= QFile::ExeOther;

    // Set permissions
    return file.setPermissions(permissions);
}

bool FilePanel::Chmod(const QString &permString) noexcept {
    if (m_model->hasMarks()) {
        // auto indexes = m_table_view->selectionModel()->selectedIndexes();
        // for (auto index : indexes) {
        // }
    } else {
        QString filePath = getCurrentItem();
        return SetPermissions(filePath, permString);
    }

    return false;
}

void FilePanel::PasteItems() noexcept {
    auto markedFiles = m_model->getMarkedFiles();
    if (markedFiles.count() > 0) {
        QString destDir = m_current_dir;

        const QStringList &filesList =
            QStringList(markedFiles.begin(), markedFiles.end());

        QThread *thread = new QThread();
        FileWorker *worker = new FileWorker(filesList, destDir, m_file_op_type);

        connect(thread, &QThread::started, worker,
                &FileWorker::performOperation);

        connect(worker, &FileWorker::finished, this,
                [&]() { emit fileOperationDone(true); });

        connect(worker, &FileWorker::error, this, [&](const QString &reason) {
            emit fileOperationDone(false, reason);
        });

        worker->moveToThread(thread);
        // TODO: Add Progress
        // connect(worker, &FileCopyWorker::progress, this,
        // &FileCopyWidget::updateProgress);

        connect(worker, &FileWorker::finished, thread, &QThread::quit);
        connect(worker, &FileWorker::finished, worker, &FileWorker::deleteLater);
        connect(thread, &QThread::finished, thread, &QThread::deleteLater);

        thread->start();
    }
}

void FilePanel::CopyItems() noexcept {
    m_file_op_type = FileOPType::COPY;
}

void FilePanel::CutItems() noexcept { m_file_op_type = FileOPType::CUT; }


void FilePanel::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void FilePanel::dragMoveEvent(QDragMoveEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void FilePanel::dropEvent(QDropEvent *event) {
    const QMimeData *mimeData = event->mimeData();

    if (mimeData->hasUrls()) {
        QStringList files;
        files.reserve(mimeData->urls().size());

        for (const QUrl &url : mimeData->urls()) {
            QString sourcePath = url.toLocalFile();
            files.append(sourcePath);
        }

        // QString destPath =
        //     QFileInfo(m_model->rootPath()).absoluteFilePath();

        //Move or copy the file
        if (event->proposedAction() == Qt::MoveAction) {
            emit dropCutRequested(files);
        } else if (event->proposedAction() == Qt::CopyAction) {
            emit dropCopyRequested(files);
        }
        event->acceptProposedAction();
    }
}

void FilePanel::startDrag(Qt::DropActions supportedActions) {
    QModelIndexList selectedIndexes = m_table_view->selectionModel()->selectedIndexes();

    if (selectedIndexes.isEmpty())
        return;

    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData;

    if (!m_model)
        return;

    // Collect file paths
    QList<QUrl> urls;
    for (const QModelIndex &index : selectedIndexes) {
        QString filePath = m_model->rootPath() + QDir::separator() +
                           m_model->data(index, Qt::DisplayRole).toString();
        urls << QUrl::fromLocalFile(filePath);
    }

    qDebug() << urls;

    mimeData->setUrls(urls);

    drag->setMimeData(mimeData);
    drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
}

void FilePanel::ItemProperty() noexcept {
    FilePropertyWidget prop_widget(getCurrentItem());
    prop_widget.exec();
}

void FilePanel::MarkInverse() noexcept {}

void FilePanel::MarkAllItems() noexcept {}

bool FilePanel::NewFolder(const QString &folderName) noexcept {
    QDir dir;
    if (dir.mkpath(m_current_dir + QDir::separator() + folderName)) {
        // SelectItemHavingString(folderName);
        return true;
    }
}

bool FilePanel::NewFile(const QString &fileName) noexcept {
    QFile file(m_current_dir + QDir::separator() + fileName);
    if (file.open(QIODevice::WriteOnly)) {
        file.close();
        // SelectItemHavingString(fileName);
        return true;
    }
    return false;
}