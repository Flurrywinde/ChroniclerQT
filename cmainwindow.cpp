#include "cmainwindow.h"

#include <QtWidgets>
#include <QStringListModel>
#include <QtGlobal>
#include <limits>
#include <QTabWidget>
#include <QSettings>
#include <QMessageBox>
#include <QStatusBar>
#include <QMimeData>
#include <QClipboard>
#include <QCursor>
#include <QWebView>

#include "cgraphicsscene.h"
#include "Bubbles/cstorybubble.h"
#include "Bubbles/cstartbubble.h"
#include "Bubbles/cstartherebubble.h"
#include "cgraphicsview.h"
#include "chomepage.h"
#include "Properties/cdockmanager.h"
#include "csettingsview.h"
#include "Properties/cprojectview.h"
#include "Connections/cconnection.h"
#include "Misc/History/cremovebubblescommand.h"

#include "Properties/cpalettecreator.h"
#include "Misc/Palette/cpalettebutton.h"
#include "Misc/Palette/cpaletteaction.h"

#include "Misc/chronicler.h"
using Chronicler::shared;


CMainWindow::CMainWindow(QSettings *settings, const QString &filename)
{
    setWindowTitle(tr("Chronicler ") + shared().ProgramVersion.string);
    setUnifiedTitleAndToolBarOnMac(false);

    shared().mainWindow = this;

    shared().statusBar = new QStatusBar(this);
    setStatusBar(shared().statusBar);

    shared().history = new QUndoStack(this);

    // Load the settings...
    shared().settingsView = new CSettingsView(settings);
    connect(shared().settingsView, SIGNAL(SettingsChanged()),
            this, SLOT(SettingsChanged()));
    SettingsChanged();

    CreateActions();
    CreateMenus();

    shared().sceneTabs = new QTabWidget(this);
    shared().sceneTabs->setMovable(true);
    shared().sceneTabs->setTabsClosable(true);
    connect(shared().sceneTabs, SIGNAL(tabCloseRequested(int)), this, SLOT(TabClosed(int)));

    shared().homepage = new CHomepage(this);
    shared().sceneTabs->addTab(shared().homepage, tr("Homepage"));

    setCentralWidget(shared().sceneTabs);

    CreateToolbars();

    shared().dock = new QDockWidget(tr("Project"), this);
    connect(shared().dock, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)),
            this, SLOT(DockAreaChanged(Qt::DockWidgetArea)));

    shared().dockManager = new CDockManager(shared().dock);
    shared().dock->setWidget(shared().dockManager);
    shared().dock->setVisible(false);
    shared().dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    // load the last dock widget area from the settings
    Qt::DockWidgetArea area = static_cast<Qt::DockWidgetArea>(shared().settingsView->settings()->value("MainWindow/DockArea",
                                                                                                       static_cast<int>(Qt::LeftDockWidgetArea)).toInt());
    addDockWidget(area, shared().dock);

    // to allow .chron files to be opened with Chronicler
    if(filename.length())
        shared().projectView->OpenProject(filename);
}


void CMainWindow::DeleteSelectedItems()
{
    shared().dockManager->setBubble(0);

    CGraphicsView *view = dynamic_cast<CGraphicsView *>(shared().sceneTabs->currentWidget());

    if(view)
    {
        QList<CBubble *> bubbles;

        for(QGraphicsItem *item : view->cScene()->selectedItems())
        {
            CBubble *bubble = dynamic_cast<CBubble *>(item);
            if(bubble && bubble->getType() != Chronicler::StartBubble)
                bubbles.append(bubble);
        }

        if(!bubbles.isEmpty())
            shared().history->push(new CRemoveBubblesCommand(view->cScene(), bubbles));
    }
}

