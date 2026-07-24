#include "MainWindow.h"
#include "CropPreviewWidget.h"
#include "WallpaperPackager.h"

#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QImageReader>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QTabWidget>
#include <QTextEdit>
#include <QThread>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("CT5 壁纸包制作工具"));
    resize(1120, 760);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("JPG/JPEG 制作 CT5 壁纸包"), central);
    QFont titleFont = title->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    title->setFont(titleFont);
    root->addWidget(title);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    root->addLayout(form);

    imageEdit = new QLineEdit(central);
    auto *imageRow = new QWidget(central);
    auto *imageLayout = new QHBoxLayout(imageRow);
    imageLayout->setContentsMargins(0, 0, 0, 0);
    imageLayout->addWidget(imageEdit, 1);
    auto *imageButton = new QPushButton(QStringLiteral("选择图片"), imageRow);
    imageLayout->addWidget(imageButton);
    form->addRow(QStringLiteral("JPG/JPEG 图片"), imageRow);

    darkImageEdit = new QLineEdit(central);
    darkImageEdit->setPlaceholderText(QStringLiteral("可选：留空则由浅色图片自动生成深色资源"));
    auto *darkImageRow = new QWidget(central);
    auto *darkImageLayout = new QHBoxLayout(darkImageRow);
    darkImageLayout->setContentsMargins(0, 0, 0, 0);
    darkImageLayout->addWidget(darkImageEdit, 1);
    auto *darkImageButton = new QPushButton(QStringLiteral("选择图片"), darkImageRow);
    darkImageLayout->addWidget(darkImageButton);
    form->addRow(QStringLiteral("深色模式 JPG/JPEG 图片"), darkImageRow);

    templateEdit = new QLineEdit(defaultTemplatePath(), central);
    auto *templateRow = new QWidget(central);
    auto *templateLayout = new QHBoxLayout(templateRow);
    templateLayout->setContentsMargins(0, 0, 0, 0);
    templateLayout->addWidget(templateEdit, 1);
    auto *templateButton = new QPushButton(QStringLiteral("选择模板包"), templateRow);
    templateLayout->addWidget(templateButton);
    form->addRow(QStringLiteral("模板壁纸包"), templateRow);

    leftKzbSourceEdit = new QLineEdit(central);
    leftKzbSourceEdit->setPlaceholderText(QStringLiteral("可选：已有主题包或 .kzb，用于替换左半屏 KZB 图像"));
    auto *leftKzbRow = new QWidget(central);
    auto *leftKzbLayout = new QHBoxLayout(leftKzbRow);
    leftKzbLayout->setContentsMargins(0, 0, 0, 0);
    leftKzbLayout->addWidget(leftKzbSourceEdit, 1);
    auto *leftKzbPackageButton = new QPushButton(QStringLiteral("选择主题包"), leftKzbRow);
    auto *leftKzbFileButton = new QPushButton(QStringLiteral("选择 KZB"), leftKzbRow);
    leftKzbLayout->addWidget(leftKzbPackageButton);
    leftKzbLayout->addWidget(leftKzbFileButton);
    leftKzbSourceEdit->setPlaceholderText(QStringLiteral("可选：左半屏 JPG/JPEG 图片；留空则使用主图"));
    leftKzbPackageButton->setText(QStringLiteral("选择图片"));
    leftKzbFileButton->setText(QStringLiteral("选择图片"));
    form->addRow(QStringLiteral("左半屏图片"), leftKzbRow);
    leftKzbRow->setVisible(false);
    if (QWidget *leftKzbLabel = form->labelForField(leftKzbRow)) {
        leftKzbLabel->setVisible(false);
    }

    outputEdit = new QLineEdit(defaultOutputPath(), central);
    auto *outputRow = new QWidget(central);
    auto *outputLayout = new QHBoxLayout(outputRow);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->addWidget(outputEdit, 1);
    auto *outputButton = new QPushButton(QStringLiteral("选择输出目录"), outputRow);
    outputLayout->addWidget(outputButton);
    form->addRow(QStringLiteral("输出目录"), outputRow);

    packageIdEdit = new QLineEdit(WallpaperPackager::defaultPackageId(), central);
    form->addRow(QStringLiteral("壁纸包目录名"), packageIdEdit);

    innerFolderEdit = new QLineEdit(WallpaperPackager::defaultInnerFolderName(), central);
    form->addRow(QStringLiteral("内部目录名"), innerFolderEdit);

    overwriteCheck = new QCheckBox(QStringLiteral("允许覆盖输出目录中已存在的同名壁纸包"), central);
    overwriteCheck->setChecked(true);
    form->addRow(QString(), overwriteCheck);

    auto *cropTabs = new QTabWidget(central);
    root->addWidget(cropTabs, 1);

    auto *lightTab = new QWidget(cropTabs);
    auto *lightGrid = new QGridLayout(lightTab);
    lightGrid->setContentsMargins(0, 8, 0, 0);
    lightGrid->setSpacing(10);
    cropTabs->addTab(lightTab, QStringLiteral("浅色裁切"));

    leftCropPreview = new CropPreviewWidget(lightTab);
    leftCropPreview->setTarget(QStringLiteral("左右半屏整图"), 8960, 1320);
    leftCropPreview->setVerticalDividerPercent(5010.0 * 100.0 / 8960.0);
    lightGrid->addWidget(leftCropPreview, 0, 0, 1, 2);

    mainCropPreview = new CropPreviewWidget(lightTab);
    mainCropPreview->setTarget(QStringLiteral("右半屏壁纸"), 3950, 1320);
    mainCropPreview->hide();

    previewCropPreview = new CropPreviewWidget(lightTab);
    previewCropPreview->setTarget(QStringLiteral("预览图"), 2198, 367);
    lightGrid->addWidget(previewCropPreview, 1, 0);

    screenSaverCropPreview = new CropPreviewWidget(lightTab);
    screenSaverCropPreview->setTarget(QStringLiteral("屏保"), 1920, 1080);
    lightGrid->addWidget(screenSaverCropPreview, 1, 1);

    auto *darkTab = new QWidget(cropTabs);
    auto *darkGrid = new QGridLayout(darkTab);
    darkGrid->setContentsMargins(0, 8, 0, 0);
    darkGrid->setSpacing(10);
    cropTabs->addTab(darkTab, QStringLiteral("深色裁切"));

    darkCropPreview = new CropPreviewWidget(darkTab);
    darkCropPreview->setTarget(QStringLiteral("左右半屏整图"), 8960, 1320);
    darkCropPreview->setVerticalDividerPercent(5010.0 * 100.0 / 8960.0);
    darkGrid->addWidget(darkCropPreview, 0, 0, 1, 2);

    darkPreviewCropPreview = new CropPreviewWidget(darkTab);
    darkPreviewCropPreview->setTarget(QStringLiteral("预览图"), 2198, 367);
    darkGrid->addWidget(darkPreviewCropPreview, 1, 0);

    darkScreenSaverCropPreview = new CropPreviewWidget(darkTab);
    darkScreenSaverCropPreview->setTarget(QStringLiteral("屏保"), 1920, 1080);
    darkGrid->addWidget(darkScreenSaverCropPreview, 1, 1);

    buildButton = new QPushButton(QStringLiteral("制作壁纸包"), central);
    buildButton->setMinimumHeight(38);
    root->addWidget(buildButton);

    unpackKzbButton = new QPushButton(QStringLiteral("解包 KZB"), central);
    unpackKzbButton->setMinimumHeight(34);
    root->addWidget(unpackKzbButton);

    progressBar = new QProgressBar(central);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setFormat(QStringLiteral("等待制作"));
    root->addWidget(progressBar);

    logEdit = new QTextEdit(central);
    logEdit->setReadOnly(true);
    logEdit->setMaximumHeight(120);
    logEdit->setPlaceholderText(QStringLiteral("制作日志"));
    root->addWidget(logEdit, 1);

    setCentralWidget(central);

    connect(imageButton, &QPushButton::clicked, this, &MainWindow::chooseImage);
    connect(darkImageButton, &QPushButton::clicked, this, &MainWindow::chooseDarkImage);
    connect(templateButton, &QPushButton::clicked, this, &MainWindow::chooseTemplate);
    connect(leftKzbPackageButton, &QPushButton::clicked, this, &MainWindow::chooseLeftImage);
    connect(leftKzbFileButton, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
                    this,
                    QStringLiteral("选择左半屏 JPG/JPEG 图片"),
                    leftKzbSourceEdit->text().isEmpty() ? QStringLiteral("E:/paper") : leftKzbSourceEdit->text(),
                    QStringLiteral("JPEG 图片 (*.jpg *.jpeg)"));
        if (!path.isEmpty()) {
            leftKzbSourceEdit->setText(QDir::toNativeSeparators(path));
        }
    });
    connect(outputButton, &QPushButton::clicked, this, &MainWindow::chooseOutput);
    connect(buildButton, &QPushButton::clicked, this, &MainWindow::buildPackage);
    connect(unpackKzbButton, &QPushButton::clicked, this, &MainWindow::unpackKzbPackage);
    connect(imageEdit, &QLineEdit::textChanged, this, &MainWindow::updateCropPreview);
    connect(darkImageEdit, &QLineEdit::textChanged, this, &MainWindow::updateCropPreview);
    connect(leftKzbSourceEdit, &QLineEdit::textChanged, this, &MainWindow::updateCropPreview);
    connect(templateEdit, &QLineEdit::textChanged, this, &MainWindow::updateTemplateShapes);
    connect(packageIdEdit, &QLineEdit::textChanged, this, &MainWindow::updateTemplateShapes);

    appendLog(QStringLiteral("模板包必须是完整的 BFA3A0F4596C4C57A6BCDC1EB3348932 目录。"));
    appendLog(QStringLiteral("vcd 文件夹是深色/浅色右半屏壁纸，左半屏壁纸在 .kzb 资源包内。"));
    appendLog(QStringLiteral("当前工具会生成右半屏 PNG，并尝试把左半屏图片编码写入 .kzb。"));
    updateTemplateShapes();
    updateCropPreview();
}

