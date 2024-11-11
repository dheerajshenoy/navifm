#pragma once

#include <QObject>
#include <QString>
#include <QUuid>
#include <QProcess>
#include <QTemporaryFile>
#include <QFileSystemWatcher>
#include <QDebug>

class Task : public QObject {
    Q_OBJECT

  public:
    Task();
    enum class TaskType { COMMAND = 0, COPY, MOVE, DELETE };
    void setTaskType(const TaskType &type) noexcept;
    void setCommandString(const QString &command, const QStringList &args) noexcept;
    void run() noexcept;
    const QUuid uuid() const noexcept { return m_uuid; }
    void stop() noexcept;
    TaskType type() const { return m_type; }
    QString commandString() const {
      return m_command + " " + m_command_args_list.join(" ");
    }

    signals:
    void progress(const int &);
    void finished(const QUuid &uuid);
    void errorOccured(const QString &reason);
    void stdout(const QString &output);
    void stderr(const QString &error);

private:
    void runCommand() noexcept;
    QUuid m_uuid;
    TaskType m_type;
    QString m_command;
    QStringList m_command_args_list;
    QProcess *process;
};