#include "WallpaperPackager.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QThread>
#include <QVector>

namespace {
const int DefaultVcdWidth = 3950;
const int DefaultVcdHeight = 1320;
const int DefaultSharedScreenWidth = 8960;
const int DefaultSharedScreenHeight = 1320;
const int DefaultSharedLeftWidth = 5010;
const int DefaultSharedRightWidth = 3950;
const int DefaultPreviewWidth = 2198;
const int DefaultPreviewHeight = 367;
const int DefaultScreenSaverWidth = 1920;
const int DefaultScreenSaverHeight = 1080;
const int KzbImageHeaderSize = 24;
const int KzbImageFooterSize = 8;
const int KzbAstcPayloadSize = 2956800;
const int KzbTextureEntrySize = KzbImageHeaderSize + KzbAstcPayloadSize + KzbImageFooterSize;
const int KzbDecodedWidth = 8960;
const int KzbDecodedHeight = 1320;
const int KzbBlurEdgeFadeWidth = 390;
const int DarkWallpaperOverlayAlpha = 80;

bool isJpegFile(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == "jpg" || suffix == "jpeg";
}

bool hasCadiWallpaperChild(const QDir &dir)
{
    const QFileInfoList children = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &child : children) {
        if (child.fileName().startsWith("cadi_wallpaper", Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

QString firstCadiWallpaperChild(const QString &packagePath, const QString &fallback)
{
    const QDir packageDir(packagePath);
    const QFileInfoList children = packageDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &child : children) {
        if (child.fileName().startsWith("cadi_wallpaper", Qt::CaseInsensitive)) {
            return child.fileName();
        }
    }
    return fallback;
}

void reportProgress(const WallpaperPackager::ProgressCallback &progress, int value, const QString &message)
{
    if (progress) {
        progress(qBound(0, value, 100), message);
    }
}

QString astcencPath();

struct KzbEntry {
    quint32 id = 0;
    quint32 unknown = 0;
    quint32 offset = 0;
    quint32 size = 0;
    quint32 pad = 0;
    quint32 size2 = 0;
    QString path;
};

QString readNullTerminatedAscii(QFile &file)
{
    QByteArray bytes;
    char ch = 0;
    while (file.getChar(&ch)) {
        if (ch == '\0') {
            break;
        }
        bytes.append(ch);
    }
    return QString::fromLatin1(bytes);
}

quint32 readUInt32(QFile &file, bool *ok)
{
    quint32 value = 0;
    if (file.read(reinterpret_cast<char *>(&value), sizeof(value)) != sizeof(value)) {
        if (ok) {
            *ok = false;
        }
        return 0;
    }
    return value;
}

bool readKzbEntries(const QString &path, QVector<KzbEntry> *entries, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("无法读取 KZB: %1").arg(path);
        }
        return false;
    }

    bool ok = true;
    const quint32 magic = readUInt32(file, &ok);
    if (!ok || magic != 0x46425A4B) {
        if (error) {
            *error = QStringLiteral("不是有效 KZB 文件: %1").arg(path);
        }
        return false;
    }

    readUInt32(file, &ok);
    const quint32 nameLength = readUInt32(file, &ok);
    const QByteArray nameBytes = file.read(nameLength);
    if (!ok || nameBytes.size() != static_cast<int>(nameLength)) {
        if (error) {
            *error = QStringLiteral("KZB 头读取失败? %1").arg(path);
        }
        return false;
    }

    const QString name = QString::fromLatin1(nameBytes);
    readUInt32(file, &ok); // CT5 KZB name padding.
    const quint32 pathCount = readUInt32(file, &ok);
    if (!ok || pathCount == 0 || pathCount > 4096) {
        if (error) {
            *error = QStringLiteral("KZB 目录数量异常: %1").arg(path);
        }
        return false;
    }

    QVector<QString> paths;
    paths.reserve(static_cast<int>(pathCount));
    for (quint32 i = 0; i < pathCount; ++i) {
        paths.append(name + readNullTerminatedAscii(file));
    }

    const quint32 entryCount = readUInt32(file, &ok);
    if (!ok || entryCount != pathCount) {
        if (error) {
            *error = QStringLiteral("KZB 目录表不匹配: %1").arg(path);
        }
        return false;
    }

    QVector<KzbEntry> parsed;
    parsed.reserve(static_cast<int>(entryCount));
    for (quint32 i = 0; i < entryCount; ++i) {
        KzbEntry entry;
        entry.id = readUInt32(file, &ok);
        entry.unknown = readUInt32(file, &ok);
        entry.offset = readUInt32(file, &ok);
        entry.size = readUInt32(file, &ok);
        entry.pad = readUInt32(file, &ok);
        entry.size2 = readUInt32(file, &ok);
        entry.path = paths.at(static_cast<int>(i));
        if (!ok) {
            if (error) {
                *error = QStringLiteral("KZB 条目表读取失败? %1").arg(path);
            }
            return false;
        }
        parsed.append(entry);
    }

    if (entries) {
        *entries = parsed;
    }
    return true;
}

bool isLeftWallpaperKzbEntry(const KzbEntry &entry)
{
    const QString path = entry.path.toLower();
    const QString fileName = path.section(QLatin1Char('/'), -1);
    return entry.size > 0
            && path.contains(QStringLiteral("/resource files/images/"))
            && fileName.contains(QStringLiteral("wallpaper"))
            && path.endsWith(QStringLiteral(".png"));
}

bool isKzbImageEntry(const KzbEntry &entry)
{
    const QString path = entry.path.toLower();
    return entry.size == KzbTextureEntrySize
            && path.contains(QStringLiteral("/resource files/images/"))
            && path.endsWith(QStringLiteral(".png"));
}

QString safeOutputName(int index, const QString &path, const QString &suffix)
{
    QString name = path;
    name.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    if (name.size() > 140) {
        name = name.right(140);
    }
    return QStringLiteral("%1_%2%3").arg(index, 3, 10, QLatin1Char('0')).arg(name, suffix);
}

QByteArray astcHeader(int width, int height)
{
    QByteArray header(16, '\0');
    header[0] = char(0x13);
    header[1] = char(0xAB);
    header[2] = char(0xA1);
    header[3] = char(0x5C);
    header[4] = char(8);
    header[5] = char(8);
    header[6] = char(1);
    header[7] = char(width & 0xFF);
    header[8] = char((width >> 8) & 0xFF);
    header[9] = char((width >> 16) & 0xFF);
    header[10] = char(height & 0xFF);
    header[11] = char((height >> 8) & 0xFF);
    header[12] = char((height >> 16) & 0xFF);
    header[13] = char(1);
    return header;
}

bool writeFileBytes(const QString &path, const QByteArray &bytes, QString *error)
{
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error) {
            *error = QStringLiteral("Cannot create directory: %1").arg(info.absolutePath());
        }
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot write file: %1").arg(path);
        }
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        if (error) {
            *error = QStringLiteral("Short write: %1").arg(path);
        }
        return false;
    }
    return true;
}

