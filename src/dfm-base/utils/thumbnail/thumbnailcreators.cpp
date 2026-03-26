// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "thumbnailcreators.h"
#include "thumbnailhelper.h"

#include <dfm-base/utils/fileutils.h>
#include <dfm-base/mimetype/dmimedatabase.h>
#include <dfm-base/base/schemefactory.h>

#include <dfm-io/dfmio_utils.h>

#include <DThumbnailProvider>

#include <QProcess>
#include <QFont>
#include <QPen>
#include <QPainter>
#include <QImageReader>
#include <QDebug>
#include <QTemporaryDir>

// use original poppler api
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-image.h>
#include <poppler/cpp/poppler-page.h>
#include <poppler/cpp/poppler-page-renderer.h>

static constexpr char kFormat[] { ".png" };

using namespace dfmbase;
DFMGLOBAL_USE_NAMESPACE

QImage ThumbnailCreators::defaultThumbnailCreator(const FileInfoPointer &info, ThumbnailSize size, const std::atomic_bool *stoped)
{
    if (info.isNull()) {
        qCWarning(logDFMBase) << "ThumbnailCreators::defaultThumbnailCreator file info is nullptr !";
        return {};
    }
    const auto &path = info->pathOf(PathInfoType::kAbsoluteFilePath);
    QFileInfo qInf(path);
    auto sz = static_cast<DTK_GUI_NAMESPACE::DThumbnailProvider::Size>(size);
    if (stoped && (*stoped).load(std::memory_order_release))
        return {};
    QString thumbPath = DTK_GUI_NAMESPACE::DThumbnailProvider::instance()->createThumbnail(qInf, sz);
    if (thumbPath.isEmpty()) {
        qCWarning(logDFMBase) << "thumbnail: cannot generate thumbnail by default creator for" << path;
        qCWarning(logDFMBase) << "thumbnail:" << DTK_GUI_NAMESPACE::DThumbnailProvider::instance()->errorString();
        return {};
    }

    return QImage(thumbPath);
}

QImage ThumbnailCreators::videoThumbnailCreator(const FileInfoPointer &info, ThumbnailSize size, const std::atomic_bool *stoped)
{
    if (info.isNull()) {
        qCWarning(logDFMBase) << "ThumbnailCreators::videoThumbnailCreator file info is nullptr !";
        return {};
    }
    QImage img = videoThumbnailCreatorLib(info, size, stoped);
    if (img.isNull()) {
        qCWarning(logDFMBase) << "thumbnail: create video's thumbnail by lib failed, try ffmpeg" << info->fileUrl();
        if (stoped && (*stoped).load(std::memory_order_release))
            return img;
        img = videoThumbnailCreatorFfmpeg(info, size, stoped);
    }

    return img;
}

QImage ThumbnailCreators::videoThumbnailCreatorFfmpeg(const FileInfoPointer &info, ThumbnailSize size, const std::atomic_bool *stoped)
{
    if (info.isNull()) {
        qCWarning(logDFMBase) << "ThumbnailCreators::videoThumbnailCreatorFfmpeg file info is nullptr !";
        return {};
    }
    const auto &path = info->pathOf(PathInfoType::kAbsoluteFilePath);
    QProcess ffmpeg;
    QStringList args { "-nostats", "-loglevel", "0", "-i", path,
                       "-vf", QString("scale='min(%1, iw)':-1").arg(size), "-f",
                       "image2pipe", "-vcodec", "png", "-fs", "9000", "-" };
    ffmpeg.start("ffmpeg", args, QIODevice::ReadOnly);

    QImage img;
    if (!ffmpeg.waitForFinished()) {
        qCWarning(logDFMBase) << "thumbnail: ffmpeg execute failed: "
                   << ffmpeg.errorString()
                   << info;
        return img;
    }

    const auto &data = ffmpeg.readAllStandardOutput();
    if (data.isEmpty())
        return img;

    if (stoped && (*stoped).load(std::memory_order_release))
        return {};

    QString outputs(data);
    if (!img.loadFromData(data)) {   // filter the outputs outputed by video tool.
        QStringList data = outputs.split(QRegExp("[\n]"), QString::SkipEmptyParts);
        if (data.isEmpty())
            return img;

        outputs = data.last();
    }

    if (!img.loadFromData(outputs.toLocal8Bit().data(), "png"))
        qCWarning(logDFMBase) << "thumbnail: cannot load image from ffmpeg outputs." << path;
    return img;
}

