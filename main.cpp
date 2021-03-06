#include <QCoreApplication>
#include <QDebug>
#include <QProcess>
#include <QUrl>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QFile>
#include <QDir>
#include <JlCompress.h>
#include <QThread>
#include <QMutex>

void myMsgOutput(QtMsgType type, const QMessageLogContext &context, const QString& msg)
{
    static QMutex mutex;
    mutex.lock();
    QString time=QDateTime::currentDateTime().toString(QString("[ yyyy-MM-dd HH:mm:ss:zzz ]"));
    QString mmsg;
    switch(type)
    {
    case QtDebugMsg:
        mmsg=QString("%1: Debug:\t%2 (file:%3, line:%4, func: %5)").arg(time).arg(msg).arg(QString(context.file)).arg(context.line).arg(QString(context.function));
        break;
    case QtInfoMsg:
        mmsg=QString("%1: Info:\t%2").arg(time).arg(msg);
        break;
    case QtWarningMsg:
        mmsg=QString("%1: Warning:\t%2 (file:%3, line:%4, func: %5)").arg(time).arg(msg).arg(QString(context.file)).arg(context.line).arg(QString(context.function));
        break;
    case QtCriticalMsg:
        mmsg=QString("%1: Critical:\t%2 (file:%3, line:%4, func: %5)").arg(time).arg(msg).arg(QString(context.file)).arg(context.line).arg(QString(context.function));
        break;
    case QtFatalMsg:
        mmsg=QString("%1: Fatal:\t%2 (file:%3, line:%4, func: %5)").arg(time).arg(msg).arg(QString(context.file)).arg(context.line).arg(QString(context.function));
        abort();
    }
    QFile file("update_history.log");
    file.open(QIODevice::ReadWrite | QIODevice::Append);
    QTextStream stream(&file);
    stream << mmsg << "\r\n";
    file.flush();
    file.close();
    mutex.unlock();
}

bool deleteDir(const QString &path)
{
    if (path.isEmpty())
        return false;

    QDir dir(path);
    if(!dir.exists())
        return true;

    dir.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    QFileInfoList fileList = dir.entryInfoList();
    foreach (QFileInfo fi, fileList)
    {
        if (fi.isFile())
            fi.dir().remove(fi.fileName());
        else
            deleteDir(fi.absoluteFilePath());
    }
    return dir.rmpath(dir.absolutePath());
}

bool copyDir(const QString &source, const QString &destination, bool override = false)
{
    QDir directory(source);
    if (!directory.exists())
    {
        return false;
    }

    QString srcPath = QDir::toNativeSeparators(source);
    if (!srcPath.endsWith(QDir::separator()))
        srcPath += QDir::separator();
    QString dstPath = QDir::toNativeSeparators(destination);
    if (!dstPath.endsWith(QDir::separator()))
        dstPath += QDir::separator();

    QDir root_dir(destination);
    root_dir.mkpath(destination);

    bool error = false;
    QStringList fileNames = directory.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    for (QStringList::size_type i=0; i != fileNames.size(); ++i)
    {
        QString fileName = fileNames.at(i);
        QString srcFilePath = srcPath + fileName;
        QString dstFilePath = dstPath + fileName;
        QFileInfo fileInfo(srcFilePath);
        if (fileInfo.isFile() || fileInfo.isSymLink())
        {
            if (override)
            {
                QFile::setPermissions(dstFilePath, QFile::WriteOwner);
                if (QFileInfo(dstFilePath).exists())
                {
                    QFile(dstFilePath).remove();
                }
            }

            bool rst = QFile::copy(srcFilePath, dstFilePath);
            Q_UNUSED(rst)
            // qInfo() << "copy:" << rst << srcFilePath << dstFilePath;
        }
        else if (fileInfo.isDir())
        {
            QDir dstDir(dstFilePath);
            dstDir.mkpath(dstFilePath);
            if (!copyDir(srcFilePath, dstFilePath, override))
            {
                error = true;
            }
        }
    }

    return !error;
}

/// ??????????????????
/// ????????????????????????
QByteArray downloadWebFile(QString uri)
{
    QUrl url(uri);
    QNetworkAccessManager manager;
    QEventLoop loop;
    QNetworkReply *reply;
    QNetworkRequest request(url);

    reply = manager.get(request);
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit())); //??????????????????????????????????????????????????????
    loop.exec(); //?????????????????????

    return reply->readAll();
}

/// ????????????????????????
void actionDownload(QString url, QString filePath)
{
    qInfo() << "download:" << url << filePath;
    // ????????????
    QDir().mkpath(QFileInfo(filePath).absolutePath());

    QFile file(filePath);
    if (!file.open(QFile::WriteOnly))
    {
        qCritical() << "unable to write file:" << filePath;
        return ;
    }

    // ????????????
    QByteArray ba = downloadWebFile(url);
    qInfo() << "file.size:" << ba.size();

    // ????????????
    file.write(ba);
    file.close();
}

