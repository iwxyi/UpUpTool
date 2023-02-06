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
    QFile file("update_tool.log");
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
    bool rst = dir.rmpath(dir.absolutePath());
    if (!rst)
        qWarning() << "delete dir failed:" << path;
    return rst;
}

QByteArray getFileMd5(const QString &fileName)
{
    QFile file(fileName);
    const bool isOpen = file.open(QIODevice::ReadOnly);
    if (isOpen)//以只读形式打开文件
    {
        QCryptographicHash hash(QCryptographicHash::Md5);
        while(!file.atEnd())
        {
            QByteArray data = file.read(100 * 1024 * 1024);// 100m  实际内容若不足只读实际大小
            //QByteArray catalog = file.readAll(); // 小文件可以一直全读在内存中，大文件必须分批处理

            hash.addData(data);
        }
        QByteArray md5 = hash.result();
        file.close();//及时关闭
        return md5;
    }
    return QByteArray();
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
            if (override && fileInfo.exists())
            {
                QFile::setPermissions(dstFilePath, QFile::WriteOwner);
                if (QFileInfo(dstFilePath).exists())
                {
                    QFile(dstFilePath).remove();
                }
            }

            if (!QFile::copy(srcFilePath, dstFilePath))
            {
                QByteArray md5_1 = getFileMd5(srcFilePath);
                QByteArray md5_2 = getFileMd5(dstFilePath);
                if (md5_1 == md5_2)
                    qInfo() << "copy skip same file:" << dstFilePath << md5_1.toBase64();
                else
                    qWarning() << "copy failed:" << srcFilePath << "->" << dstFilePath;
            }
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

/// 下载网络文件
/// 会覆盖现有的文件
QByteArray downloadWebFile(QString uri)
{
    QUrl url(uri);
    QNetworkAccessManager manager;
    QEventLoop loop;
    QNetworkReply *reply;
    QNetworkRequest request(url);

    reply = manager.get(request);
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit())); //请求结束并下载完成后，退出子事件循环
    loop.exec(); //开启子事件循环

    return reply->readAll();
}

/// 下载更新的安装包
void actionDownload(QString url, QString filePath)
{
    qInfo() << "download:" << url << "->" << filePath;
    // 判断路径
    QDir().mkpath(QFileInfo(filePath).absolutePath());

    QFile file(filePath);
    if (!file.open(QFile::WriteOnly))
    {
        qCritical() << "unable to write file:" << filePath;
        return ;
    }

    // 下载文件
    QByteArray ba = downloadWebFile(url);
    qInfo() << "file.size:" << ba.size()/1024/1024 << "KB";

    // 写入文件
    file.write(ba);
    file.close();
}

/// 打开程序
/// 也可以是带参数的命令
void actionOpen(QString program, QStringList args)
{
    QProcess process;
    qInfo() << "open file:" << program << args;
    process.startDetached(program, args);
}

/// 解压文件并覆盖（强制覆盖）
void actionUnzip(QString file, QString dir, QStringList args)
{
    qInfo() << "unzip:"<< file << dir << args;
    // 确保待解压文件存在
    if (!QFileInfo(file).exists())
    {
        qCritical() << "file not exist:" << file;
        return ;
    }

    // 延迟解压，等待x秒
    if (args.contains("-1"))
        QThread::sleep(1);
    if (args.contains("-2"))
        QThread::sleep(2);
    if (args.contains("-4"))
        QThread::sleep(4);
    if (args.contains("-8"))
        QThread::sleep(8);

    // 确保目录存在
    QString tempDir = "update_unzip_temp";
    QDir().mkpath(tempDir);
    QDir().mkpath(dir);

    // 开始解压
    QStringList result = JlCompress::extractDir(file, tempDir);
    if (result.size())
    {
        foreach (auto path, result)
        {
            qInfo() << "      " << path;
        }
    }
    else
    {
        qWarning() << "extract failed";
    }

    // 复制文件
    qInfo() << "copy dir:" << tempDir << "->" << dir;
    copyDir(tempDir, dir, true);
    deleteDir(tempDir);

    // 其他参数
    if (args.contains("-d"))
    {
        // 删除压缩包
        qInfo() << "delete zip:" << file;
        QFile(file).remove();
    }
}

/// 压缩文件（强制覆盖）
void actionZip(QString file, QString dir)
{
    qInfo()<< "zip:" << file << dir;
    // 删除已存在的压缩包
    if (!QFileInfo(file).exists())
        QFile(file).remove();

    // 开始压缩
    JlCompress::compressDir(file, dir);
}

/// 重命名文件（强制覆盖）
void actionRename(QString oldFile, QString newFile, QStringList args)
{
    qInfo() << "rename:" << oldFile << newFile << args;
    // 延迟解压，等待x秒
    if (args.contains("-1"))
        QThread::sleep(1);
    if (args.contains("-2"))
        QThread::sleep(2);
    if (args.contains("-4"))
        QThread::sleep(4);
    if (args.contains("-8"))
        QThread::sleep(8);

    // 删除现有文件
    if (QFileInfo(newFile).exists())
        QFile(newFile).remove();

    QFile file(oldFile);
    file.rename(newFile);
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // 输出
    //if (QFileInfo("update_tool.log").exists())
    {
        qInstallMessageHandler(myMsgOutput);
    }

    QStringList args;
    if (argc == 0)
    {
        printf("See Document\n");
        return 0;
    }
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

    // argv[0] 永远都是程序执行文件的路径，argc>=1
    // 从argv[1]开始才是传入的参数
    for (int i = 0; i < argc; i++)
    {
        args.append(QString::fromLocal8Bit(argv[i]));
    }

    QString ac = args[1].toLower();
    args.removeFirst(); // app
    args.removeFirst(); // -action
    argc = args.size(); // 去掉两个之后的size
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
