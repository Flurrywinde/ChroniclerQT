#ifndef CSETTINGSVIEW_H
#define CSETTINGSVIEW_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QSettings;
class QLayout;
class QLineEdit;
class QColor;
class QFontComboBox;
class QSpinBox;
class QPushButton;
class QSize;
class QCheckBox;
QT_END_NAMESPACE


class CSettingsView : public QWidget
{
    Q_OBJECT

public:
    CSettingsView(QSettings *settings, QWidget *parent = 0);

    QSettings *settings();

    QString choiceScriptDirectory();

    QFont font();
    QColor fontColor();

    int maxAutosaves();
    int autosaveInterval();
    int maxUndos();
    bool storeHistoryInProject();
    int maxRecentFiles();

    bool pendingChanges();

    void ApplyPendingChanges();

private:
    // TODO
    // autocomplete snippets
    // show homepage at startup
    // hide/show various warnings (ask to save before closing)

    void SetupChoiceScript(QLayout *main_layout);
    void SetupEditor(QLayout *main_layout);
    void SetupHistory(QLayout *main_layout);

    void LoadSettings();
    void SaveSettings();

    void MarkChanged(bool changed);

    QIcon ColorIcon(const QSize &size, const QColor &color);

    QSettings       *m_settings;

    QLineEdit       *m_csdir;

    QFontComboBox   *m_fontCombo;
    QSpinBox        *m_fontSize;
    QPushButton     *m_fontColorButton;
    QFont            m_font;
    QColor           m_fontColor;

    QSpinBox        *m_autosaves;
    QSpinBox        *m_autosave_interval;
    QSpinBox        *m_undos;
    QCheckBox       *m_history;
    QSpinBox        *m_recent_files;

    QPushButton     *m_apply;
    QPushButton     *m_cancel;

signals:
    void SettingsChanged();

private slots:
    void CSDirButtonPressed();
    void FontColorButtonPressed();

    void SettingsApplied();
    void SettingsCanceled();

    void SettingChanged();

    void FontChanged();
    void FontColorSelected(const QColor &);
};

#endif // CSETTINGSVIEW_H