void CMainWindow::CopySelectedItems()
{
    CGraphicsView *view = dynamic_cast<CGraphicsView *>(shared().sceneTabs->currentWidget());

    if(view)
    {
        // deselect start bubble
        bool start_selected = view->cScene()->startBubble()->isSelected();
        view->cScene()->startBubble()->setSelected(false);

        if(view->cScene()->selectedItems().length())
        {
            QMimeData *data = new QMimeData();
            QByteArray ba;
            QDataStream ds(&ba, QIODevice::WriteOnly);
            QPointF topLeft = view->cScene()->selectedItems().first()->pos();

            // serialize bubbles & calculate top left
            ds << static_cast<qint32>(view->cScene()->selectedItems().length());
            for(QGraphicsItem *item : view->cScene()->selectedItems())
            {
                CBubble *bubble = dynamic_cast<CBubble *>(item);
                if(bubble)
                {
                    ds << *bubble;

                    if(bubble->pos().x() < topLeft.x() && bubble->pos().y() < topLeft.y())
                        topLeft = bubble->pos();
                }
            }
            ds << topLeft;

            // copy to clipboard
            data->setData("application/clip-bubbles", ba);
            QApplication::clipboard()->setMimeData(data);
        }

        view->cScene()->startBubble()->setSelected(start_selected);
    }
}

void CMainWindow::PasteItems()
{
    CGraphicsView *view = dynamic_cast<CGraphicsView *>(shared().sceneTabs->currentWidget());

    if(view)
    {
        QDataStream ds(QApplication::clipboard()->mimeData()->data("application/clip-bubbles"));

        if(!ds.atEnd())
        {
            qint32 len, bubble_type;
            CBubble *bbl;

            // deselect all
            for(QGraphicsItem *item : view->cScene()->selectedItems())
                item->setSelected(false);

            // deserialize bubbles
            ds >> len;
            for(int i = 0; i < len; ++i)
            {
                ds >> bubble_type;
                bbl = view->cScene()->AddBubble(Chronicler::BubbleType(bubble_type), QPointF(), false);
                bbl->setSelected(true);
                ds >> *bbl;
            }

            // calculate & move items to new offset
            QPointF newPoint = view->mapToScene(view->mapFromGlobal(QCursor::pos()));
            QPointF oldPoint;
            ds >> oldPoint;
            int diffx = newPoint.x() - oldPoint.x();
            int diffy = newPoint.y() - oldPoint.y();

            for(QGraphicsItem *item : view->cScene()->selectedItems())
                item->moveBy(diffx, diffy);

            // hook up deserialized connections (more efficient if done after move)
            for(CConnection *connection : view->cScene()->connections())
                if(!connection->isConnected())
                    connection->ConnectToUIDs(true);

            for(QGraphicsItem *item : view->cScene()->selectedItems())
            {
                if((bbl = dynamic_cast<CBubble *>(item)))
                    bbl->UpdateUID();
            }
        }
    }
}


void CMainWindow::PointerGroupClicked(int id)
{
    shared().setMode(Chronicler::Mode(id));
}

void CMainWindow::NewProject()
{
    shared().projectView->NewProject();
}

void CMainWindow::OpenProject()
{
    shared().projectView->OpenProject();
}

void CMainWindow::ImportProject()
{
    shared().projectView->ImportChoiceScript();
}

void CMainWindow::ImportScene()
{
    shared().projectView->ImportChoiceScriptScene();
}

void CMainWindow::SaveProject()
{
    shared().projectView->SaveProject();
}

void CMainWindow::SaveAsProject()
{
    shared().projectView->SaveProjectAs();
}

void CMainWindow::ShowSettings()
{
    if(shared().sceneTabs->indexOf(shared().settingsView) == -1)
        shared().sceneTabs->insertTab(0, shared().settingsView, tr("Settings"));

    shared().sceneTabs->setCurrentWidget(shared().settingsView);
}

void CMainWindow::ShowHomepage()
{
    if(shared().sceneTabs->indexOf(shared().homepage) == -1)
        shared().sceneTabs->insertTab(0, shared().homepage, tr("Homepage"));

    shared().sceneTabs->setCurrentWidget(shared().homepage);
}

void CMainWindow::ShowDock()
{
    shared().dock->setVisible(!shared().dock->isVisible());
}

