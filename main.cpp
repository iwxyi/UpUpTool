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
    qInfo() << "file.size:" << ba.size();

    // 写入文件
    file.write(ba);
    file.close();
}

/// 打开程序
/// 也可以是带参数的命令
void actionOpen(QString program, QStringList args)
{
    QProcess process;
    qInfo() << "open file:" << program;
    process.startDetached(program, args);
}

/// 解压文件并覆盖（强制覆盖）
void actionUnzip(QString file, QString dir, QStringList args)
{
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
    QDir().mkpath(dir);

    // 开始解压
    JlCompress::extractDir(file, dir);

    // 其他参数
    if (args.contains("-d"))
    {
        // 删除压缩包
        QFile(file).remove();
    }
}

/// 压缩文件（强制覆盖）
void actionZip(QString file, QString dir)
{
    // 删除已存在的压缩包
    if (!QFileInfo(file).exists())
        QFile(file).remove();

    // 开始压缩
    JlCompress::compressDir(file, dir);
}

/// 重命名文件（强制覆盖）
void actionRename(QString oldFile, QString newFile, QStringList args)
{
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

    if (is("help"))
    {
        qInfo() << "-d url path       : download net file";
        qInfo() << "-o program [args] : open installation package";
        qInfo() << "-u file dir       : unzip file to dir, append '-d' to remove zip file after the end";
        qInfo() << "-z dir file       : zip one dir to one file";
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

    return 0;
}