bool decodeAstcToPng(const QString &astcPath, const QString &pngPath, QString *error)
{
    const QString decoder = astcencPath();
    if (decoder.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Cannot find astcenc-native.exe");
        }
        return false;
    }

    const QFileInfo pngInfo(pngPath);
    if (!QDir().mkpath(pngInfo.absolutePath())) {
        if (error) {
            *error = QStringLiteral("Cannot create directory: %1").arg(pngInfo.absolutePath());
        }
        return false;
    }

    QProcess process;
    process.setProgram(decoder);
    process.setArguments(QStringList()
                         << QStringLiteral("-dl")
                         << astcPath
                         << pngPath);
    process.start();
    if (!process.waitForFinished(-1)
            || process.exitStatus() != QProcess::NormalExit
            || process.exitCode() != 0) {
        if (error) {
            *error = QStringLiteral("ASTC decode failed: %1 %2")
                    .arg(QString::fromLocal8Bit(process.readAllStandardOutput()),
                         QString::fromLocal8Bit(process.readAllStandardError()));
        }
        return false;
    }
    return true;
}

bool saveVerticalFlip(const QString &sourcePath, const QString &targetPath, QString *error)
{
    QImage image(sourcePath);
    if (image.isNull()) {
        if (error) {
            *error = QStringLiteral("Cannot load decoded PNG: %1").arg(sourcePath);
        }
        return false;
    }
    const QFileInfo info(targetPath);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error) {
            *error = QStringLiteral("Cannot create directory: %1").arg(info.absolutePath());
        }
        return false;
    }
    if (!image.mirrored(false, true).save(targetPath, "PNG", 100)) {
        if (error) {
            *error = QStringLiteral("Cannot write flipped PNG: %1").arg(targetPath);
        }
        return false;
    }
    return true;
}

QImage opaqueImage(const QImage &image)
{
    return image.convertToFormat(QImage::Format_RGB32).convertToFormat(QImage::Format_ARGB32);
}

QImage darkenWallpaper(const QImage &image)
{
    QImage output = image.convertToFormat(QImage::Format_ARGB32);
    QPainter painter(&output);
    painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    painter.fillRect(output.rect(), QColor(0, 0, 0, DarkWallpaperOverlayAlpha));
    return output.convertToFormat(QImage::Format_ARGB32);
}

QImage clearRightSide(const QImage &image, int keepWidth)
{
    QImage output = image.convertToFormat(QImage::Format_ARGB32);
    QPainter painter(&output);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    const int clearX = qBound(0, keepWidth, output.width());
    if (clearX < output.width()) {
        painter.fillRect(QRect(clearX, 0, output.width() - clearX, output.height()),
                         QColor(0, 0, 0, 0));
    }
    return output.convertToFormat(QImage::Format_ARGB32);
}

bool removeKnownDebugArtifacts(const QString &innerPath, QString *error)
{
    const QStringList debugFilePatterns = {
        QStringLiteral("*_rgb_forced_opaque*.png"),
        QStringLiteral("*_edge_crop.png"),
        QStringLiteral("*_left_preview.png"),
        QStringLiteral("*_visible.png"),
        QStringLiteral("*_raw.png")
    };

    for (const QString &pattern : debugFilePatterns) {
        QDirIterator it(innerPath, QStringList() << pattern, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            if (!QFile::remove(path)) {
                if (error) {
                    *error = QStringLiteral("无法删除调试文件: %1").arg(path);
                }
                return false;
            }
        }
    }

    const QStringList debugDirNames = {
        QStringLiteral("_analysis_decoded_kzb")
    };
    for (const QString &name : debugDirNames) {
        QDirIterator it(innerPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            if (QFileInfo(path).fileName().compare(name, Qt::CaseInsensitive) != 0) {
                continue;
            }
            QDir dir(path);
            if (!dir.removeRecursively()) {
                if (error) {
                    *error = QStringLiteral("无法删除调试目录: %1").arg(path);
                }
                return false;
            }
        }
    }
    return true;
}

bool readKzbAstcBlocks(QFile &file, const KzbEntry &entry, int index, QByteArray *blocks, QString *error)
{
    if (entry.size != KzbTextureEntrySize) {
        if (error) {
            *error = QStringLiteral("KZB image entry size mismatch: index %1").arg(index);
        }
        return false;
    }

    const quint64 fileSize = static_cast<quint64>(file.size());
    const quint64 dataOffset = static_cast<quint64>(entry.offset) + KzbImageHeaderSize;
    const quint64 dataEnd = dataOffset + KzbAstcPayloadSize;
    if (dataEnd > fileSize || dataEnd > static_cast<quint64>(entry.offset) + static_cast<quint64>(entry.size)) {
        if (error) {
            *error = QStringLiteral("KZB image entry is outside file bounds: index %1").arg(index);
        }
        return false;
    }

    if (!file.seek(static_cast<qint64>(dataOffset))) {
        if (error) {
            *error = QStringLiteral("Cannot seek KZB image payload: index %1").arg(index);
        }
        return false;
    }

    const QByteArray payload = file.read(KzbAstcPayloadSize);
    if (payload.size() != KzbAstcPayloadSize) {
        if (error) {
            *error = QStringLiteral("Cannot read KZB image payload: index %1").arg(index);
        }
        return false;
    }

    if (blocks) {
        *blocks = payload;
    }
    return true;
}

QImage decodeKzbEntryVisibleImage(QFile &file, const KzbEntry &entry, const QString &tempPath,
                                  int index, QString *error)
{
    QByteArray blocks;
    if (!readKzbAstcBlocks(file, entry, index, &blocks, error)) {
        return QImage();
    }

    const QString base = safeOutputName(index, entry.path.section(QLatin1Char('/'), -1), QString());
    const QString astcPath = QDir(tempPath).filePath(base + QStringLiteral("_base.astc"));
    const QString decodedPath = QDir(tempPath).filePath(base + QStringLiteral("_base.png"));

    QByteArray astc = astcHeader(KzbDecodedWidth, KzbDecodedHeight);
    astc.append(blocks);
    if (!writeFileBytes(astcPath, astc, error)
            || !decodeAstcToPng(astcPath, decodedPath, error)) {
        return QImage();
    }

    QImage decoded(decodedPath);
    if (decoded.isNull()) {
        if (error) {
            *error = QStringLiteral("Cannot load decoded KZB image: index %1").arg(index);
        }
        return QImage();
    }
    if (decoded.size() != QSize(KzbDecodedWidth, KzbDecodedHeight)) {
        if (error) {
            *error = QStringLiteral("Decoded KZB image size mismatch: index %1, got %2x%3")
                    .arg(index).arg(decoded.width()).arg(decoded.height());
        }
        return QImage();
    }

    return decoded.mirrored(false, true).convertToFormat(QImage::Format_ARGB32);
}

