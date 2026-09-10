#pragma once

#include <QImage>
#include <QRect>
#include <QString>

#include <functional>

class WallpaperPackager
{
public:
    struct Request {
        QString imagePath;
        QString darkImagePath;
        QString leftImagePath;
        QString outputRootPath;
        QString packageId;
        QString innerFolderName;
        int leftWidth = 8960;
        int leftHeight = 1320;
        int mainWidth = 3950;
        int mainHeight = 1320;
        int previewWidth = 2198;
        int previewHeight = 367;
        int screenSaverWidth = 1920;
        int screenSaverHeight = 1080;
        int leftCropFocusX = 50;
        int leftCropFocusY = 50;
        int mainCropFocusX = 50;
        int mainCropFocusY = 50;
        int previewCropFocusX = 50;
        int previewCropFocusY = 50;
        int screenSaverCropFocusX = 50;
        int screenSaverCropFocusY = 50;
        int darkLeftCropFocusX = 50;
        int darkLeftCropFocusY = 50;
        int darkMainCropFocusX = 50;
        int darkMainCropFocusY = 50;
        int darkPreviewCropFocusX = 50;
        int darkPreviewCropFocusY = 50;
        int darkScreenSaverCropFocusX = 50;
        int darkScreenSaverCropFocusY = 50;
        int leftZoomPercent = 100;
        int mainZoomPercent = 100;
        int previewZoomPercent = 100;
        int screenSaverZoomPercent = 100;
        int darkLeftZoomPercent = 100;
        int darkMainZoomPercent = 100;
        int darkPreviewZoomPercent = 100;
        int darkScreenSaverZoomPercent = 100;
        bool overwrite = false;
    };

    struct Result {
        bool ok = false;
        QString message;
        QString outputPackagePath;
        int writtenImages = 0;
    };

    struct KzbUnpackResult {
        bool ok = false;
        QString message;
        QString outputPath;
        int entryCount = 0;
        int imageCount = 0;
    };

    using ProgressCallback = std::function<void(int, const QString &)>;

    static QString defaultPackageId();
    static QString defaultInnerFolderName();
    static Result buildFromImage(const Request &request, const ProgressCallback &progress = ProgressCallback());
    static KzbUnpackResult unpackKzb(const QString &kzbPath, const QString &outputRootPath,
                                     const ProgressCallback &progress = ProgressCallback());

private:
    static QString outputPackagePath(const QString &outputRootPath, const QString &packageId);
    static bool copyDirectory(const QString &sourcePath, const QString &targetPath, QString *error);
    static bool writeThemeImages(const QString &innerPath, const QImage &lightSource,
                                 const QImage &darkSource, bool hasExplicitDarkSource,
                                 const Request &request,
                                 int *written, QString *error, const ProgressCallback &progress);
    static bool writeLeftKzbImages(const QString &targetInnerPath, const QImage &lightSource,
                                   const QImage &darkSource, bool hasExplicitDarkSource,
                                   const Request &request, int *written, QString *error,
                                   const ProgressCallback &progress);
    static bool replaceLeftKzbImages(const QString &targetInnerPath, const QString &sourcePath,
                                     int *written, QString *error, const ProgressCallback &progress);
    static bool encodeAstc8x8(const QImage &image, QByteArray *payload, QString *error);
    static QImage opaqueCopy(const QImage &image);
    static QImage cheapBlur(const QImage &image);
    static bool writePng(const QString &path, const QImage &image, int *written, QString *error);
    static QImage centerCrop(const QImage &source, int width, int height, bool dim,
                             int cropFocusX, int cropFocusY, int zoomPercent);
    static QImage sharedSideCrop(const QImage &source, int width, int height, bool leftSide, bool dim,
                                 int cropFocusX, int cropFocusY, int zoomPercent);
    static QRect sourceCropRect(const QSize &sourceSize, qreal targetAspect,
                                int cropFocusX, int cropFocusY, int zoomPercent);
};