void CMainWindow::ShowForum()
{
    QDesktopServices::openUrl(QUrl("https://forum.choiceofgames.com/t/tool-chronicler-choicescript-visual-code-editor/6811"));
}

void CMainWindow::TabClosed(int index)
{
    if(shared().sceneTabs->widget(index) == shared().settingsView)
    {
        if(shared().settingsView->pendingChanges())
        {
            QCheckBox dontShow(tr("Remember my choice and don't show again."));
            dontShow.blockSignals(true); // performance
            QMessageBox *msgBox = new QMessageBox();
            msgBox->setText(tr("Settings have been modified."));
            msgBox->setInformativeText(tr("Do you wish to apply these changes?"));
            msgBox->setStandardButtons(QMessageBox::Apply | QMessageBox::Discard | QMessageBox::Cancel);
            msgBox->setDefaultButton(QMessageBox::Apply);
            msgBox->setCheckBox(&dontShow);
            int ret = msgBox->exec();
            msgBox->deleteLater();

            if(ret == QMessageBox::Apply)
                shared().settingsView->ApplyPendingChanges();
            else if(ret == QMessageBox::Cancel)
                return;

            // TODO
            // save dontShow value...
        }
    }

    shared().sceneTabs->removeTab(index);
}

void CMainWindow::DockAreaChanged(Qt::DockWidgetArea area)
{
    // reflect this change in the settings.
    shared().settingsView->settings()->setValue("MainWindow/DockArea", static_cast<int>(area));
}

void CMainWindow::PointerToolBarAreaChanged(bool)
{
    // reflect this change in the settings.
    shared().settingsView->settings()->setValue("MainWindow/ToolBarArea", static_cast<int>(toolBarArea(shared().pointerToolBar)));
}

void CMainWindow::EscapePressed()
{
    shared().setMode(Chronicler::Cursor);
}

void CMainWindow::PlayProject()
{
    shared().projectView->PlayProject();
}

void CMainWindow::DebugProject()
{
    CStartHereBubble *debugStart = Q_NULLPTR;

    for(CGraphicsView *view : shared().projectView->getViews())
    {
        for(QGraphicsItem *item : view->cScene()->selectedItems())
        {
            if((debugStart = dynamic_cast<CStartHereBubble *>(item)))
            {
                shared().projectView->PlayProject(debugStart);
                return;
            }
        }
    }
}

void CMainWindow::QuickTest()
{
    QString cs_dir = shared().settingsView->choiceScriptDirectory();
    if(QDir(cs_dir + "/web").exists())
    {
        shared().projectView->ExportChoiceScript(cs_dir + "/web/mygame", Q_NULLPTR);

        QDesktopServices::openUrl(QUrl::fromLocalFile(cs_dir + "/quicktest.html"));
    }
    else
    {
        QMessageBox msgBox;
        msgBox.setText(tr("ChoiceScript settings are not valid."));
        msgBox.exec();

        if(shared().sceneTabs->indexOf(shared().settingsView) == -1)
            shared().sceneTabs->insertTab(0, shared().settingsView, tr("Settings"));

        shared().sceneTabs->setCurrentWidget(shared().settingsView);
    }
}

void CMainWindow::RandomTest()
{
    QString cs_dir = shared().settingsView->choiceScriptDirectory();
    if(QDir(cs_dir + "/web").exists())
    {
        shared().projectView->ExportChoiceScript(cs_dir + "/web/mygame", Q_NULLPTR);

        QDesktopServices::openUrl(QUrl::fromLocalFile(cs_dir + "/randomtest.html"));
    }
    else
    {
        QMessageBox msgBox;
        msgBox.setText(tr("ChoiceScript settings are not valid."));
        msgBox.exec();

        if(shared().sceneTabs->indexOf(shared().settingsView) == -1)
            shared().sceneTabs->insertTab(0, shared().settingsView, tr("Settings"));

        shared().sceneTabs->setCurrentWidget(shared().settingsView);
    }
}