bool preserveRightAstcBlocks(QFile &file, const KzbEntry &entry, int index, int preserveFromX,
                             QByteArray *payload, QString *error)
{
    if (!payload || payload->size() != KzbAstcPayloadSize) {
        if (error) {
            *error = QStringLiteral("Encoded KZB payload size mismatch: index %1").arg(index);
        }
        return false;
    }

    QByteArray original;
    if (!readKzbAstcBlocks(file, entry, index, &original, error)) {
        return false;
    }

    const int blockBytes = 16;
    const int blocksX = (KzbDecodedWidth + 7) / 8;
    const int blocksY = (KzbDecodedHeight + 7) / 8;
    const int replaceBlocks = qBound(0, (preserveFromX + 7) / 8, blocksX);
    const int keepOffsetInRow = replaceBlocks * blockBytes;
    const int keepBytesInRow = (blocksX - replaceBlocks) * blockBytes;
    for (int y = 0; y < blocksY; ++y) {
        const int rowOffset = y * blocksX * blockBytes;
        payload->replace(rowOffset + keepOffsetInRow,
                         keepBytesInRow,
                         original.constData() + rowOffset + keepOffsetInRow,
                         keepBytesInRow);
    }
    return true;
}

QString firstKzbInPath(const QString &path)
{
    const QFileInfo info(path);
    if (info.isFile() && info.suffix().compare(QStringLiteral("kzb"), Qt::CaseInsensitive) == 0) {
        return info.absoluteFilePath();
    }
    if (!info.isDir()) {
        return QString();
    }

    const QString direct = QDir(info.absoluteFilePath()).filePath(QStringLiteral("ipd/wallpaper"));
    if (QDir(direct).exists()) {
        const QFileInfoList files = QDir(direct).entryInfoList(QStringList() << QStringLiteral("*.kzb"),
                                                              QDir::Files, QDir::Name);
        if (!files.isEmpty()) {
            return files.first().absoluteFilePath();
        }
    }

    const QFileInfoList children = QDir(info.absoluteFilePath()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                                               QDir::Name);
    for (const QFileInfo &child : children) {
        if (child.fileName().startsWith(QStringLiteral("cadi_wallpaper"), Qt::CaseInsensitive)) {
            const QString found = firstKzbInPath(child.absoluteFilePath());
            if (!found.isEmpty()) {
                return found;
            }
        }
    }

    QDirIterator it(info.absoluteFilePath(), QStringList() << QStringLiteral("*.kzb"),
                    QDir::Files, QDirIterator::Subdirectories);
    return it.hasNext() ? it.next() : QString();
}

QString astcencPath()
{
    const QString appLocal = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("astcenc-native.exe"));
    if (QFileInfo::exists(appLocal)) {
        return appLocal;
    }

    const QString devPath = QStringLiteral("D:/Cadillac/kzb-tools/bin/astcenc-native.exe");
    if (QFileInfo::exists(devPath)) {
        return devPath;
    }

    const QString projectBinPath = QStringLiteral("D:/Cadillac/Ct5WallpaperPackager/bin/x64/Release/astcenc-native.exe");
    if (QFileInfo::exists(projectBinPath)) {
        return projectBinPath;
    }

    return QString();
}
}

QString WallpaperPackager::defaultPackageId()
{
    return QStringLiteral("BFA3A0F4596C4C57A6BCDC1EB3348932");
}

QString WallpaperPackager::defaultInnerFolderName()
{
    return QStringLiteral("cadi_wallpaper05111930");
}

WallpaperPackager::KzbUnpackResult WallpaperPackager::unpackKzb(
        const QString &kzbPath,
        const QString &outputRootPath,
        const ProgressCallback &progress)
{
    KzbUnpackResult result;
    const QString sourcePath = QDir::toNativeSeparators(kzbPath.trimmed());
    if (sourcePath.isEmpty() || !QFileInfo(sourcePath).isFile()) {
        result.message = QStringLiteral("Please choose an existing .kzb file");
        return result;
    }
    if (QFileInfo(sourcePath).suffix().compare(QStringLiteral("kzb"), Qt::CaseInsensitive) != 0) {
        result.message = QStringLiteral("Input is not a .kzb file: %1").arg(sourcePath);
        return result;
    }

    QString outputRoot = QDir::toNativeSeparators(outputRootPath.trimmed());
    if (outputRoot.isEmpty()) {
        const QFileInfo sourceInfo(sourcePath);
        outputRoot = sourceInfo.dir().filePath(sourceInfo.completeBaseName() + QStringLiteral("_unpacked"));
    }

    QString error;
    reportProgress(progress, 2, QStringLiteral("Read KZB entry table"));
    QVector<KzbEntry> entries;
    if (!readKzbEntries(sourcePath, &entries, &error)) {
        result.message = error;
        return result;
    }

    if (!QDir().mkpath(outputRoot)) {
        result.message = QStringLiteral("Cannot create output directory: %1").arg(outputRoot);
        return result;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        result.message = QStringLiteral("Cannot create temporary decode directory");
        return result;
    }

    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        result.message = QStringLiteral("Cannot read KZB: %1").arg(sourcePath);
        return result;
    }

    int imageCount = 0;
    const QByteArray header = astcHeader(KzbDecodedWidth, KzbDecodedHeight);
    for (int i = 0; i < entries.size(); ++i) {
        const KzbEntry &entry = entries.at(i);
        if (entry.offset > quint64(source.size())
                || quint64(entry.offset) + quint64(entry.size) > quint64(source.size())) {
            result.message = QStringLiteral("KZB entry is outside file bounds: index %1").arg(i);
            return result;
        }
        if (!source.seek(entry.offset)) {
            result.message = QStringLiteral("Cannot seek KZB entry: index %1").arg(i);
            return result;
        }

        const QByteArray payload = source.read(entry.size);
        if (payload.size() != static_cast<int>(entry.size)) {
            result.message = QStringLiteral("Cannot read KZB entry: index %1").arg(i);
            return result;
        }

        if (!isKzbImageEntry(entry)) {
            continue;
        }

        const QByteArray blocks = payload.mid(KzbImageHeaderSize, KzbAstcPayloadSize);
        if (blocks.size() != KzbAstcPayloadSize) {
            result.message = QStringLiteral("KZB image payload size mismatch: index %1").arg(i);
            return result;
        }

        QString imageName = entry.path.section(QLatin1Char('/'), -1);
        if (imageName.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)) {
            imageName.chop(4);
        }
        const QString safeBase = safeOutputName(i, imageName, QString());
        const QString astcPath = QDir(tempDir.path()).filePath(safeBase + QStringLiteral(".astc"));
        const QString decodedPath = QDir(tempDir.path()).filePath(safeBase + QStringLiteral(".png"));
        const QString correctedPath = QDir(outputRoot).filePath(safeBase + QStringLiteral(".png"));

        QByteArray astc = header;
        astc.append(blocks);
        if (!writeFileBytes(astcPath, astc, &error)
                || !decodeAstcToPng(astcPath, decodedPath, &error)
                || !saveVerticalFlip(decodedPath, correctedPath, &error)) {
            result.message = error;
            return result;
        }

        ++imageCount;
        reportProgress(progress, 8 + imageCount * 10, QStringLiteral("Decoded KZB image %1").arg(i));
    }

    result.ok = true;
    result.outputPath = QDir::toNativeSeparators(outputRoot);
    result.entryCount = entries.size();
    result.imageCount = imageCount;
    result.message = QStringLiteral("KZB unpacked: %1 entries, %2 images").arg(result.entryCount).arg(result.imageCount);
    reportProgress(progress, 100, result.message);
    return result;
}

