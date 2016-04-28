#include "cmainwindow.h"

#include <QtWidgets>
#include <QStringListModel>
#include <QtGlobal>
#include <limits>
#include <QTabWidget>
#include <QFileInfo>
#include <QSettings>
#include <QMessageBox>

#include "Connections/arrow.h"
#include "cgraphicsscene.h"
#include "Bubbles/cstorybubble.h"
#include "cgraphicsview.h"
#include "chomepage.h"
#include "Properties/cdockmanager.h"
#include "csettingsview.h"


const int InsertTextButton = 10;


CMainWindow::CMainWindow(QSettings *settings)
    : m_ShiftHeld(false), m_settings(settings), m_settingsView(0)
{
    setWindowTitle(tr("Chronicler-Next"));
    setUnifiedTitleAndToolBarOnMac(true);

    CreateActions();
    CreateMenus();


    m_scene = new CGraphicsScene(m_itemMenu, this);
    connect(m_scene, SIGNAL(itemInserted(CBubble*)),
            this, SLOT(ItemInserted(CBubble*)));
    connect(m_scene, SIGNAL(itemSelected(QGraphicsItem*)),
            this, SLOT(ItemSelected(QGraphicsItem*)));
    connect(m_scene, SIGNAL(leftReleased()),
            this, SLOT(SceneLeftReleased()));
    connect(m_scene, SIGNAL(leftPressed()),
            this, SLOT(SceneLeftPressed()));

    m_view = new CGraphicsView(m_scene);

    QStringList lst = QStringList() << "*set" << "*action" << "*create" << "*if" << "*elseif" << "${name}" << "${title}" << "${strength}";
    QStringListModel * lstModel = new QStringListModel(lst, this);

    m_dock = new QDockWidget("Project", this);
    connect(m_dock, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)),
            this, SLOT(DockAreaChanged(Qt::DockWidgetArea)));

    m_dockManager = new CDockManager(lstModel, m_dock);
    m_dock->setWidget(m_dockManager);

    // load the last dock widget area from the settings
    Qt::DockWidgetArea area = static_cast<Qt::DockWidgetArea>(m_settings->value("MainWindow/DockArea", static_cast<int>(Qt::LeftDockWidgetArea)).toInt());
    addDockWidget(area, m_dock);

    m_dock->setVisible(false);
    m_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    CHomepage *home = new CHomepage(this, m_settings);


    m_tabView = new QTabWidget(this);
    m_tabView->setMovable(true);
    m_tabView->setTabsClosable(true);
    connect(m_tabView, SIGNAL(tabCloseRequested(int)),
            this, SLOT(TabClosed(int)));

    m_tabView->addTab(home,"Homepage");

    //m_tabView->addTab(m_view, "startup.scn");


    setCentralWidget(m_tabView);

    CreateToolbars();

    m_settingsView = new CSettingsView(m_settings);
    SettingsChanged();
    delete m_settingsView;
    m_settingsView = 0;
}

void CMainWindow::LoadProject(const QString &filepath)
{
    m_dock->setVisible(true);
    m_dock->setWindowTitle(QFileInfo(filepath).fileName());
    m_tabView->addTab(m_view, "startup.scn");
    m_tabView->setCurrentWidget(m_view);
}


void CMainWindow::keyPressEvent(QKeyEvent *evt)
{
    QMainWindow::keyPressEvent(evt);

    if(evt->key() == Qt::Key_Shift)
        m_ShiftHeld = true;
    else if(evt->key() == Qt::Key_Delete)
        DeleteItem();
}

void CMainWindow::keyReleaseEvent(QKeyEvent *evt)
{
    QMainWindow::keyReleaseEvent(evt);

    if(evt->key() == Qt::Key_Shift)
    {
        m_ShiftHeld = false;
        m_pointerTypeGroup->button(int(CGraphicsScene::Cursor))->setChecked(true);
        m_scene->setMode(CGraphicsScene::Cursor);
        m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    }
}


void CMainWindow::DeleteItem()
{
    m_dockManager->setBubble(0);

    foreach (QGraphicsItem *item, m_scene->selectedItems())
        delete item;
}


void CMainWindow::PointerGroupClicked(int id)
{
    m_scene->setMode(CGraphicsScene::Mode(id));

    if(id == int(CGraphicsScene::InsertLine))
        m_view->setDragMode(QGraphicsView::NoDrag);
    else
        m_view->setDragMode(QGraphicsView::ScrollHandDrag);
}