void CMainWindow::Compile()
{
    QString cs_dir = shared().settingsView->choiceScriptDirectory();
    if(QDir(cs_dir + "/web").exists())
    {
        shared().projectView->ExportChoiceScript(cs_dir + "/web/mygame", Q_NULLPTR);

        QDesktopServices::openUrl(QUrl::fromLocalFile(cs_dir + "/compile.html"));
    }
    else
    {
        QMessageBox msgBox;
        msgBox.setText(tr("ChoiceScript settings are not valid."));
        msgBox.exec();

        if(shared().sceneTabs->indexOf(shared().settingsView) == -1)
            shared().sceneTabs->insertTab(0, shared().settingsView, tr("Settings"));

        shared().sceneTabs->setCurrentWidget(shared().settingsView);
    }
}

void CMainWindow::SettingsChanged()
{
    // Update font and font color.
    setFont(shared().settingsView->font());

    QPalette pal = palette();
    pal.setColor(QPalette::WindowText, shared().settingsView->fontColor());
    pal.setColor(QPalette::Text, shared().settingsView->fontColor());
    setPalette(pal);

    // Update history
    shared().history->setUndoLimit(shared().settingsView->maxUndos());
}

void CMainWindow::ShowAbout()
{
    QMessageBox::about(this, tr("About Chronicler-Next"), tr("<b>Insert legal stuff here...</b>"));
}