WallpaperPackager::Result WallpaperPackager::buildFromImage(const Request &request, const ProgressCallback &progress)
{
    Result result;
    reportProgress(progress, 0, QStringLiteral("检查输入参数"));

    const QString imagePath = QDir::toNativeSeparators(request.imagePath.trimmed());
    const QString darkImagePath = QDir::toNativeSeparators(request.darkImagePath.trimmed());
    const QString templatePath = normalizeTemplatePackagePath(request.templatePackagePath.trimmed(), request.packageId);
    const QString targetPath = outputPackagePath(request.outputRootPath.trimmed(), request.packageId);

    if (imagePath.isEmpty() || !QFileInfo::exists(imagePath)) {
        result.message = QStringLiteral("请选择存在的 JPG/JPEG 图片");
        return result;
    }
    if (!isJpegFile(imagePath)) {
        result.message = QStringLiteral("当前工具只接受 jpg 或 jpeg 图片");
        return result;
    }
    if (templatePath.isEmpty() || !QFileInfo(templatePath).isDir()) {
        result.message = QStringLiteral("请选择完整的模板壁纸包目录");
        return result;
    }
    if (request.outputRootPath.trimmed().isEmpty()) {
        result.message = QStringLiteral("请选择输出目录");
        return result;
    }
    if (targetPath.isEmpty()) {
        result.message = QStringLiteral("输出目录无效");
        return result;
    }

    const QString templateCanonical = QFileInfo(templatePath).canonicalFilePath();
    const QString targetCanonical = QFileInfo(targetPath).canonicalFilePath();
    if (!targetCanonical.isEmpty() && targetCanonical == templateCanonical) {
        result.message = QStringLiteral("输出目录不能覆盖正在使用的模板包");
        return result;
    }

    QImage source;
    reportProgress(progress, 12, QStringLiteral("读取 JPG/JPEG 原图"));
    if (!source.load(imagePath)) {
        result.message = QStringLiteral("图片无法解码，请换一张 JPG/JPEG");
        return result;
    }

    QImage darkSource = source;
    const bool hasExplicitDarkSource = !darkImagePath.isEmpty();
    if (hasExplicitDarkSource) {
        if (!QFileInfo::exists(darkImagePath)) {
            result.message = QStringLiteral("请选择存在的深色模式 JPG/JPEG 图片");
            return result;
        }
        if (!isJpegFile(darkImagePath)) {
            result.message = QStringLiteral("深色模式图片只支持 jpg 或 jpeg");
            return result;
        }
        reportProgress(progress, 13, QStringLiteral("读取深色模式 JPG/JPEG 原图"));
        if (!darkSource.load(darkImagePath)) {
            result.message = QStringLiteral("深色模式图片无法解码");
            return result;
        }
    }

    QImage leftSource = source;
    const QString leftImagePath = QDir::toNativeSeparators(request.leftImagePath.trimmed());
    if (!leftImagePath.isEmpty()) {
        if (!QFileInfo::exists(leftImagePath)) {
            result.message = QStringLiteral("请选择存在的左半屏 JPG/JPEG 图片");
            return result;
        }
        if (!isJpegFile(leftImagePath)) {
            result.message = QStringLiteral("左半屏图片只支持 jpg 或 jpeg");
            return result;
        }
        reportProgress(progress, 14, QStringLiteral("读取左半屏 JPG/JPEG 原图"));
        if (!leftSource.load(leftImagePath)) {
            result.message = QStringLiteral("左半屏图片无法解码");
            return result;
        }
    }

    QFileInfo targetInfo(targetPath);
    if (targetInfo.exists()) {
        if (!request.overwrite) {
            result.message = QStringLiteral("输出壁纸包已存在，请勾选覆盖输出");
            return result;
        }
        reportProgress(progress, 22, QStringLiteral("删除旧输出包"));
        QDir existing(targetPath);
        if (!existing.removeRecursively()) {
            result.message = QStringLiteral("无法删除已有输出: %1").arg(targetPath);
            return result;
        }
    }

    QString error;
    reportProgress(progress, 32, QStringLiteral("复制模板包并保留左半屏 kzb 资源"));
    if (!copyDirectory(templatePath, targetPath, &error)) {
        result.message = error;
        return result;
    }

    const QString innerName = firstCadiWallpaperChild(
                targetPath,
                request.innerFolderName.trimmed().isEmpty()
                    ? defaultInnerFolderName()
                    : request.innerFolderName.trimmed());
    const QString innerPath = QDir(targetPath).filePath(innerName);
    if (!QDir(innerPath).exists() && !QDir(targetPath).mkpath(innerName)) {
        result.message = QStringLiteral("无法创建壁纸内部目录: %1").arg(innerPath);
        return result;
    }

    int written = 0;
    if (!removeKnownDebugArtifacts(innerPath, &error)) {
        result.message = error;
        return result;
    }
    reportProgress(progress, 52, QStringLiteral("生成裁切图片"));
    if (!writeThemeImages(innerPath, source, darkSource, hasExplicitDarkSource,
                          request, &written, &error, progress)) {
        result.message = error;
        return result;
    }

    if (true) {
        reportProgress(progress, 97, QStringLiteral("替换左半屏 KZB 图像资源"));
        if (!writeLeftKzbImages(innerPath, leftSource, darkSource, hasExplicitDarkSource,
                                request, &written, &error, progress)) {
            result.message = error;
            return result;
        }
    }

    result.ok = true;
    result.writtenImages = written;
    result.outputPackagePath = QDir::toNativeSeparators(targetPath);
    result.message = QStringLiteral("壁纸包制作完成，写入 %1 个 PNG").arg(written);
    reportProgress(progress, 100, result.message);
    return result;
}