void CMainWindow::ItemInserted(CBubble *)
{
    if(!m_ShiftHeld)
    {
        m_pointerTypeGroup->button(int(CGraphicsScene::Cursor))->setChecked(true);
        m_scene->setMode(CGraphicsScene::Cursor);
        m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    }
}


void CMainWindow::ItemSelected(QGraphicsItem *selectedItem)
{
    if(!m_scene->isRubberBandSelecting())
    {
        foreach (QGraphicsItem *item, m_scene->items())
            item->setZValue(item->zValue() - qPow(1, -10));

        selectedItem->setZValue(1);
    }
}

void CMainWindow::ShowSettings()
{
    if(m_settingsView)
    {
        m_tabView->setCurrentWidget(m_settingsView);
    }
    else
    {
        m_settingsView = new CSettingsView(m_settings);
        connect(m_settingsView, SIGNAL(SettingsChanged()),
                this, SLOT(SettingsChanged()));

        m_tabView->addTab(m_settingsView, "Settings");
        m_tabView->setCurrentWidget(m_settingsView);
    }
}


void CMainWindow::SceneLeftPressed()
{
    //m_dockManager->setBubble(0, true);
}

void CMainWindow::SceneLeftReleased()
{
    if(!m_dock->isHidden())
        m_dock->activateWindow();
    QList<QGraphicsItem *> selected = m_scene->selectedItems();
    if(selected.size() == 1)
    {
        CBubble *bbl = dynamic_cast<CBubble *>(selected.first());
        m_dockManager->setBubble(bbl);
    }
    else
        m_dockManager->setBubble(0);
}

void CMainWindow::TabClosed(int index)
{
    if(m_tabView->widget(index) == m_settingsView)
    {
        if(m_settingsView->pendingChanges())
        {
            QCheckBox dontShow("Remember my choice and don't show again.");
            dontShow.blockSignals(true);
            QMessageBox msgBox;
            msgBox.setText("Settings have been modified.");
            msgBox.setInformativeText("Do you wish to apply these changes?");
            msgBox.setStandardButtons(QMessageBox::Apply | QMessageBox::Discard | QMessageBox::Cancel);
            msgBox.setDefaultButton(QMessageBox::Apply);
            msgBox.setCheckBox(&dontShow);
            int ret = msgBox.exec();

            if(ret == QMessageBox::Apply)
                m_settingsView->ApplyPendingChanges();
            else if(ret == QMessageBox::Cancel)
                return;

            // TODO
            // save dontShow value...
        }

        delete m_settingsView;
        m_settingsView = 0;
    }

    m_tabView->removeTab(index);
}

void CMainWindow::DockAreaChanged(Qt::DockWidgetArea area)
{
    // reflect this change in the settings.
    m_settings->setValue("MainWindow/DockArea", static_cast<int>(area));
}

void CMainWindow::ToolBarAreaChanged(bool)
{
    m_settings->setValue("MainWindow/ToolBarArea", static_cast<int>(toolBarArea(m_pointerToolBar)));
}

void CMainWindow::SettingsChanged()
{
    setFont(m_settingsView->font());

    QPalette pal = palette();
    pal.setColor(QPalette::WindowText, m_settingsView->fontColor());
    pal.setColor(QPalette::Text, m_settingsView->fontColor());
    setPalette(pal);
}


void CMainWindow::ShowAbout()
{
    QMessageBox::about(this, tr("About Chronicler-Next"), tr("<b>Insert legal stuff here...</b>"));
}


void CMainWindow::CreateActions()
{
    m_deleteAction = new QAction(QIcon(":/images/delete.png"), tr("&Delete"), this);
    m_deleteAction->setShortcut(tr("Delete"));
    m_deleteAction->setStatusTip(tr("Delete selected bubble(S)"));
    connect(m_deleteAction, SIGNAL(triggered()), this, SLOT(DeleteItem()));

    m_exitAction = new QAction(tr("E&xit"), this);
    m_exitAction->setShortcuts(QKeySequence::Quit);
    m_exitAction->setStatusTip(tr("Quit program"));
    connect(m_exitAction, SIGNAL(triggered()), this, SLOT(close()));

    m_settingsAction = new QAction(tr("&Settings"), this);
    m_settingsAction->setShortcut(tr("Ctrl+P"));
    connect(m_settingsAction, SIGNAL(triggered(bool)), this, SLOT(ShowSettings()));

    m_aboutAction = new QAction(tr("A&bout"), this);
    connect(m_aboutAction, SIGNAL(triggered()), this, SLOT(ShowAbout()));
}


