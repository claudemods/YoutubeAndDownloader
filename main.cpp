#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QWebEngineView>
#include <QProcess>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineFullScreenRequest>
#include <QMessageBox>
#include <QInputDialog>
#include <QDebug>
#include <QStandardPaths>
#include <QWebEngineProfile>

class Downloader : public QObject {
    Q_OBJECT
public:
    explicit Downloader(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE void downloadMP3(const QString &url) {
        download("mp3", url);
    }

    Q_INVOKABLE void downloadMP4(const QString &url) {
        download("mp4", url);
    }

private:
    void download(const QString &format, const QString &url) {
        QString downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (downloadPath.isEmpty()) {
            QMessageBox::warning(nullptr, "Error", "Could not find download location");
            return;
        }

        QStringList args;
        if (format == "mp3") {
            args << "--extract-audio" << "--audio-format" << "mp3" << "-o" << downloadPath + "/%(title)s.%(ext)s" << url;
        } else if (format == "mp4") {
            args << "-f" << "mp4" << "-o" << downloadPath + "/%(title)s.%(ext)s" << url;
        }

        m_process.start("yt-dlp", args);
        if (!m_process.waitForStarted()) {
            qWarning() << "Failed to start yt-dlp process";
            QMessageBox::warning(nullptr, "Error", "Failed to start yt-dlp process");
            return;
        }

        connect(&m_process, &QProcess::readyReadStandardOutput, this, [this]() {
            qDebug() << "yt-dlp output:" << m_process.readAllStandardOutput();
        });

        connect(&m_process, &QProcess::readyReadStandardError, this, [this]() {
            QString errorOutput = m_process.readAllStandardError();
            qDebug() << "yt-dlp error:" << errorOutput;
            if (errorOutput.contains("ERROR")) {
                QMessageBox::warning(nullptr, "Error", "Download failed: " + errorOutput);
            }
        });
    }

    QProcess m_process;
};

class FullScreenWebEnginePage : public QWebEnginePage {
    Q_OBJECT
public:
    FullScreenWebEnginePage(QWebEngineView *view, QWidget *mainWindow, QObject* parent = nullptr)
    : QWebEnginePage(new QWebEngineProfile("YouTubePersistentProfile"), parent),
    m_view(view),
    m_mainWindow(mainWindow)
    {
        // Persistent session configuration
        QWebEngineProfile* profile = this->profile();
        QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

        profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
        profile->setCachePath(dataPath + "/youtube_cache");
        profile->setPersistentStoragePath(dataPath + "/youtube_storage");
        profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);

        // Your original settings
        QWebEngineSettings *settings = this->settings();
        settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
        settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
        settings->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, false);
        settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);

        connect(this, &QWebEnginePage::fullScreenRequested, this, &FullScreenWebEnginePage::fullScreenRequested);
    }

private slots:
    void fullScreenRequested(QWebEngineFullScreenRequest request) {
        if (request.toggleOn()) {
            m_view->setParent(nullptr);
            m_view->showFullScreen();
        } else {
            m_view->setParent(m_mainWindow);
            m_view->showNormal();
        }
        request.accept();
    }

private:
    QWebEngineView *m_view;
    QWidget *m_mainWindow;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setGeometry(100, 100, 800, 600);

    QVBoxLayout layout(&window);

    QWebEngineView webView;
    FullScreenWebEnginePage *page = new FullScreenWebEnginePage(&webView, &window, &webView);
    webView.setPage(page);
    webView.setUrl(QUrl("https://www.youtube.com"));
    layout.addWidget(&webView);

    Downloader downloader;

    QObject::connect(page, &QWebEnginePage::urlChanged, [&](const QUrl &url) {
        if (url.toString().contains("watch")) {
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(&window, "Download", "Do you want to download this video as MP3 or MP4?",
                                          QMessageBox::Yes|QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                QStringList formats = {"mp3", "mp4"};
                bool ok;
                QString format = QInputDialog::getItem(&window, "Select Format", "Choose the format:", formats, 0, false, &ok);
                if (ok && !format.isEmpty()) {
                    if (format == "mp3") {
                        downloader.downloadMP3(url.toString());
                    } else if (format == "mp4") {
                        downloader.downloadMP4(url.toString());
                    }
                }
            }
        }
    });

    window.show();
    return app.exec();
}

#include "main.moc"