QImage ThumbnailCreators::videoThumbnailCreatorLib(const FileInfoPointer &info, ThumbnailSize size, const std::atomic_bool *stoped)
{
    Q_UNUSED(size)

    if (info.isNull()) {
        qCWarning(logDFMBase) << "ThumbnailCreators::videoThumbnailCreatorLib file info is nullptr !";
        return {};
    }
    static QLibrary lib("libimageviewer.so");
    QImage img;

    const auto &path = info->pathOf(PathInfoType::kAbsoluteFilePath);

    if (lib.isLoaded() || lib.load()) {
        typedef void (*GetMovieCover)(const QUrl &, const QString &, QImage *);
        GetMovieCover func = reinterpret_cast<GetMovieCover>(lib.resolve("getMovieCover"));

        if (stoped && (*stoped).load(std::memory_order_release))
            return {};
        if (func)
            func(QUrl::fromLocalFile(path), path, &img);
    }

    return img;
}

QImage ThumbnailCreators::textThumbnailCreator(const FileInfoPointer &info, ThumbnailSize size, const std::atomic_bool *stoped)
{
    if (info.isNull()) {
        qCWarning(logDFMBase) << "ThumbnailCreators::textThumbnailCreator file info is nullptr !";
        return {};
    }
    const auto &path = info->pathOf(PathInfoType::kAbsoluteFilePath);
    QImage img;
    DFMIO::DFile dfile(path);
    if (!dfile.open(DFMIO::DFile::OpenFlag::kReadOnly)) {
        qCWarning(logDFMBase) << "thumbnail: can not open this file." << path;
        return img;
    }

    QString text { FileUtils::toUnicode(dfile.read(2000), info->nameOf(NameInfoType::kFileName)) };
    if (stoped && (*stoped).load(std::memory_order_release))
        return {};
    QFont font;
    font.setPixelSize(12);

    QPen pen;
    pen.setColor(Qt::black);

    img = QImage(static_cast<int>(0.70707070 * size), size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);

    QPainter painter(&img);
    painter.setFont(font);
    painter.setPen(pen);

    if (stoped && (*stoped).load(std::memory_order_release))
        return {};
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    painter.drawText(img.rect(), text, option);

    return img;
}

QImage ThumbnailCreators::audioThumbnailCreator(const FileInfoPointer &info, ThumbnailSize size, const std::atomic_bool *stoped)
{
    if (info.isNull()) {
        qCWarning(logDFMBase) << "ThumbnailCreators::audioThumbnailCreator file info is nullptr !";
        return {};
    }
    const auto &path = info->pathOf(PathInfoType::kAbsoluteFilePath);
    QProcess ffmpeg;
    QStringList args { "-nostats", "-loglevel", "0", "-i", path,
                       "-an", "-vf", QString("scale='min(%1, iw)':-1").arg(size), "-f", "image2pipe", "-fs", "9000", "-" };
    ffmpeg.start("ffmpeg", args, QIODevice::ReadOnly);

    QImage img;
    if (stoped && (*stoped).load(std::memory_order_release))
        return img;

    if (!ffmpeg.waitForFinished()) {
        qCWarning(logDFMBase) << "thumbnail: ffmpeg execute failed: "
                   << ffmpeg.errorString()
                   << path;

        return img;
    }

    if (stoped && (*stoped).load(std::memory_order_release))
        return {};
    const QByteArray &output = ffmpeg.readAllStandardOutput();
    if (!img.loadFromData(output))
        qCWarning(logDFMBase) << "thumbnail: cannot load image from ffmpeg outputs." << path;

    return img;
}