QString WallpaperPackager::normalizeTemplatePackagePath(const QString &path, const QString &packageId)
{
    if (path.isEmpty()) {
        return QString();
    }
    QFileInfo info(path);
    if (!info.isDir()) {
        return QString();
    }

    QDir dir(info.absoluteFilePath());
    if (info.fileName().compare(packageId, Qt::CaseInsensitive) == 0 || hasCadiWallpaperChild(dir)) {
        return dir.absolutePath();
    }

    const QString packageChild = dir.filePath(packageId);
    if (QFileInfo(packageChild).isDir()) {
        return QFileInfo(packageChild).absoluteFilePath();
    }

    if (info.fileName().startsWith("cadi_wallpaper", Qt::CaseInsensitive)) {
        return info.dir().absolutePath();
    }

    return dir.absolutePath();
}

QString WallpaperPackager::outputPackagePath(const QString &outputRootPath, const QString &packageId)
{
    if (outputRootPath.isEmpty()) {
        return QString();
    }
    QFileInfo rootInfo(outputRootPath);
    if (rootInfo.fileName().compare(packageId, Qt::CaseInsensitive) == 0) {
        return rootInfo.absoluteFilePath();
    }
    return QDir(rootInfo.absoluteFilePath()).filePath(packageId);
}

bool WallpaperPackager::copyDirectory(const QString &sourcePath, const QString &targetPath, QString *error)
{
    const QDir sourceDir(sourcePath);
    if (!sourceDir.exists()) {
        if (error) {
            *error = QStringLiteral("模板目录不存在: %1").arg(sourcePath);
        }
        return false;
    }
    if (!QDir().mkpath(targetPath)) {
        if (error) {
            *error = QStringLiteral("无法创建输出目录: %1").arg(targetPath);
        }
        return false;
    }

    const QFileInfoList entries = sourceDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        const QString targetEntry = QDir(targetPath).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyDirectory(entry.absoluteFilePath(), targetEntry, error)) {
                return false;
            }
        } else {
            if (QFile::exists(targetEntry) && !QFile::remove(targetEntry)) {
                if (error) {
                    *error = QStringLiteral("无法覆盖文件: %1").arg(targetEntry);
                }
                return false;
            }
            if (!QFile::copy(entry.absoluteFilePath(), targetEntry)) {
                if (error) {
                    *error = QStringLiteral("无法复制文件: %1").arg(entry.absoluteFilePath());
                }
                return false;
            }
        }
    }
    return true;
}

bool WallpaperPackager::writeThemeImages(const QString &innerPath, const QImage &lightSource,
                                         const QImage &darkSource, bool hasExplicitDarkSource,
                                         const Request &request,
                                         int *written, QString *error, const ProgressCallback &progress)
{
    const int mainFocusX = qBound(0, request.mainCropFocusX, 100);
    const int mainFocusY = qBound(0, request.mainCropFocusY, 100);
    const int previewFocusX = qBound(0, request.previewCropFocusX, 100);
    const int previewFocusY = qBound(0, request.previewCropFocusY, 100);
    const int screenSaverFocusX = qBound(0, request.screenSaverCropFocusX, 100);
    const int screenSaverFocusY = qBound(0, request.screenSaverCropFocusY, 100);
    const int darkMainFocusX = qBound(0, request.darkMainCropFocusX, 100);
    const int darkMainFocusY = qBound(0, request.darkMainCropFocusY, 100);
    const int darkPreviewFocusX = qBound(0, request.darkPreviewCropFocusX, 100);
    const int darkPreviewFocusY = qBound(0, request.darkPreviewCropFocusY, 100);
    const int darkScreenSaverFocusX = qBound(0, request.darkScreenSaverCropFocusX, 100);
    const int darkScreenSaverFocusY = qBound(0, request.darkScreenSaverCropFocusY, 100);
    const int mainZoom = qBound(100, request.mainZoomPercent, 500);
    const int previewZoom = qBound(100, request.previewZoomPercent, 500);
    const int screenSaverZoom = qBound(100, request.screenSaverZoomPercent, 500);
    const int darkMainZoom = qBound(100, request.darkMainZoomPercent, 500);
    const int darkPreviewZoom = qBound(100, request.darkPreviewZoomPercent, 500);
    const int darkScreenSaverZoom = qBound(100, request.darkScreenSaverZoomPercent, 500);
    const int mainWidth = request.mainWidth > 0 ? request.mainWidth : DefaultVcdWidth;
    const int mainHeight = request.mainHeight > 0 ? request.mainHeight : DefaultVcdHeight;
    const int previewWidth = request.previewWidth > 0 ? request.previewWidth : DefaultPreviewWidth;
    const int previewHeight = request.previewHeight > 0 ? request.previewHeight : DefaultPreviewHeight;
    const int screenSaverWidth = request.screenSaverWidth > 0
            ? request.screenSaverWidth
            : DefaultScreenSaverWidth;
    const int screenSaverHeight = request.screenSaverHeight > 0
            ? request.screenSaverHeight
            : DefaultScreenSaverHeight;

    const QImage vcd = sharedSideCrop(lightSource, mainWidth, mainHeight, false, false,
                                      mainFocusX, mainFocusY, mainZoom);
    const QImage rawDarkVcd = sharedSideCrop(darkSource, mainWidth, mainHeight, false, false,
                                             darkMainFocusX, darkMainFocusY, darkMainZoom);
    const QImage darkVcd = hasExplicitDarkSource ? rawDarkVcd : darkenWallpaper(rawDarkVcd);
    const QImage dim = sharedSideCrop(lightSource, mainWidth, mainHeight, false, false,
                                      mainFocusX, mainFocusY, mainZoom);
    const QImage darkDim = sharedSideCrop(darkSource, mainWidth, mainHeight, false, true,
                                          darkMainFocusX, darkMainFocusY, darkMainZoom);
    const QImage preview = centerCrop(lightSource, previewWidth, previewHeight, false,
                                      previewFocusX, previewFocusY, previewZoom);
    const QImage rawDarkPreview = centerCrop(darkSource, previewWidth, previewHeight, false,
                                             darkPreviewFocusX, darkPreviewFocusY, darkPreviewZoom);
    const QImage darkPreview = hasExplicitDarkSource ? rawDarkPreview : darkenWallpaper(rawDarkPreview);

    reportProgress(progress, 60, QStringLiteral("写入预览图 light_preview_image.png"));
    if (!writePng(QDir(innerPath).filePath("light_preview_image.png"), preview, written, error)) {
        return false;
    }
    reportProgress(progress, 65, QStringLiteral("写入预览图 dark_preview_image.png"));
    if (!writePng(QDir(innerPath).filePath("dark_preview_image.png"), darkPreview, written, error)) {
        return false;
    }
    reportProgress(progress, 70, QStringLiteral("写入暗背景 light_dim_background.png"));
    if (!writePng(QDir(innerPath).filePath("light_dim_background.png"), dim, written, error)) {
        return false;
    }
    reportProgress(progress, 75, QStringLiteral("写入暗背景 dark_dim_background.png"));
    if (!writePng(QDir(innerPath).filePath("dark_dim_background.png"), darkDim, written, error)) {
        return false;
    }

    const QString vcdPath = QDir(innerPath).filePath("vcd/wallpaper");
    reportProgress(progress, 80, QStringLiteral("写入右半屏壁纸 light_wallpaper_vcd.png"));
    if (!writePng(QDir(vcdPath).filePath("light_wallpaper_vcd.png"), vcd, written, error)) {
        return false;
    }
    reportProgress(progress, 86, QStringLiteral("写入右半屏壁纸 dark_wallpaper_vcd.png"));
    if (!writePng(QDir(vcdPath).filePath("dark_wallpaper_vcd.png"), darkVcd, written, error)) {
        return false;
    }

    const QString screenSaverPath = QDir(innerPath).filePath("rid/screenSaver");
    if (QDir(screenSaverPath).exists()) {
        const QImage saver = centerCrop(lightSource, screenSaverWidth, screenSaverHeight, false,
                                        screenSaverFocusX, screenSaverFocusY, screenSaverZoom);
        const QImage rawDarkSaver = centerCrop(darkSource, screenSaverWidth, screenSaverHeight, false,
                                               darkScreenSaverFocusX, darkScreenSaverFocusY, darkScreenSaverZoom);
        const QImage darkSaver = hasExplicitDarkSource ? rawDarkSaver : darkenWallpaper(rawDarkSaver);
        reportProgress(progress, 92, QStringLiteral("写入屏保 light_screenSaver_rid.png"));
        if (!writePng(QDir(screenSaverPath).filePath("light_screenSaver_rid.png"), saver, written, error)) {
            return false;
        }
        reportProgress(progress, 96, QStringLiteral("写入屏保 dark_screenSaver_rid.png"));
        if (!writePng(QDir(screenSaverPath).filePath("dark_screenSaver_rid.png"), darkSaver, written, error)) {
            return false;
        }
    }

    return true;
}