void CMainWindow::CreateActions()
{
    // Edit
    shared().copyAction = new QAction(QIcon(":/images/icn_copy"), tr("&Copy"), this);
    shared().copyAction->setShortcut(QKeySequence::Copy);
    shared().copyAction->setToolTip(tr("Copy selected bubbles to the clipboard"));
    connect(shared().copyAction, SIGNAL(triggered()), this, SLOT(CopySelectedItems()));

    shared().pasteAction = new QAction(QIcon(":/images/icn_paste"), tr("&Paste"), this);
    shared().pasteAction->setShortcut(QKeySequence::Paste);
    shared().pasteAction->setToolTip(tr("Paste bubbles from the clipboard"));
    connect(shared().pasteAction, SIGNAL(triggered()), this, SLOT(PasteItems()));

    shared().deleteAction = new QAction(QIcon(":/images/icn_trash.png"), tr("&Delete"), this);
    shared().deleteAction->setShortcut(QKeySequence::Delete);
    shared().deleteAction->setToolTip(tr("Delete selected bubbles"));
    connect(shared().deleteAction, SIGNAL(triggered()), this, SLOT(DeleteSelectedItems()));

    shared().undoAction = shared().history->createUndoAction(this);
    shared().undoAction->setIcon(QIcon(":/images/icn_undo"));
    shared().undoAction->setShortcut(QKeySequence::Undo);

    shared().redoAction = shared().history->createRedoAction(this);
    shared().redoAction->setIcon(QIcon(":/images/icn_redo"));
    shared().redoAction->setShortcut(QKeySequence::Redo);

    // Help
    shared().aboutAction = new QAction(QIcon(":/images/icn_info"), tr("&About"), this);
    connect(shared().aboutAction, SIGNAL(triggered()), this, SLOT(ShowAbout()));

    shared().forumAction = new QAction(QIcon(":/images/icn_cslogo"), tr("Visit &forum"), this);
    connect(shared().forumAction, SIGNAL(triggered()), this, SLOT(ShowForum()));

    // File
    shared().exitAction = new QAction(QIcon(":/images/icn_exit.png"), tr("E&xit"), this);
    shared().exitAction->setShortcuts(QKeySequence::Quit);
    shared().exitAction->setToolTip(tr("Quit program"));
    connect(shared().exitAction, SIGNAL(triggered()), this, SLOT(close()));

    shared().settingsAction = new QAction(QIcon(":/images/icn_settings"), tr("&Settings"), this);
    shared().settingsAction->setShortcut(QKeySequence::Preferences);
    connect(shared().settingsAction, SIGNAL(triggered(bool)), this, SLOT(ShowSettings()));

    shared().newProjectAction = new QAction(QIcon(":/images/icn_new"), tr("New Project"), this);
    shared().newProjectAction->setShortcut(QKeySequence::New);
    shared().newProjectAction->setToolTip(tr("Create New Project"));
    connect(shared().newProjectAction, SIGNAL(triggered(bool)), this, SLOT(NewProject()));

    shared().openProjectAction = new QAction(QIcon(":/images/icn_load"), tr("Open Project"), this);
    shared().openProjectAction->setShortcut(QKeySequence::Open);
    shared().openProjectAction->setToolTip(tr("Open Existing Project"));
    connect(shared().openProjectAction, SIGNAL(triggered(bool)), this, SLOT(OpenProject()));

    shared().importProjectAction = new QAction(QIcon(":/images/icn_loadcs"), tr("Import Project"), this);
    shared().importProjectAction->setToolTip(tr("Import ChoiceScript Project"));
    connect(shared().importProjectAction, SIGNAL(triggered(bool)), this, SLOT(ImportProject()));

    shared().importSceneAction = new QAction(QIcon(":/images/icn_loadcs"), tr("Import Scene"), this);
    shared().importSceneAction->setToolTip(tr("Import Single ChoiceScript Scene"));
    shared().importSceneAction->setEnabled(false);
    connect(shared().importSceneAction, SIGNAL(triggered(bool)), this, SLOT(ImportScene()));

    shared().saveProjectAction = new QAction(QIcon(":/images/icn_save"), tr("Save"), this);
    shared().saveProjectAction->setShortcut(QKeySequence::Save);
    shared().saveProjectAction->setToolTip(tr("Save Project"));
    connect(shared().saveProjectAction, SIGNAL(triggered(bool)), this, SLOT(SaveProject()));

    shared().saveAsProjectAction = new QAction(QIcon(":/images/icn_savecs"), tr("Save As"), this);
    shared().saveAsProjectAction->setShortcut(QKeySequence::SaveAs);
    shared().saveAsProjectAction->setToolTip(tr("Save Project As"));
    connect(shared().saveAsProjectAction, SIGNAL(triggered(bool)), this, SLOT(SaveAsProject()));

    shared().showHomepageAction = new QAction(QIcon(":/images/icn_home"), tr("Show &homepage"), this);
    connect(shared().showHomepageAction, SIGNAL(triggered(bool)), this, SLOT(ShowHomepage()));

    shared().showDockAction = new QAction(tr("Show &dock"), this);
    connect(shared().showDockAction, SIGNAL(triggered(bool)), this, SLOT(ShowDock()));

    // Tools
    QString testToolTip = tr("Before running, make sure your web directory is clean and the default program for opening .html files is NOT Google Chrome.");
    shared().quickTestAction = new QAction(tr("&Quick test"), this);
    shared().quickTestAction->setToolTip(testToolTip);
    connect(shared().quickTestAction, SIGNAL(triggered(bool)), this, SLOT(QuickTest()));

    shared().randomTestAction = new QAction(tr("&Random test"), this);
    shared().randomTestAction->setToolTip(testToolTip);
    connect(shared().randomTestAction, SIGNAL(triggered(bool)), this, SLOT(RandomTest()));

    shared().compileAction = new QAction(tr("&Compile"), this);
    shared().compileAction->setToolTip(testToolTip);
    connect(shared().compileAction, SIGNAL(triggered(bool)), this, SLOT(Compile()));

    shared().playAction = new QAction(QIcon(":/images/icn_play"), tr("&Play"), this);
    connect(shared().playAction, SIGNAL(triggered(bool)), this, SLOT(PlayProject()));

    shared().debugAction = new QAction(QIcon(":/images/icn_debug"), tr("&Debug"), this);
    shared().debugAction->setEnabled(false);
    connect(shared().debugAction, SIGNAL(triggered(bool)), this, SLOT(DebugProject()));

    // Other
    shared().escapeAction = new QAction(this);
    shared().escapeAction->setShortcut(Qt::Key_Escape);
    connect(shared().escapeAction, SIGNAL(triggered(bool)), this, SLOT(EscapePressed()));
    addAction(shared().escapeAction);
}


