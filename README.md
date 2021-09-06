自动更新工具
===

使发布的程序自动更新，调用命令行（QProcess）即可在程序退出后进行更新操作。

开发环境基于 Qt5.3.1。

## 命令

- `-d url path`：下载网络文件
- `-z zipPath unzipDir`：压缩文件
- `-u zipPath unzipDir`：解压文件；最后面加 `-d` 会在完成后删除压缩包
- `-r oldPath newPath`：重命名文件

### 用法

以绿色版程序 MyApp 为例，在启动时检测更新，调用命令进行下载：

```cpp
QString url = "新版安装包的下载路径";
QString pkgPath = QApplication::applicationDirPath() + "/update.zip";
QProcess process;
QProcess.startDetached("UpUpTool.exe", { "-d", url, pkgPath});
```

在退出程序的时候，检测是否有 `update.zip`，若有，则调用命令进行覆盖 exe：

```cpp
QString appPath = QApplication::applicationDirPath(); // 程序安装路径
QString pkgPath = appPath + "/update.zip"; // 新安装包路径，与下载的位置一致
QProcess process;
process.startDetached("UpUpTool.exe", { "-u", pkgPath, appPath, "-d" } );
```

如果不是绿色版，下载的是 exe 而非 zip，直接打开 exe 让用户进行安装。

也可能是一点小改动，只是修改了发布的 exe 程序，其余 dll 没变，此时只需要重命名下载的 exe：

```cpp
QString pkgPath = QApplication::applicationDirPath() + "/update.exe"; // 新exe路径
QString appPath = QApplication::applicationFilePath(); // 旧exe路径
QProcess process;
process.startDetached("UpUpTool.exe", { "-r", pkgPath, appPath } );
```



