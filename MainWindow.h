#pragma once

#include <QMainWindow>
#include <QSize>

class QCheckBox;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTextEdit;
class CropPreviewWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void chooseImage();
    void chooseDarkImage();
    void chooseLeftImage();
    void chooseTemplate();
    void chooseLeftKzbSource();
    void chooseOutput();
    void buildPackage();
    void unpackKzbPackage();
    void setBusy(bool busy);
    void setProgress(int value, const QString &text);
    void updateTemplateShapes();
    void updateCropPreview();
    void appendLog(const QString &text);
    QString currentTemplatePackagePath() const;
    QSize readTemplateImageSize(const QString &relativePath, const QSize &fallback) const;
    QString defaultTemplatePath() const;
    QString defaultOutputPath() const;

    QLineEdit *imageEdit = nullptr;
    QLineEdit *darkImageEdit = nullptr;
    QLineEdit *leftImageEdit = nullptr;
    QLineEdit *templateEdit = nullptr;
    QLineEdit *leftKzbSourceEdit = nullptr;
    QLineEdit *outputEdit = nullptr;
    QLineEdit *packageIdEdit = nullptr;
    QLineEdit *innerFolderEdit = nullptr;
    CropPreviewWidget *leftCropPreview = nullptr;
    CropPreviewWidget *mainCropPreview = nullptr;
    CropPreviewWidget *previewCropPreview = nullptr;
    CropPreviewWidget *screenSaverCropPreview = nullptr;
    CropPreviewWidget *darkCropPreview = nullptr;
    CropPreviewWidget *darkPreviewCropPreview = nullptr;
    CropPreviewWidget *darkScreenSaverCropPreview = nullptr;
    QCheckBox *overwriteCheck = nullptr;
    QProgressBar *progressBar = nullptr;
    QTextEdit *logEdit = nullptr;
    QPushButton *buildButton = nullptr;
    QPushButton *unpackKzbButton = nullptr;
    QSize leftTargetSize = QSize(8960, 1320);
    QSize mainTargetSize = QSize(3950, 1320);
    QSize previewTargetSize = QSize(2198, 367);
    QSize screenSaverTargetSize = QSize(1920, 1080);
};