void CMainWindow::CreateMenus()
{
    shared().fileMenu = menuBar()->addMenu(tr("&File"));
    shared().fileMenu->setToolTipsVisible(true);
    shared().fileMenu->addAction(shared().newProjectAction);
    shared().fileMenu->addAction(shared().openProjectAction);
    shared().fileMenu->addAction(shared().importProjectAction);
    shared().fileMenu->addAction(shared().importSceneAction);
    shared().fileMenu->addSeparator();
    shared().fileMenu->addAction(shared().saveProjectAction);
    shared().fileMenu->addAction(shared().saveAsProjectAction);
    shared().fileMenu->addSeparator();
    shared().fileMenu->addAction(shared().settingsAction);
    shared().fileMenu->addSeparator();
    shared().fileMenu->addAction(shared().exitAction);

    shared().editMenu = menuBar()->addMenu(tr("&Edit"));
    shared().editMenu->setToolTipsVisible(true);
    shared().editMenu->addAction(shared().undoAction);
    shared().editMenu->addAction(shared().redoAction);
    shared().editMenu->addSeparator();
    shared().editMenu->addAction(shared().copyAction);
    shared().editMenu->addAction(shared().pasteAction);
    shared().editMenu->addAction(shared().deleteAction);    

    shared().viewMenu = menuBar()->addMenu(tr("&View"));
    shared().viewMenu->setToolTipsVisible(true);
    shared().viewMenu->addAction(shared().showHomepageAction);
    shared().viewMenu->addAction(shared().showDockAction);

    shared().toolsMenu = menuBar()->addMenu(tr("&Tools"));
    shared().toolsMenu->setToolTipsVisible(true);
    shared().toolsMenu->addAction(shared().playAction);
    shared().toolsMenu->addAction(shared().debugAction);
    shared().toolsMenu->addSeparator();
    shared().toolsMenu->addAction(shared().quickTestAction);
    shared().toolsMenu->addAction(shared().randomTestAction);
    shared().toolsMenu->addAction(shared().compileAction);

    shared().helpMenu = menuBar()->addMenu(tr("&Help"));
    shared().helpMenu->setToolTipsVisible(true);
    shared().helpMenu->addAction(shared().forumAction);
    shared().helpMenu->addAction(shared().aboutAction);
}