bool WallpaperPackager::replaceLeftKzbImages(const QString &targetInnerPath, const QString &sourcePath,
                                             int *written, QString *error, const ProgressCallback &progress)
{
    const QString sourceKzb = firstKzbInPath(sourcePath.trimmed());
    const QString targetKzb = firstKzbInPath(targetInnerPath);

    if (sourceKzb.isEmpty()) {
        if (error) {
            *error = QStringLiteral("左半屏来源中未找到 .kzb: %1").arg(sourcePath);
        }
        return false;
    }
    if (targetKzb.isEmpty()) {
        if (error) {
            *error = QStringLiteral("输出包中未找到目标 .kzb: %1").arg(targetInnerPath);
        }
        return false;
    }

    if (QFileInfo(sourceKzb).canonicalFilePath() == QFileInfo(targetKzb).canonicalFilePath()) {
        reportProgress(progress, 98, QStringLiteral("左半屏来源 KZB 与目标相同，跳过替换"));
        return true;
    }

    QVector<KzbEntry> sourceEntries;
    QVector<KzbEntry> targetEntries;
    if (!readKzbEntries(sourceKzb, &sourceEntries, error)
            || !readKzbEntries(targetKzb, &targetEntries, error)) {
        return false;
    }

    QFile sourceFile(sourceKzb);
    QFile targetFile(targetKzb);
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("无法读取左半屏来源 KZB: %1").arg(sourceKzb);
        }
        return false;
    }
    if (!targetFile.open(QIODevice::ReadWrite)) {
        if (error) {
            *error = QStringLiteral("无法写入目标 KZB: %1").arg(targetKzb);
        }
        return false;
    }

    int replaced = 0;
    const int count = qMin(sourceEntries.size(), targetEntries.size());
    for (int i = 0; i < count; ++i) {
        const KzbEntry &sourceEntry = sourceEntries.at(i);
        const KzbEntry &targetEntry = targetEntries.at(i);
        if (!isLeftWallpaperKzbEntry(sourceEntry) || !isLeftWallpaperKzbEntry(targetEntry)) {
            continue;
        }
        if (sourceEntry.size != targetEntry.size || sourceEntry.size == 0) {
            if (error) {
                *error = QStringLiteral("KZB 条目大小不一致，不能安全替换: index %1").arg(i);
            }
            return false;
        }
        if (!sourceFile.seek(sourceEntry.offset) || !targetFile.seek(targetEntry.offset)) {
            if (error) {
                *error = QStringLiteral("KZB 条目定位失败: index %1").arg(i);
            }
            return false;
        }

        const QByteArray data = sourceFile.read(sourceEntry.size);
        if (data.size() != static_cast<int>(sourceEntry.size)) {
            if (error) {
                *error = QStringLiteral("KZB 来源条目读取失败: index %1").arg(i);
            }
            return false;
        }
        if (targetFile.write(data) != data.size()) {
            if (error) {
                *error = QStringLiteral("KZB 目标条目写入失败: index %1").arg(i);
            }
            return false;
        }
        ++replaced;
    }

    if (replaced == 0) {
        if (error) {
            *error = QStringLiteral("未找到可替换的左半屏 wallpaper KZB 图像条目");
        }
        return false;
    }

    if (written) {
        *written += replaced;
    }
    reportProgress(progress, 99, QStringLiteral("已替换左半屏 KZB 图像条目 %1 个").arg(replaced));
    return true;
}