QImage ThumbnailCreators::imageThumbnailCreator(const FileInfoPointer &info, ThumbnailSize size, const std::atomic_bool *stoped)
{
    //! fix bug#49451 因为使用mime.preferredSuffix(),会导致后续image.save崩溃，具体原因还需进一步跟进
    //! QImageReader构造时不传format参数，让其自行判断
    //! fix bug #53200 QImageReader构造时不传format参数，会造成没有读取不了真实的文件 类型比如将png图标后缀修改为jpg，读取的类型不对
    if (info.isNull()) {
        qCWarning(logDFMBase) << "ThumbnailCreators::imageThumbnailCreator file info is nullptr !";
        return {};
    }

    const auto &path = info->pathOf(PathInfoType::kAbsoluteFilePath);
    QString mimeType = DMimeDatabase().mimeTypeForFile(QUrl::fromLocalFile(path), QMimeDatabase::MatchContent).name();
    const QString &suffix = mimeType.replace("image/", "");

    QImageReader reader(path, suffix.toLatin1());
    if (!reader.canRead()) {
        qCWarning(logDFMBase) << "thumbnail: can not read this file:"
                   << reader.errorString()
                   << path;
        return {};
    }

    if (stoped && (*stoped).load(std::memory_order_release))
        return {};

    const QSize &imageSize = reader.size();

    //fix 读取损坏icns文件（可能任意损坏的image类文件也有此情况）在arm平台上会导致递归循环的问题
    //这里先对损坏文件（imagesize无效）做处理，不再尝试读取其image数据
    if (!imageSize.isValid()) {
        qCWarning(logDFMBase) << "thumbnail: fail to read image file attribute data." << info;
        return {};
    }

    const QString &defaultMime = DMimeDatabase().mimeTypeForFile(QUrl::fromLocalFile(path)).name();
    if (imageSize.width() > size || imageSize.height() > size || defaultMime == DFMGLOBAL_NAMESPACE::Mime::kTypeImageSvgXml)
        reader.setScaledSize(reader.size().scaled(size, size, Qt::KeepAspectRatio));

    if (stoped && (*stoped).load(std::memory_order_release))
        return {};
    reader.setAutoTransform(true);
    QImage image;
    if (!reader.read(&image)) {
        qCWarning(logDFMBase) << "thumbnail: read failed."
                   << reader.errorString()
                   << path;
        return image;
    }

    return image;
}

QImage ThumbnailCreators::djvuThumbnailCreator(const FileInfoPointer &info, ThumbnailSize size, const std::atomic_bool *stoped)
{
    if (info.isNull()) {
        qCWarning(logDFMBase) << "ThumbnailCreators::djvuThumbnailCreator file info is nullptr !";
        return {};
    }

    QImage img = defaultThumbnailCreator(info, size);
    if (!img.isNull())
        return img;

    const auto &path = info->pathOf(PathInfoType::kAbsoluteFilePath);
    const QString &readerBinary = QStandardPaths::findExecutable("deepin-reader");
    if (readerBinary.isEmpty())
        return img;
    if (stoped && (*stoped).load(std::memory_order_release))
        return {};
    //! 使用子进程来调用deepin-reader程序生成djvu格式文件缩略图
    QProcess process;
    QStringList arguments;
    //! 生成缩略图缓存地址
    const QString &fileUrl = QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded);
    const QString &thumbnailName = ThumbnailHelper::dataToMd5Hex(fileUrl.toLocal8Bit()) + kFormat;
    const QString &saveImage = DFMIO::DFMUtils::buildFilePath(ThumbnailHelper::sizeToFilePath(size).toStdString().c_str(),
                                                              thumbnailName.toStdString().c_str(), nullptr);
    arguments << "--thumbnail"
              << "-f" << path << "-t" << saveImage;
    process.start(readerBinary, arguments);

    if (!process.waitForFinished() || process.exitCode() != 0) {
        qCWarning(logDFMBase) << "thumbnail: deepin-reader execute failed:"
                   << process.errorString()
                   << path;

        return img;
    }

    if (stoped && (*stoped).load(std::memory_order_release))
        return {};
    DFMIO::DFile dfile(saveImage);
    if (dfile.open(DFMIO::DFile::OpenFlag::kReadOnly)) {
        const QByteArray &output = dfile.readAll();
        if (output.isEmpty()) {
            qCWarning(logDFMBase) << "thumbnail: read failed:" << info;
            dfile.close();
            return img;
        }

        if (stoped && (*stoped).load(std::memory_order_release))
            return {};
        img.loadFromData(output, "png");
        dfile.close();
    }

    return img;
}