/// ????????????
/// ??????????????????????????????
void actionOpen(QString program, QStringList args)
{
    QProcess process;
    qInfo() << "open file:" << program << args;
    process.startDetached(program, args);
}

/// ???????????????????????????????????????
void actionUnzip(QString file, QString dir, QStringList args)
{
    qInfo() << "unzip:"<< file << dir << args;
    // ???????????????????????????
    if (!QFileInfo(file).exists())
    {
        qCritical() << "file not exist:" << file;
        return ;
    }

    // ?????????????????????x???
    if (args.contains("-1"))
        QThread::sleep(1);
    if (args.contains("-2"))
        QThread::sleep(2);
    if (args.contains("-4"))
        QThread::sleep(4);
    if (args.contains("-8"))
        QThread::sleep(8);

    // ??????????????????
    QString tempDir = "update_unzip_temp";
    QDir().mkpath(tempDir);
    QDir().mkpath(dir);

    // ????????????
    QStringList result = JlCompress::extractDir(file, tempDir);
    if (result.size())
    {
        foreach (auto path, result)
        {
            qInfo() << "    " << path;
        }
    }
    else
    {
        qWarning() << "extract failed";
    }

    // ????????????
    qInfo() << "copy dir:" << tempDir << dir;
    copyDir(tempDir, dir, true);
    deleteDir(tempDir);

    // ????????????
    if (args.contains("-d"))
    {
        // ???????????????
        qInfo() << "delete zip:" << file;
        QFile(file).remove();
    }
}

/// ??????????????????????????????
void actionZip(QString file, QString dir)
{
    qInfo()<< "zip:" << file << dir;
    // ???????????????????????????
    if (!QFileInfo(file).exists())
        QFile(file).remove();

    // ????????????
    JlCompress::compressDir(file, dir);
}

/// ?????????????????????????????????
void actionRename(QString oldFile, QString newFile, QStringList args)
{
    qInfo() << "rename:" << oldFile << newFile << args;
    // ?????????????????????x???
    if (args.contains("-1"))
        QThread::sleep(1);
    if (args.contains("-2"))
        QThread::sleep(2);
    if (args.contains("-4"))
        QThread::sleep(4);
    if (args.contains("-8"))
        QThread::sleep(8);

    // ??????????????????
    if (QFileInfo(newFile).exists())
        QFile(newFile).remove();

    QFile file(oldFile);
    file.rename(newFile);
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // ??????
    if (QFileInfo("update_history.log").exists())
    {
        qInstallMessageHandler(myMsgOutput);
    }

    QStringList args;
    if (argc < 2)
    {
        qCritical() << "need open by console with at least 2 args\ninput -h to get help";
        return -1;
    }
    if (argc < 2)
    {
        qCritical() << "need at least 2 args";
        return -1;
    }

    // argv[0] ??????????????????????????????????????????argc>=1
    // ???argv[1]???????????????????????????
    for (int i = 0; i < argc; i++)
    {
        args.append(QString::fromLocal8Bit(argv[i]));
    }

    QString ac = args[1].toLower();
    args.removeFirst(); // app
    args.removeFirst(); // -action
    argc = args.size(); // ?????????????????????size
    if (ac.startsWith("-"))
        ac = ac.right(ac.length() - 1);
    auto is = [=](QString action) -> bool {
        return ac == action || ac == action.at(0);
    };

    auto showHelp = [=]{
        qInfo() << "-d url path       : download net file";
        qInfo() << "-o program [args] : open installation package";
        qInfo() << "-u file dir       : unzip file to dir, append '-d' to remove zip file after the end";
        qInfo() << "-z dir file       : zip one dir to one file";
    };

    if (is("help"))
    {
        showHelp();
        return 0;
    }
    else if (is("download"))
    {
        if (argc < 2)
        {
            qCritical() << "need 3 args: -d url path";
            return -1;
        }

        actionDownload(args[0], args[1]);
    }
    else if (is("open"))
    {
        if (argc < 1)
        {
            qCritical() << "need least 2 args: -o program [args]";
            return -1;
        }

        auto program = args[0];
        auto sl = args;
        sl.removeFirst(); // program
        actionOpen(program, sl);
    }
    else if (is("unzip"))
    {
        if (argc < 2)
        {
            qCritical() << "need 3 args: -u zipFile unzipDir";
            return -1;
        }

        auto sl = args;
        sl.removeFirst();
        sl.removeFirst();
        actionUnzip(args[0], args[1], sl);
    }
    else if (is("zip"))
    {
        if (argc < 2)
        {
            qCritical() << "need least 3 args: -o zipFile unzipDir";
            return -1;
        }

        actionZip(args[0], args[1]);
    }
    else if (is("rename"))
    {
        if (argc < 2)
        {
            qCritical() << "need least 3 args: -r oldPath newPath";
            return -1;
        }

        auto sl = args;
        sl.removeFirst();
        sl.removeFirst();
        actionRename(args[0], args[1], sl);
    }
    else
    {
        showHelp();
    }

    return 0;
}
