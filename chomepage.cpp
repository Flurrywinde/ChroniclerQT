#include "chomepage.h"

#include <QListWidget>
#include <QListWidgetItem>
#include <QWebView>
#include <QUrl>
#include <QLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QStringList>
#include <QSettings>
#include <QPushButton>
#include <QAction>
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QStatusBar>
#include <QCoreApplication>
#include <QProcess>

#include "cmainwindow.h"

#include "Properties/cprojectview.h"
#include "csettingsview.h"
#include "Misc/qactionbutton.h"

#include "Misc/cfiledownloader.h"

#include "Misc/chronicler.h"
using Chronicler::shared;
using Chronicler::CVersion;

// for QSettings
Q_DECLARE_METATYPE(QStringList)


CHomepage::CHomepage(QWidget *parent)
    : QWidget(parent), m_webView(0), m_recentView(0)
{
    QHBoxLayout *main_layout = new QHBoxLayout(this);

    SetupSidebar(main_layout);
    SetupMainWindow(main_layout);

    setLayout(main_layout);

    shared().statusBar->showMessage("Acquiring the latest news...");

    QString news = "https://www.dropbox.com/s/tru5nkm9m4uukwe/Chronicler_news.html?dl=1";
    m_downloader = new CFileDownloader(QUrl(news), SLOT(NewsDownloaded()), this);
}

void CHomepage::SetupSidebar(QHBoxLayout *main_layout)
{
    m_recentView = new QListWidget();
    QVBoxLayout *layout_recent = new QVBoxLayout();
    QHBoxLayout *layout_buttons = new QHBoxLayout();

    if(shared().settingsView->maxRecentFiles() > 0)
    {
        // load the recently opened projects from the settings
        m_recentView->addItems(shared().settingsView->settings()->value("Homepage/RecentFiles").value<QStringList>());
        connect(m_recentView, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(RecentItemSelected(QListWidgetItem*)));

        layout_recent->addWidget(new QLabel("Recent files"));
        layout_recent->addWidget(m_recentView);
    }

    // Buttons
    const QSize icon_size(32, 32);
    QActionButton *btn_new = new QActionButton(0, shared().newProjectAction);
    btn_new->setIconSize(icon_size);

    QActionButton *btn_load = new QActionButton(0, shared().openProjectAction);
    btn_load->setIconSize(icon_size);

    QActionButton *btn_import = new QActionButton(0, shared().importProjectAction);
    btn_import->setIconSize(icon_size);

    layout_buttons->addWidget(btn_new);
    layout_buttons->addWidget(btn_load);
    layout_buttons->addWidget(btn_import);
    layout_buttons->addStretch(1);
    layout_recent->addLayout(layout_buttons);

    // Add to main layout
    main_layout->addLayout(layout_recent, 1);
}

void CHomepage::SetupMainWindow(QHBoxLayout *main_layout)
{
    m_webView = new QWebView();

    // Add to main layout
    main_layout->addWidget(m_webView, 4);
}