QImage ThumbnailCreators::pdfThumbnailCreator(const FileInfoPointer &info, ThumbnailSize size, const std::atomic_bool *stoped)
{
    if (info.isNull()) {
        qCWarning(logDFMBase) << "ThumbnailCreators::pdfThumbnailCreator file info is nullptr !";
        return {};
    }
    QImage img;
    const auto &path = info->pathOf(PathInfoType::kAbsoluteFilePath);
    QScopedPointer<poppler::document> doc(poppler::document::load_from_file(path.toStdString()));
    if (!doc || doc->is_locked()) {
        qCWarning(logDFMBase) << "thumbnail: can not read this pdf file." << path;
        return img;
    }

    if (doc->pages() < 1) {
        qCWarning(logDFMBase) << "thumbnail: this stream is invalid." << path;
        return img;
    }

    if (stoped && (*stoped).load(std::memory_order_release))
        return {};

    QScopedPointer<const poppler::page> page(doc->create_page(0));
    if (!page) {
        qCWarning(logDFMBase) << "thumbnail: can not get this page at index 0." << path;
        return img;
    }

    poppler::page_renderer pr;
    pr.set_render_hint(poppler::page_renderer::antialiasing, true);
    pr.set_render_hint(poppler::page_renderer::text_antialiasing, true);

    poppler::image imageData = pr.render_page(page.data(), 72, 72, -1, -1, -1, size);
    if (!imageData.is_valid()) {
        qCWarning(logDFMBase) << "thumbnail: the render page is invalid." << path;
        return img;
    }

    poppler::image::format_enum format = imageData.format();
    switch (format) {
    case poppler::image::format_invalid:
        qCWarning(logDFMBase) << "thumbnail: image format is invalid." << path;
        break;
    case poppler::image::format_mono:
        img = QImage(reinterpret_cast<uchar *>(imageData.data()), imageData.width(), imageData.height(), QImage::Format_Mono);
        break;
    case poppler::image::format_rgb24:
        img = QImage(reinterpret_cast<uchar *>(imageData.data()), imageData.width(), imageData.height(), QImage::Format_ARGB6666_Premultiplied);
        break;
    case poppler::image::format_argb32:
        img = QImage(reinterpret_cast<uchar *>(imageData.data()), imageData.width(), imageData.height(), QImage::Format_ARGB32);
        break;
    default:
        break;
    }

    if (!img.isNull())
        img = img.scaled(QSize(size, size), Qt::KeepAspectRatio, Qt::SmoothTransformation);

    return img;
}

QImage ThumbnailCreators::appimageThumbnailCreator(const FileInfoPointer &info, ThumbnailSize size, const std::atomic_bool *stoped)
{
    if (!info
        || info->nameOf(NameInfoType::kMimeTypeName) != Global::Mime::kTypeAppAppimage
        || !info->isAttributes(FileInfo::FileIsType::kIsExecutable)) {
        qCWarning(logDFMBase) << "ThumbnailCreators::appimageThumbnailCreator File is not a valid AppImage or has no executable permission:" << info
                              << "mimeType:" << (info ? info->nameOf(NameInfoType::kMimeTypeName) : "null")
                              << "isExecutable:" << (info ? info->isAttributes(FileInfo::FileIsType::kIsExecutable) : false);
        return QImage();
    }

    const auto &path = info->pathOf(PathInfoType::kAbsoluteFilePath);

    // 3. 创建临时目录，用于解压 appimage
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        qCWarning(logDFMBase) << "Cannot create temporary directory for extraction. Error:" << tempDir.errorString()
                              << "File:" << path;
        return QImage();
    }
    auto extractTo = tempDir.path();

    if (stoped && (*stoped).load(std::memory_order_release))
        return {};

    // 4. 解压 appimage 到临时目录
    QProcess proc;
    proc.setWorkingDirectory(extractTo);
    proc.start(path, { "--appimage-extract" });
    auto done = proc.waitForFinished();
    if (stoped && (*stoped).load(std::memory_order_release))
        return {};
    qCInfo(logDFMBase) << "AppImage extraction completed for" << info
                       << "to" << extractTo
                       << "with status:" << (done ? "success" : "failed");

    // 5. 遍历临时目录下的 png/svg 文件
    QString iconPath;
    QDir extractDir(extractTo + "/squashfs-root");
    auto files = extractDir.entryInfoList(QStringList { "*.png", "*.svg" },
                                          QDir::Files | QDir::NoDotAndDotDot);
    iconPath = files.isEmpty() ? "" : files.first().filePath();

    QImage icon;
    if (!iconPath.isEmpty())
        icon = QImage(iconPath).scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    else
        qCWarning(logDFMBase) << "Failed to find icon in AppImage:" << path;

    return icon;
}