void CMainWindow::CreateToolbars()
{

    // Pointer and link
    QToolButton *tb_pointer = new QToolButton();
    tb_pointer->setCheckable(true);
    tb_pointer->setChecked(true);
    tb_pointer->setIcon(QIcon(":/images/icn_pointer.png"));
    tb_pointer->setToolTip(tr("Selection tool"));

    QToolButton *tb_link = new QToolButton();
    tb_link->setCheckable(true);
    tb_link->setIcon(QIcon(":/images/icn_link.png"));
    tb_link->setToolTip(tr("Link tool\nLeft drag: create link\nRight click: remove link"));


    // Bubbles
    QToolButton *tb_story = new QToolButton();
    tb_story->setCheckable(true);
    tb_story->setIcon(QIcon(":/images/icn_story.png"));
    tb_story->setToolTip(tr("Story bubble"));

    QToolButton *tb_condition = new QToolButton();
    tb_condition->setCheckable(true);
    tb_condition->setIcon(QIcon(":/images/icn_condition.png"));
    tb_condition->setToolTip(tr("Condition bubble\nLeft anchor: TRUE\nRight anchor: FALSE"));

    QToolButton *tb_action = new QToolButton();
    tb_action->setCheckable(true);
    tb_action->setIcon(QIcon(":/images/icn_action.png"));
    tb_action->setToolTip(tr("Action bubble"));

    QToolButton *tb_choice = new QToolButton();
    tb_choice->setCheckable(true);
    tb_choice->setIcon(QIcon(":/images/icn_choice2.png"));
    tb_choice->setToolTip(tr("Choice bubble"));

    QToolButton *tb_code = new QToolButton();
    tb_code->setCheckable(true);
    tb_code->setIcon(QIcon(":/images/icn_code.png"));
    tb_code->setToolTip(tr("Code bubble"));

    QToolButton *tb_startHere = new QToolButton();
    tb_startHere->setCheckable(true);
    tb_startHere->setIcon(QIcon(":/images/icn_startHere.png"));
    tb_startHere->setToolTip(tr("Start Here bubble"));

    // Palette creator
    shared().paletteButton = new CPaletteButton();

    // Pointer group
    shared().pointerTypeGroup = new QButtonGroup(this);
    shared().pointerTypeGroup->addButton(tb_pointer, int(Chronicler::Cursor));
    shared().pointerTypeGroup->addButton(tb_link, int(Chronicler::InsertConnection));
    shared().pointerTypeGroup->addButton(tb_story, int(Chronicler::InsertStory));
    shared().pointerTypeGroup->addButton(tb_condition, int(Chronicler::InsertCondition));
    shared().pointerTypeGroup->addButton(tb_choice, int(Chronicler::InsertChoice));
    shared().pointerTypeGroup->addButton(tb_action, int(Chronicler::InsertAction));
    shared().pointerTypeGroup->addButton(tb_code, int(Chronicler::InsertCode));
    shared().pointerTypeGroup->addButton(tb_startHere, int(Chronicler::InsertStartHere));
    shared().pointerTypeGroup->addButton(shared().paletteButton, int(Chronicler::Paint));
    connect(shared().pointerTypeGroup, SIGNAL(buttonClicked(int)),
            this, SLOT(PointerGroupClicked(int)));

    // Playtest
    QToolButton *tb_play = new QToolButton();
    tb_play->setIcon(QIcon(":/images/icn_play.png"));
    tb_play->setToolTip(tr("Play game"));
    connect(tb_play, SIGNAL(clicked(bool)), this, SLOT(PlayProject()));

    shared().debugButton = new QToolButton();
    shared().debugButton->setIcon(QIcon(":/images/icn_debug.png"));
    shared().debugButton->setToolTip(tr("Debug game"));
    shared().debugButton->setEnabled(false);
    connect(shared().debugButton, SIGNAL(clicked(bool)), this, SLOT(DebugProject()));

    // Pointer toolbar
    Qt::ToolBarArea area = static_cast<Qt::ToolBarArea>(shared().settingsView->settings()->value("MainWindow/ToolBarArea",
                                                                                                 static_cast<int>(Qt::RightToolBarArea)).toInt());
    shared().pointerToolBar = new QToolBar(tr("Pointer type"));
    shared().pointerToolBar->setVisible(false);
    shared().pointerToolBar->addWidget(tb_pointer);
    shared().pointerToolBar->addWidget(tb_link);
    shared().pointerToolBar->addWidget(tb_story);
    shared().pointerToolBar->addWidget(tb_choice);
    shared().pointerToolBar->addWidget(tb_action);
    shared().pointerToolBar->addWidget(tb_condition);
    shared().pointerToolBar->addWidget(tb_code);
    shared().pointerToolBar->addWidget(tb_startHere);
    shared().pointerToolBar->addSeparator();
    shared().pointerToolBar->addWidget(shared().paletteButton);
    shared().pointerToolBar->addSeparator();
    shared().pointerToolBar->addWidget(tb_play);
    shared().pointerToolBar->addWidget(shared().debugButton);
    shared().pointerToolBar->setIconSize(QSize(32,32));
    addToolBar(area, shared().pointerToolBar);
    connect(shared().pointerToolBar, SIGNAL(topLevelChanged(bool)), this, SLOT(PointerToolBarAreaChanged(bool)));
}

void CMainWindow::closeEvent(QCloseEvent *event)
{
    shared().settingsView->settings()->setValue("MainWindow/Geometry", geometry());
    event->accept();
}