void MainWindow::chooseImage()
{
    const QString path = QFileDialog::getOpenFileName(
                this,
                QStringLiteral("选择 JPG/JPEG 图片"),
                QDir::homePath(),
                QStringLiteral("JPEG 图片 (*.jpg *.jpeg)"));
    if (!path.isEmpty()) {
        imageEdit->setText(QDir::toNativeSeparators(path));
    }
}

void MainWindow::chooseDarkImage()
{
    const QString path = QFileDialog::getOpenFileName(
                this,
                QStringLiteral("选择深色模式 JPG/JPEG 图片"),
                darkImageEdit->text().isEmpty() ? QDir::homePath() : darkImageEdit->text(),
                QStringLiteral("JPEG 图片 (*.jpg *.jpeg)"));
    if (!path.isEmpty()) {
        darkImageEdit->setText(QDir::toNativeSeparators(path));
    }
}

void MainWindow::chooseLeftImage()
{
    const QString path = QFileDialog::getOpenFileName(
                this,
                QStringLiteral("选择左半屏 JPG/JPEG 图片"),
                leftKzbSourceEdit->text().isEmpty() ? QDir::homePath() : leftKzbSourceEdit->text(),
                QStringLiteral("JPEG 图片 (*.jpg *.jpeg)"));
    if (!path.isEmpty()) {
        leftKzbSourceEdit->setText(QDir::toNativeSeparators(path));
    }
}