void CMainWindow::CreateMenus()
{
    m_fileMenu = menuBar()->addMenu(tr("&File"));
    m_fileMenu->addAction(m_exitAction);

    m_editMenu = menuBar()->addMenu(tr("&Edit"));
    m_editMenu->addAction(m_settingsAction);

    m_itemMenu = menuBar()->addMenu(tr("&Item"));
    m_itemMenu->addAction(m_deleteAction);
    m_itemMenu->addSeparator();

    m_aboutMenu = menuBar()->addMenu(tr("&Help"));
    m_aboutMenu->addAction(m_aboutAction);
}


void CMainWindow::CreateToolbars()
{
    QToolButton *pointerButton = new QToolButton;
    pointerButton->setCheckable(true);
    pointerButton->setChecked(true);
    pointerButton->setIcon(QIcon(":/images/pointer.png"));
    QToolButton *linePointerButton = new QToolButton;
    linePointerButton->setCheckable(true);
    linePointerButton->setIcon(QIcon(":/images/linepointer.png"));


    QToolButton *storyBubbleToolButton = new QToolButton;
    storyBubbleToolButton->setCheckable(true);
    storyBubbleToolButton->setIcon(QIcon(":/images/icn_story.png"));
    QToolButton *conditionBubbleToolButton = new QToolButton;
    conditionBubbleToolButton->setCheckable(true);
    conditionBubbleToolButton->setIcon(QIcon(":/images/icn_condition.png"));
    QToolButton *actionBubbleToolButton = new QToolButton;
    actionBubbleToolButton->setCheckable(true);
    actionBubbleToolButton->setIcon(QIcon(":/images/icn_choice.png"));


    m_pointerTypeGroup = new QButtonGroup(this);
    m_pointerTypeGroup->addButton(pointerButton, int(CGraphicsScene::Cursor));
    m_pointerTypeGroup->addButton(linePointerButton, int(CGraphicsScene::InsertLine));
    m_pointerTypeGroup->addButton(storyBubbleToolButton, int(CGraphicsScene::InsertStory));
    m_pointerTypeGroup->addButton(conditionBubbleToolButton, int(CGraphicsScene::InsertCondition));
    m_pointerTypeGroup->addButton(actionBubbleToolButton, int(CGraphicsScene::InsertChoice));
    connect(m_pointerTypeGroup, SIGNAL(buttonClicked(int)),
            this, SLOT(PointerGroupClicked(int)));

    Qt::ToolBarArea area = static_cast<Qt::ToolBarArea>(m_settings->value("MainWindow/ToolBarArea", static_cast<int>(Qt::TopToolBarArea)).toInt());
    m_pointerToolBar = new QToolBar("Pointer type");
    m_pointerToolBar->addWidget(pointerButton);
    m_pointerToolBar->addWidget(linePointerButton);
    m_pointerToolBar->addWidget(storyBubbleToolButton);
    m_pointerToolBar->addWidget(conditionBubbleToolButton);
    m_pointerToolBar->addWidget(actionBubbleToolButton);
    addToolBar(area, m_pointerToolBar);
    connect(m_pointerToolBar, SIGNAL(topLevelChanged(bool)),
            this, SLOT(ToolBarAreaChanged(bool)));
}


QIcon CMainWindow::CreateColorToolButtonIcon(const QString &imageFile, QColor color)
{
    QPixmap pixmap(50, 80);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    QPixmap image(imageFile);
    // Draw icon centred horizontally on button.
    QRect target(4, 0, 42, 43);
    QRect source(0, 0, 42, 43);
    painter.fillRect(QRect(0, 60, 50, 80), color);
    painter.drawPixmap(target, image, source);

    return QIcon(pixmap);
}


QIcon CMainWindow::CreateColorIcon(QColor color)
{
    QPixmap pixmap(20, 20);
    QPainter painter(&pixmap);
    painter.setPen(Qt::NoPen);
    painter.fillRect(QRect(0, 0, 20, 20), color);

    return QIcon(pixmap);
}