bool WallpaperPackager::writeLeftKzbImages(const QString &targetInnerPath, const QImage &lightSource,
                                           const QImage &darkSource, bool hasExplicitDarkSource,
                                           const Request &request, int *written, QString *error,
                                           const ProgressCallback &progress)
{
    const QString targetKzb = firstKzbInPath(targetInnerPath);
    if (targetKzb.isEmpty()) {
        if (error) {
            *error = QStringLiteral("输出包中未找到目标 .kzb: %1").arg(targetInnerPath);
        }
        return false;
    }

    QVector<KzbEntry> entries;
    if (!readKzbEntries(targetKzb, &entries, error)) {
        return false;
    }

    const int leftFocusX = qBound(0, request.leftCropFocusX, 100);
    const int leftFocusY = qBound(0, request.leftCropFocusY, 100);
    const int leftZoom = qBound(100, request.leftZoomPercent, 500);
    const int darkLeftFocusX = qBound(0, request.darkLeftCropFocusX, 100);
    const int darkLeftFocusY = qBound(0, request.darkLeftCropFocusY, 100);
    const int darkLeftZoom = qBound(100, request.darkLeftZoomPercent, 500);

    reportProgress(progress, 95, QStringLiteral("Crop KZB full image 8960x1320"));
    const QImage lightVisible = opaqueCopy(centerCrop(lightSource,
                                                      DefaultSharedScreenWidth,
                                                      DefaultSharedScreenHeight,
                                                      false,
                                                      leftFocusX,
                                                      leftFocusY,
                                                      leftZoom));
    const QImage rawDarkVisible = opaqueCopy(centerCrop(darkSource,
                                                        DefaultSharedScreenWidth,
                                                        DefaultSharedScreenHeight,
                                                        false,
                                                        darkLeftFocusX,
                                                        darkLeftFocusY,
                                                        darkLeftZoom));
    auto darken = [](const QImage &image) {
        return darkenWallpaper(image);
    };
    auto storageImage = [](const QImage &visible) {
        return visible.mirrored(false, true).convertToFormat(QImage::Format_ARGB32);
    };
    auto blurEdgeFade = [](const QImage &image) {
        QImage output = image.convertToFormat(QImage::Format_ARGB32);
        const int fadeStart = qMax(0, DefaultSharedLeftWidth - KzbBlurEdgeFadeWidth);
        const int fadeEnd = qMin(output.width(), DefaultSharedLeftWidth);

        for (int y = 0; y < output.height(); ++y) {
            QRgb *line = reinterpret_cast<QRgb *>(output.scanLine(y));
            for (int x = fadeStart; x < output.width(); ++x) {
                if (x >= fadeEnd) {
                    line[x] = qRgba(0, 0, 0, 0);
                    continue;
                }
                const QRgb pixel = line[x];
                const qreal t = (x - fadeStart) / static_cast<qreal>(fadeEnd - fadeStart);
                const int alpha = qRound(qAlpha(pixel) * qBound<qreal>(0.0, 1.0 - t, 1.0));
                line[x] = qRgba(qRed(pixel), qGreen(pixel), qBlue(pixel), alpha);
            }
        }
        return output.convertToFormat(QImage::Format_ARGB32);
    };
    auto composeRegion = [](const QImage &baseVisible, const QImage &overlayVisible, int width) {
        QImage output = baseVisible.convertToFormat(QImage::Format_ARGB32);
        QPainter painter(&output);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        const QRect rect(0, 0, qBound(0, width, output.width()), DefaultSharedScreenHeight);
        painter.drawImage(rect, overlayVisible, rect);
        return output.convertToFormat(QImage::Format_ARGB32);
    };

    const QImage darkVisible = hasExplicitDarkSource ? rawDarkVisible : darken(lightVisible);
    // KZB 069/072 fade out inside the left screen; the right side is transparent.
    const QImage lightBlurVisible = blurEdgeFade(lightVisible);
    const QImage darkBlurVisible = blurEdgeFade(darkVisible);
    const QImage lightDicVisible = clearRightSide(lightVisible, DefaultSharedLeftWidth);
    const QImage darkDicVisible = clearRightSide(darkVisible, DefaultSharedLeftWidth);

    const QImage light = storageImage(lightVisible);
    const QImage dark = storageImage(darkVisible);

    QByteArray lightPayload;
    QByteArray darkPayload;

    reportProgress(progress, 97, QStringLiteral("ASTC 8x8 编码左半屏正常图"));
    if (!encodeAstc8x8(light, &lightPayload, error)
            || !encodeAstc8x8(dark, &darkPayload, error)) {
        return false;
    }
    reportProgress(progress, 98, QStringLiteral("ASTC 8x8 编码左半屏模糊图"));
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        if (error) {
            *error = QStringLiteral("Cannot create temporary KZB decode directory");
        }
        return false;
    }
    QFile targetFile(targetKzb);
    if (!targetFile.open(QIODevice::ReadWrite)) {
        if (error) {
            *error = QStringLiteral("无法写入目标 KZB: %1").arg(targetKzb);
        }
        return false;
    }

    int replaced = 0;
    for (int i = 0; i < entries.size(); ++i) {
        const KzbEntry &entry = entries.at(i);
        if (!isLeftWallpaperKzbEntry(entry)) {
            continue;
        }

        const QString entryPath = entry.path.toLower();
        const bool darkEntry = entryPath.contains(QStringLiteral("dark"));
        const bool blurEntry = entryPath.contains(QStringLiteral("blur")) || entry.id == 69 || entry.id == 72;
        const bool dicEntry = entryPath.contains(QStringLiteral("dic")) || entry.id == 70 || entry.id == 73;
        const bool halfEntry = blurEntry || dicEntry;

        QByteArray entryPayload;
        const QByteArray *payload = darkEntry ? &darkPayload : &lightPayload;
        if (halfEntry) {
            const QImage baseVisible = decodeKzbEntryVisibleImage(targetFile, entry, tempDir.path(), i, error);
            if (baseVisible.isNull()) {
                return false;
            }

            const QImage *leftVisible = darkEntry ? &darkVisible : &lightVisible;
            int replaceWidth = DefaultSharedLeftWidth;
            if (blurEntry) {
                leftVisible = darkEntry ? &darkBlurVisible : &lightBlurVisible;
                replaceWidth = KzbDecodedWidth;
            } else if (dicEntry) {
                leftVisible = darkEntry ? &darkDicVisible : &lightDicVisible;
            }

            reportProgress(progress, 98, QStringLiteral("ASTC 8x8 encode KZB entry %1").arg(entry.id));
            const QImage composite = storageImage(composeRegion(baseVisible, *leftVisible, replaceWidth));
            if (!encodeAstc8x8(composite, &entryPayload, error)) {
                return false;
            }
            if (!preserveRightAstcBlocks(targetFile, entry, i, replaceWidth, &entryPayload, error)) {
                return false;
            }
            payload = &entryPayload;
        }

        const int expectedPayloadSize = static_cast<int>(entry.size) - KzbImageHeaderSize - KzbImageFooterSize;
        if (expectedPayloadSize <= 0 || payload->size() != expectedPayloadSize) {
            if (error) {
                *error = QStringLiteral("KZB 图像块大小不匹配: index %1, need %2, got %3")
                        .arg(i).arg(expectedPayloadSize).arg(payload->size());
            }
            return false;
        }

        if (!targetFile.seek(entry.offset + KzbImageHeaderSize)) {
            if (error) {
                *error = QStringLiteral("KZB 图像块定位失败? index %1").arg(i);
            }
            return false;
        }
        if (targetFile.write(*payload) != payload->size()) {
            if (error) {
                *error = QStringLiteral("KZB 图像块写入失败? index %1").arg(i);
            }
            return false;
        }
        ++replaced;
    }

    if (replaced == 0) {
        if (error) {
            *error = QStringLiteral("未找到可写入的左半屏 wallpaper KZB 图像条目");
        }
        return false;
    }

    if (written) {
        *written += replaced;
    }
    reportProgress(progress, 99, QStringLiteral("已写入左半屏 KZB 图像条目 %1 个").arg(replaced));
    return true;
}