void MainWindow::chooseTemplate()
{
    const QString path = QFileDialog::getExistingDirectory(
                this,
                QStringLiteral("选择完整模板壁纸包目录"),
                templateEdit->text().isEmpty() ? QStringLiteral("E:/paper") : templateEdit->text());
    if (!path.isEmpty()) {
        templateEdit->setText(QDir::toNativeSeparators(path));
        updateTemplateShapes();
    }
}

void MainWindow::chooseLeftKzbSource()
{
    const QString path = QFileDialog::getExistingDirectory(
                this,
                QStringLiteral("选择已有左半屏来源主题包"),
                leftKzbSourceEdit->text().isEmpty() ? QStringLiteral("E:/paper") : leftKzbSourceEdit->text());
    if (!path.isEmpty()) {
        leftKzbSourceEdit->setText(QDir::toNativeSeparators(path));
    }
}

void MainWindow::chooseOutput()
{
    const QString path = QFileDialog::getExistingDirectory(
                this,
                QStringLiteral("选择输出目录"),
                outputEdit->text().isEmpty() ? QStringLiteral("E:/paper/output") : outputEdit->text());
    if (!path.isEmpty()) {
        outputEdit->setText(QDir::toNativeSeparators(path));
    }
}

void MainWindow::buildPackage()
{
    if (buildButton && !buildButton->isEnabled()) {
        return;
    }

    WallpaperPackager::Request request;
    request.imagePath = imageEdit->text();
    request.darkImagePath = darkImageEdit->text();
    request.templatePackagePath = templateEdit->text();
    request.leftImagePath.clear();
    request.outputRootPath = outputEdit->text();
    request.packageId = packageIdEdit->text().trimmed().isEmpty()
            ? WallpaperPackager::defaultPackageId()
            : packageIdEdit->text().trimmed();
    request.innerFolderName = innerFolderEdit->text().trimmed();
    request.leftWidth = leftTargetSize.width();
    request.leftHeight = leftTargetSize.height();
    request.mainWidth = mainTargetSize.width();
    request.mainHeight = mainTargetSize.height();
    request.previewWidth = previewTargetSize.width();
    request.previewHeight = previewTargetSize.height();
    request.screenSaverWidth = screenSaverTargetSize.width();
    request.screenSaverHeight = screenSaverTargetSize.height();
    const QPoint sharedFocus = leftCropPreview->focus();
    const QPoint darkSharedFocus = darkCropPreview->focus();
    const QPoint previewFocus = previewCropPreview->focus();
    const QPoint darkPreviewFocus = darkPreviewCropPreview->focus();
    const QPoint screenSaverFocus = screenSaverCropPreview->focus();
    const QPoint darkScreenSaverFocus = darkScreenSaverCropPreview->focus();
    request.leftCropFocusX = sharedFocus.x();
    request.leftCropFocusY = sharedFocus.y();
    request.mainCropFocusX = sharedFocus.x();
    request.mainCropFocusY = sharedFocus.y();
    request.previewCropFocusX = previewFocus.x();
    request.previewCropFocusY = previewFocus.y();
    request.screenSaverCropFocusX = screenSaverFocus.x();
    request.screenSaverCropFocusY = screenSaverFocus.y();
    request.darkLeftCropFocusX = darkSharedFocus.x();
    request.darkLeftCropFocusY = darkSharedFocus.y();
    request.darkMainCropFocusX = darkSharedFocus.x();
    request.darkMainCropFocusY = darkSharedFocus.y();
    request.darkPreviewCropFocusX = darkPreviewFocus.x();
    request.darkPreviewCropFocusY = darkPreviewFocus.y();
    request.darkScreenSaverCropFocusX = darkScreenSaverFocus.x();
    request.darkScreenSaverCropFocusY = darkScreenSaverFocus.y();
    request.leftZoomPercent = leftCropPreview->zoom();
    request.mainZoomPercent = leftCropPreview->zoom();
    request.previewZoomPercent = previewCropPreview->zoom();
    request.screenSaverZoomPercent = screenSaverCropPreview->zoom();
    request.darkLeftZoomPercent = darkCropPreview->zoom();
    request.darkMainZoomPercent = darkCropPreview->zoom();
    request.darkPreviewZoomPercent = darkPreviewCropPreview->zoom();
    request.darkScreenSaverZoomPercent = darkScreenSaverCropPreview->zoom();
    request.overwrite = overwriteCheck->isChecked();

    setBusy(true);
    setProgress(0, QStringLiteral("准备制作"));
    appendLog(QStringLiteral("开始制作: %1").arg(request.imagePath));
    appendLog(QStringLiteral("模板形状: 右半屏壁纸 %1x%2，预览图 %3x%4，屏保 %5x%6")
              .arg(request.mainWidth).arg(request.mainHeight)
              .arg(request.previewWidth).arg(request.previewHeight)
              .arg(request.screenSaverWidth).arg(request.screenSaverHeight));
    appendLog(QStringLiteral("左右半屏整图裁切: X=%1%, Y=%2%, 缩放=%3%，虚线为中间分界")
              .arg(request.mainCropFocusX).arg(request.mainCropFocusY).arg(request.mainZoomPercent));
    appendLog(QStringLiteral("预览图裁切: X=%1%, Y=%2%, 缩放=%3%")
              .arg(request.previewCropFocusX).arg(request.previewCropFocusY).arg(request.previewZoomPercent));
    appendLog(QStringLiteral("屏保裁切: X=%1%, Y=%2%, 缩放=%3%")
              .arg(request.screenSaverCropFocusX).arg(request.screenSaverCropFocusY).arg(request.screenSaverZoomPercent));

    if (!request.leftImagePath.trimmed().isEmpty()) {
        appendLog(QStringLiteral("左半屏图片? %1").arg(request.leftImagePath));
    }

    QPointer<MainWindow> guard(this);
    QThread *worker = QThread::create([guard, request]() {
        const WallpaperPackager::Result result = WallpaperPackager::buildFromImage(
                    request,
                    [guard](int value, const QString &message) {
            if (!guard) {
                return;
            }
            QMetaObject::invokeMethod(guard.data(), [guard, value, message]() {
                if (guard) {
                    guard->setProgress(value, message);
                }
            }, Qt::QueuedConnection);
        });

        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard.data(), [guard, result]() {
            if (!guard) {
                return;
            }
            guard->setBusy(false);
            guard->appendLog(result.message);
            if (result.ok) {
                guard->setProgress(100, result.message);
                guard->appendLog(QStringLiteral("输出位置: %1").arg(result.outputPackagePath));
                QMessageBox::information(guard.data(), QStringLiteral("完成"),
                                         result.message + "\n" + result.outputPackagePath);
            } else {
                guard->setProgress(0, QStringLiteral("制作失败"));
                QMessageBox::warning(guard.data(), QStringLiteral("失败"), result.message);
            }
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void MainWindow::unpackKzbPackage()
{
    if (unpackKzbButton && !unpackKzbButton->isEnabled()) {
        return;
    }

    const QString kzbPath = QFileDialog::getOpenFileName(
                this,
                QStringLiteral("选择 KZB 文件"),
                QDir::homePath(),
                QStringLiteral("KZB files (*.kzb)"));
    if (kzbPath.isEmpty()) {
        return;
    }

    const QFileInfo kzbInfo(kzbPath);
    const QString defaultOut = kzbInfo.dir().filePath(kzbInfo.completeBaseName() + QStringLiteral("_unpacked"));
    const QString outputPath = QFileDialog::getExistingDirectory(
                this,
                QStringLiteral("选择 KZB 解包输出目录"),
                defaultOut);
    if (outputPath.isEmpty()) {
        return;
    }

    setBusy(true);
    setProgress(0, QStringLiteral("开始解包 KZB"));
    appendLog(QStringLiteral("KZB 输入: %1").arg(QDir::toNativeSeparators(kzbPath)));
    appendLog(QStringLiteral("KZB 输出: %1").arg(QDir::toNativeSeparators(outputPath)));

    QPointer<MainWindow> guard(this);
    QThread *worker = QThread::create([guard, kzbPath, outputPath]() {
        const WallpaperPackager::KzbUnpackResult result = WallpaperPackager::unpackKzb(
                    kzbPath,
                    outputPath,
                    [guard](int value, const QString &message) {
            if (!guard) {
                return;
            }
            QMetaObject::invokeMethod(guard.data(), [guard, value, message]() {
                if (guard) {
                    guard->setProgress(value, message);
                }
            }, Qt::QueuedConnection);
        });

        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard.data(), [guard, result]() {
            if (!guard) {
                return;
            }
            guard->setBusy(false);
            guard->appendLog(result.message);
            if (result.ok) {
                guard->setProgress(100, result.message);
                guard->appendLog(QStringLiteral("KZB 已解包到: %1").arg(result.outputPath));
                QMessageBox::information(guard.data(), QStringLiteral("KZB"),
                                         result.message + "\n" + result.outputPath);
            } else {
                guard->setProgress(0, QStringLiteral("KZB unpack failed"));
                QMessageBox::warning(guard.data(), QStringLiteral("KZB"), result.message);
            }
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void MainWindow::setBusy(bool busy)
{
    buildButton->setEnabled(!busy);
    unpackKzbButton->setEnabled(!busy);
    imageEdit->setEnabled(!busy);
    darkImageEdit->setEnabled(!busy);
    templateEdit->setEnabled(!busy);
    leftKzbSourceEdit->setEnabled(!busy);
    outputEdit->setEnabled(!busy);
    packageIdEdit->setEnabled(!busy);
    innerFolderEdit->setEnabled(!busy);
    overwriteCheck->setEnabled(!busy);
    leftCropPreview->setEnabled(!busy);
    darkCropPreview->setEnabled(!busy);
    mainCropPreview->setEnabled(!busy);
    previewCropPreview->setEnabled(!busy);
    screenSaverCropPreview->setEnabled(!busy);
    darkPreviewCropPreview->setEnabled(!busy);
    darkScreenSaverCropPreview->setEnabled(!busy);
}

void MainWindow::setProgress(int value, const QString &text)
{
    progressBar->setValue(value);
    progressBar->setFormat(QStringLiteral("%1%  %2").arg(value).arg(text));
    if (!text.isEmpty()) {
        appendLog(text);
    }
}

void MainWindow::updateTemplateShapes()
{
    const QSize oldMain = mainTargetSize;
    const QSize oldPreview = previewTargetSize;
    const QSize oldSaver = screenSaverTargetSize;

    mainTargetSize = readTemplateImageSize(
                QStringLiteral("vcd/wallpaper/light_wallpaper_vcd.png"),
                QSize(3950, 1320));
    previewTargetSize = readTemplateImageSize(
                QStringLiteral("light_preview_image.png"),
                QSize(2198, 367));
    screenSaverTargetSize = readTemplateImageSize(
                QStringLiteral("rid/screenSaver/light_screenSaver_rid.png"),
                QSize(1920, 1080));

    mainCropPreview->setTarget(QStringLiteral("右半屏壁纸"), mainTargetSize.width(), mainTargetSize.height());
    leftCropPreview->setTarget(QStringLiteral("左右半屏整图"), 8960, 1320);
    leftCropPreview->setVerticalDividerPercent(5010.0 * 100.0 / 8960.0);
    previewCropPreview->setTarget(QStringLiteral("预览图"), previewTargetSize.width(), previewTargetSize.height());
    screenSaverCropPreview->setTarget(QStringLiteral("屏保"),
                                      screenSaverTargetSize.width(),
                                      screenSaverTargetSize.height());

    darkCropPreview->setTarget(QStringLiteral("深色左右半屏整图"), 8960, 1320);
    darkCropPreview->setVerticalDividerPercent(5010.0 * 100.0 / 8960.0);
    darkPreviewCropPreview->setTarget(QStringLiteral("深色预览图"),
                                      previewTargetSize.width(),
                                      previewTargetSize.height());
    darkScreenSaverCropPreview->setTarget(QStringLiteral("深色屏保"),
                                          screenSaverTargetSize.width(),
                                          screenSaverTargetSize.height());

    if (logEdit && (mainTargetSize != oldMain
                    || previewTargetSize != oldPreview
                    || screenSaverTargetSize != oldSaver)) {
        appendLog(QStringLiteral("已读取模板形状: 右半屏壁纸 %1x%2，预览图 %3x%4，屏保 %5x%6")
                  .arg(mainTargetSize.width()).arg(mainTargetSize.height())
                  .arg(previewTargetSize.width()).arg(previewTargetSize.height())
                  .arg(screenSaverTargetSize.width()).arg(screenSaverTargetSize.height()));
    }
}

void MainWindow::updateCropPreview()
{
    const QString lightPath = imageEdit->text();
    const QString darkPath = darkImageEdit->text().trimmed().isEmpty()
            ? lightPath
            : darkImageEdit->text();
    if (leftCropPreview) {
        leftCropPreview->setImagePath(lightPath);
    }
    if (darkCropPreview) {
        darkCropPreview->setImagePath(darkPath);
    }
    if (mainCropPreview) {
        mainCropPreview->setImagePath(lightPath);
    }
    if (previewCropPreview) {
        previewCropPreview->setImagePath(lightPath);
    }
    if (screenSaverCropPreview) {
        screenSaverCropPreview->setImagePath(lightPath);
    }
    if (darkPreviewCropPreview) {
        darkPreviewCropPreview->setImagePath(darkPath);
    }
    if (darkScreenSaverCropPreview) {
        darkScreenSaverCropPreview->setImagePath(darkPath);
    }
}

QString MainWindow::currentTemplatePackagePath() const
{
    const QString packageId = packageIdEdit->text().trimmed().isEmpty()
            ? WallpaperPackager::defaultPackageId()
            : packageIdEdit->text().trimmed();
    QFileInfo info(templateEdit->text().trimmed());
    if (!info.exists() || !info.isDir()) {
        return QString();
    }
    if (info.fileName().compare(packageId, Qt::CaseInsensitive) == 0) {
        return info.absoluteFilePath();
    }
    if (info.fileName().startsWith("cadi_wallpaper", Qt::CaseInsensitive)) {
        return info.dir().absolutePath();
    }
    const QString child = QDir(info.absoluteFilePath()).filePath(packageId);
    if (QFileInfo(child).isDir()) {
        return QFileInfo(child).absoluteFilePath();
    }
    return info.absoluteFilePath();
}

QSize MainWindow::readTemplateImageSize(const QString &relativePath, const QSize &fallback) const
{
    const QString packagePath = currentTemplatePackagePath();
    if (packagePath.isEmpty()) {
        return fallback;
    }

    QDir packageDir(packagePath);
    const QFileInfoList children = packageDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &child : children) {
        if (!child.fileName().startsWith("cadi_wallpaper", Qt::CaseInsensitive)) {
            continue;
        }
        QImageReader reader(QDir(child.absoluteFilePath()).filePath(relativePath));
        const QSize size = reader.size();
        if (size.isValid()) {
            return size;
        }
    }
    return fallback;
}

void MainWindow::appendLog(const QString &text)
{
    logEdit->append(QStringLiteral("[%1] %2")
                    .arg(QDateTime::currentDateTime().toString("HH:mm:ss"), text));
}

QString MainWindow::defaultTemplatePath() const
{
    const QString path = QStringLiteral("E:/paper/") + WallpaperPackager::defaultPackageId();
    return QDir::toNativeSeparators(path);
}

QString MainWindow::defaultOutputPath() const
{
    return QDir::toNativeSeparators(QStringLiteral("E:/paper/output"));
}