void CHomepage::RecentItemSelected(QListWidgetItem *item)
{
    if(QFileInfo(item->text()).exists())
    {
        // move the selected project to the top of the list
        m_recentView->takeItem(m_recentView->row(item));
        m_recentView->insertItem(0, item);
        item->setSelected(true);

        // save the updated list to settings
        QStringList labels;
        for(int i = 0; i < m_recentView->count() && i < shared().settingsView->maxRecentFiles(); ++i)
            labels << m_recentView->item(i)->text();

        shared().settingsView->settings()->setValue("Homepage/RecentFiles", QVariant::fromValue(labels));

        // load the selected project
        shared().projectView->OpenProject(item->text());
    }
    else
    {
        QMessageBox msgBox;
        msgBox.setText("Selected project file not found.");
        msgBox.setInformativeText("Do you wish to remove this file from recent items?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Yes);

        if(msgBox.exec() == QMessageBox::Yes)
        {
            // remove the non-existant project
            m_recentView->takeItem(m_recentView->row(item));

            // save the updated list to settings
            QStringList labels;
            for(int i = 0; i < m_recentView->count() && i < shared().settingsView->maxRecentFiles(); ++i)
                labels << m_recentView->item(i)->text();

            shared().settingsView->settings()->setValue("Homepage/RecentFiles", QVariant::fromValue(labels));
        }
    }
}

void CHomepage::NewsDownloaded()
{
    QString news = QFileInfo(shared().settingsView->settings()->fileName()).absolutePath() + "/Chronicler_news.html";
    QByteArray ba(m_downloader->downloadedData());

    if(ba.length() > 0)
    {
        QFile file(news);
        file.open(QFile::WriteOnly);
        file.write(ba);
        file.close();
    }

    m_webView->load(QUrl::fromLocalFile(news));

    // Check for updates
    shared().statusBar->showMessage("Checking for updates...");

    m_downloader->deleteLater();
    QString version = "https://www.dropbox.com/s/6y6ozos568pox4b/Chronicler_Version.txt?dl=1";
    m_downloader = new CFileDownloader(QUrl(version), SLOT(CheckForUpdates()), this);
}

void CHomepage::CheckForUpdates()
{
    QString server_string = m_downloader->downloadedData();
    server_string.remove('\r');
    const int first_line = server_string.indexOf("\n");
    QString server_version = server_string.mid(0, first_line);
    QString update_info = server_string.mid(first_line + 1);

    qDebug() << server_string;
    qDebug() << server_version;
    qDebug() << update_info;

    if(shared().ProgramVersion < server_version)
    {
        QMessageBox msgBox;
        msgBox.setText(tr("Version ") + server_version + tr(" available.\n") + update_info);
        msgBox.setInformativeText("Download now?");
        msgBox.setStandardButtons(QMessageBox::Apply | QMessageBox::Ignore);
        msgBox.setDefaultButton(QMessageBox::Apply);
        int ret = msgBox.exec();

        if(ret == QMessageBox::Apply)
        {
            shared().statusBar->showMessage("Downloading update...");

            m_downloader->deleteLater();

            QString update;
#ifdef Q_OS_WIN
            update = "https://www.dropbox.com/s/07e6zjvino75hh5/Chronicler-Next.exe?dl=1";
#endif
#ifdef Q_OS_OSX
            update = "";
#endif
#ifdef Q_OS_LINUX
            update = "https://www.dropbox.com/s/119iwpun4lxo16w/Chronicler-Next?dl=1";
#endif

            if(!update.isEmpty())
                m_downloader = new CFileDownloader(QUrl(update), SLOT(UpdateDownloaded()), this);
            else
                shared().statusBar->showMessage("Unable to determine your current Operating System...");
        }
    }
    else
        shared().statusBar->showMessage("Chronicler is up to date.");
}

void CHomepage::UpdateDownloaded()
{
    QByteArray ba(m_downloader->downloadedData());

    QString update = QFileInfo(shared().settingsView->settings()->fileName()).absolutePath();

#ifdef Q_OS_WIN
            update += "/Chronicler-Next.exe";
#endif
#ifdef Q_OS_OSX
            news +=  "";
#endif
#ifdef Q_OS_LINUX
            news += "Chronicler-Next";
#endif

    if(ba.length() > 0)
    {
        QFile file(update);
        file.open(QFile::WriteOnly);
        file.write(ba);
        file.close();

        QMessageBox msgBox;
        msgBox.setText(tr("Update has been downloaded. Chronicler must be restarted in order to apply the update."));
        msgBox.setInformativeText(tr("Restart now?"));
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Yes);
        int ret = msgBox.exec();

        if(ret == QMessageBox::Yes)
        {
            QString program;

#ifdef Q_OS_WIN
            program = "Chronicler_Updater.exe";
#endif
#ifdef Q_OS_OSX
            program =  "";
#endif
#ifdef Q_OS_LINUX
            program = "Chronicler_Updater";
#endif

            qDebug() << "executing " + QCoreApplication::applicationDirPath() + "/" + program;
            QProcess process;
            process.start(QCoreApplication::applicationDirPath() + "/" + program);

            shared().mainWindow->close();
        }
    }
}