bool WallpaperPackager::encodeAstc8x8(const QImage &image, QByteArray *payload, QString *error)
{
    const QString encoder = astcencPath();
    if (encoder.isEmpty()) {
        if (error) {
            *error = QStringLiteral("未找到 astcenc-native.exe，无法生成 KZB ASTC 图像");
        }
        return false;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        if (error) {
            *error = QStringLiteral("无法创建临时目录");
        }
        return false;
    }

    const QString pngPath = QDir(tempDir.path()).filePath(QStringLiteral("left.png"));
    const QString astcPath = QDir(tempDir.path()).filePath(QStringLiteral("left.astc"));
    if (!image.save(pngPath, "PNG", 100)) {
        if (error) {
            *error = QStringLiteral("无法写入临时 PNG: %1").arg(pngPath);
        }
        return false;
    }

    QProcess process;
    process.setProgram(encoder);
    process.setWorkingDirectory(tempDir.path());
    process.setArguments(QStringList()
                         << QStringLiteral("-cl")
                         << QStringLiteral("left.png")
                         << QStringLiteral("left.astc")
                         << QStringLiteral("8x8")
                         << QStringLiteral("-thorough"));
    process.start();
    if (!process.waitForFinished(-1) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            *error = QStringLiteral("ASTC 编码失败: %1 %2")
                    .arg(QString::fromLocal8Bit(process.readAllStandardOutput()),
                         QString::fromLocal8Bit(process.readAllStandardError()));
        }
        return false;
    }

    QFile astc(astcPath);
    if (!astc.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("无法读取 ASTC 编码结果");
        }
        return false;
    }
    const QByteArray bytes = astc.readAll();
    if (bytes.size() <= 16
            || static_cast<unsigned char>(bytes.at(0)) != 0x13
            || static_cast<unsigned char>(bytes.at(1)) != 0xAB
            || static_cast<unsigned char>(bytes.at(2)) != 0xA1
            || static_cast<unsigned char>(bytes.at(3)) != 0x5C) {
        if (error) {
            *error = QStringLiteral("astcenc 输出不是有效 ASTC 文件");
        }
        return false;
    }
    if (payload) {
        *payload = bytes.mid(16);
    }
    return true;
}

QImage WallpaperPackager::opaqueCopy(const QImage &image)
{
    return image.convertToFormat(QImage::Format_RGB32).convertToFormat(QImage::Format_ARGB32);
}

QImage WallpaperPackager::cheapBlur(const QImage &image)
{
    const QSize smallSize(qMax(1, image.width() / 24), qMax(1, image.height() / 24));
    return image.scaled(smallSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
            .scaled(image.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

bool WallpaperPackager::writePng(const QString &path, const QImage &image, int *written, QString *error)
{
    QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error) {
            *error = QStringLiteral("无法创建目录: %1").arg(info.absolutePath());
        }
        return false;
    }
    if (!image.save(path, "PNG", 100)) {
        if (error) {
            *error = QStringLiteral("无法写入 PNG: %1").arg(path);
        }
        return false;
    }
    if (written) {
        ++(*written);
    }
    return true;
}

QImage WallpaperPackager::centerCrop(const QImage &source, int width, int height, bool dim,
                                     int cropFocusX, int cropFocusY, int zoomPercent)
{
    QImage output(width, height, QImage::Format_ARGB32);
    output.fill(Qt::black);

    QPainter painter(&output);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRect crop = sourceCropRect(source.size(),
                                      width / static_cast<qreal>(height),
                                      cropFocusX,
                                      cropFocusY,
                                      zoomPercent);
    painter.drawImage(QRectF(0, 0, width, height), source, crop);

    if (dim) {
        painter.fillRect(output.rect(), QColor(0, 0, 0, 92));
    }

    return output;
}

QImage WallpaperPackager::sharedSideCrop(const QImage &source, int width, int height, bool leftSide, bool dim,
                                         int cropFocusX, int cropFocusY, int zoomPercent)
{
    QImage output(width, height, QImage::Format_ARGB32);
    output.fill(Qt::black);

    QPainter painter(&output);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRect fullCrop = sourceCropRect(source.size(),
                                          DefaultSharedScreenWidth / static_cast<qreal>(DefaultSharedScreenHeight),
                                          cropFocusX,
                                          cropFocusY,
                                          zoomPercent);
    QRectF halfCrop(fullCrop);
    const qreal leftRatio = DefaultSharedLeftWidth / static_cast<qreal>(DefaultSharedScreenWidth);
    const qreal rightRatio = DefaultSharedRightWidth / static_cast<qreal>(DefaultSharedScreenWidth);
    halfCrop.setWidth(fullCrop.width() * (leftSide ? leftRatio : rightRatio));
    if (!leftSide) {
        halfCrop.moveLeft(fullCrop.left() + fullCrop.width() * leftRatio);
    }

    painter.drawImage(QRectF(0, 0, width, height), source, halfCrop);

    if (dim) {
        painter.fillRect(output.rect(), QColor(0, 0, 0, 92));
    }

    return output;
}

QRect WallpaperPackager::sourceCropRect(const QSize &sourceSize, qreal targetAspect,
                                        int cropFocusX, int cropFocusY, int zoomPercent)
{
    if (sourceSize.isEmpty() || targetAspect <= 0) {
        return QRect(QPoint(0, 0), sourceSize);
    }

    const qreal sourceAspect = sourceSize.width() / static_cast<qreal>(sourceSize.height());
    int cropWidth = sourceSize.width();
    int cropHeight = sourceSize.height();

    if (sourceAspect > targetAspect) {
        cropWidth = qRound(sourceSize.height() * targetAspect);
    } else {
        cropHeight = qRound(sourceSize.width() / targetAspect);
    }

    const qreal zoom = qBound(100, zoomPercent, 500) / 100.0;
    cropWidth = qMax(1, qRound(cropWidth / zoom));
    cropHeight = qMax(1, qRound(cropHeight / zoom));

    const int maxX = qMax(0, sourceSize.width() - cropWidth);
    const int maxY = qMax(0, sourceSize.height() - cropHeight);
    const int x = qRound(maxX * qBound(0, cropFocusX, 100) / 100.0);
    const int y = qRound(maxY * qBound(0, cropFocusY, 100) / 100.0);
    return QRect(x, y, cropWidth, cropHeight);
}
