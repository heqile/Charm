/*
  ActivityReport.cpp

  This file is part of Charm, a task-based time tracking application.

  Copyright (C) 2014-2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

  Author: Frank Osterfeld <frank.osterfeld@kdab.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "ActivityReport.h"
#include "ApplicationCore.h"
#include "DateEntrySyncer.h"
#include "Data.h"
#include "SelectTaskDialog.h"
#include "ViewHelpers.h"

#include "Core/Configuration.h"
#include "Core/Dates.h"

#include <QCalendarWidget>
#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QPushButton>
#include <QTimer>
#include <QtAlgorithms>
#include <QUrl>

#include "ui_ActivityReportConfigurationDialog.h"

ActivityReportConfigurationDialog::ActivityReportConfigurationDialog( QWidget* parent )
    : ReportConfigurationDialog( parent )
    , m_ui( new Ui::ActivityReportConfigurationDialog )
{
    setWindowTitle( tr( "Activity Report" ) );

    m_ui->setupUi( this );
    m_ui->dateEditEnd->calendarWidget()->setFirstDayOfWeek( Qt::Monday );
    m_ui->dateEditEnd->calendarWidget()->setVerticalHeaderFormat( QCalendarWidget::ISOWeekNumbers );
    m_ui->dateEditStart->calendarWidget()->setFirstDayOfWeek( Qt::Monday );
    m_ui->dateEditStart->calendarWidget()->setVerticalHeaderFormat( QCalendarWidget::ISOWeekNumbers );

    connect( m_ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()) );
    connect( m_ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()) );
    connect( m_ui->comboBox, SIGNAL(currentIndexChanged(int)),
             SLOT(slotTimeSpanSelected(int)) );
    connect( m_ui->addExcludeTaskButton, SIGNAL(clicked()),
             SLOT(slotExcludeTask()) );
    connect( m_ui->removeExcludeTaskButton, SIGNAL(clicked()),
             SLOT(slotRemoveExcludedTask()) );
    connect( m_ui->addIncludeTaskButton, SIGNAL(clicked()),
             SLOT(slotSelectTask()) );
    connect( m_ui->removeIncludeTaskButton, SIGNAL(clicked()),
             SLOT(slotRemoveIncludeTask()) );

    new DateEntrySyncer(m_ui->spinBoxStartWeek, m_ui->spinBoxStartYear, m_ui->dateEditStart, 1,  this );
    new DateEntrySyncer(m_ui->spinBoxEndWeek, m_ui->spinBoxEndYear, m_ui->dateEditEnd, 7, this );

    QTimer::singleShot( 0, this, SLOT(slotDelayedInitialization()) );
}

ActivityReportConfigurationDialog::~ActivityReportConfigurationDialog()
{
}

void ActivityReportConfigurationDialog::slotDelayedInitialization()
{
    slotStandardTimeSpansChanged();
    connect( ApplicationCore::instance().dateChangeWatcher(),
             SIGNAL(dateChanged()),
             SLOT(slotStandardTimeSpansChanged()) );
    // FIXME load settings
}

void ActivityReportConfigurationDialog::slotStandardTimeSpansChanged()
{
    const TimeSpans timeSpans;
    m_timespans = timeSpans.standardTimeSpans();
    NamedTimeSpan customRange = {
        tr( "Select Range" ),
        timeSpans.thisWeek().timespan,
        Range
    };
    m_timespans << customRange;
    m_ui->comboBox->clear();
    for ( int i = 0; i < m_timespans.size(); ++i )
    {
        m_ui->comboBox->addItem( m_timespans[i].name );
    }
}

void ActivityReportConfigurationDialog::slotTimeSpanSelected( int index )
{
    if ( m_ui->comboBox->count() == 0 || index == -1 ) return;
    Q_ASSERT( m_ui->comboBox->count() > index );
    if ( index == m_timespans.size() - 1 ) { // manual selection
        m_ui->groupBox->setEnabled( true );
    } else {
        m_ui->spinBoxStartYear->setValue( m_timespans[index].timespan.first.year() );
        m_ui->spinBoxStartWeek->setValue( m_timespans[index].timespan.first.weekNumber() );
        m_ui->spinBoxEndYear->setValue( m_timespans[index].timespan.second.year() );
        m_ui->spinBoxEndWeek->setValue( m_timespans[index].timespan.second.weekNumber() );
        m_ui->dateEditStart->setDate( m_timespans[index].timespan.first );
        m_ui->dateEditEnd->setDate( m_timespans[index].timespan.second );
        m_ui->groupBox->setEnabled( false );
    }
}

void ActivityReportConfigurationDialog::slotSelectTask()
{
    TaskId taskId;
    if ( selectTask( taskId ) && !m_rootTasks.contains(taskId)) {
        const TaskTreeItem& item = DATAMODEL->taskTreeItem( taskId );
        QListWidgetItem* listItem = new QListWidgetItem( Data::charmIcon(),
                                                         DATAMODEL->fullTaskName( item.task() ),
                                                         m_ui->listWidgetIncludeTask );
        listItem->setData( Qt::UserRole, taskId );
        m_rootTasks << taskId;
    }
    m_ui->removeIncludeTaskButton->setEnabled( !m_rootTasks.isEmpty() );
    m_ui->listWidgetIncludeTask->setEnabled( !m_rootTasks.isEmpty() );

}

void ActivityReportConfigurationDialog::slotExcludeTask()
{
    TaskId taskId;
    if ( selectTask( taskId ) && !m_rootExcludeTasks.contains( taskId ) ) {
        const TaskTreeItem& item = DATAMODEL->taskTreeItem( taskId );
        QListWidgetItem* listItem = new QListWidgetItem( Data::charmIcon(),
                                                         DATAMODEL->fullTaskName( item.task() ),
                                                         m_ui->listWidgetExclude );
        listItem->setData( Qt::UserRole, taskId );
        m_rootExcludeTasks << taskId;
    }
    m_ui->removeExcludeTaskButton->setEnabled( !m_rootExcludeTasks.isEmpty() );
    m_ui->listWidgetExclude->setEnabled( !m_rootExcludeTasks.isEmpty() );
}

void ActivityReportConfigurationDialog::slotRemoveExcludedTask()
{
    QListWidgetItem* item = m_ui->listWidgetExclude->currentItem();
    if ( item ) {
        m_rootExcludeTasks.remove( item->data( Qt::UserRole ).toInt() );
        delete item;
    }
    m_ui->removeExcludeTaskButton->setEnabled( !m_rootExcludeTasks.isEmpty() );
    m_ui->listWidgetExclude->setEnabled( !m_rootExcludeTasks.isEmpty() );
}

void ActivityReportConfigurationDialog::slotRemoveIncludeTask()
{
    QListWidgetItem* item = m_ui->listWidgetIncludeTask->currentItem();
    if ( item ) {
        m_rootTasks.remove( item->data( Qt::UserRole ).toInt() );
        delete item;
    }
    m_ui->removeIncludeTaskButton->setEnabled( !m_rootTasks.isEmpty() );
    m_ui->listWidgetIncludeTask->setEnabled( !m_rootTasks.isEmpty() );
}

bool ActivityReportConfigurationDialog::selectTask(TaskId& task)
{
    SelectTaskDialog dialog( this );
    dialog.setNonTrackableSelectable();
    const bool taskSelected = dialog.exec();
    if ( taskSelected )
        task = dialog.selectedTask();
    return taskSelected;
}

void ActivityReportConfigurationDialog::accept()
{
    // FIXME save settings
    QDialog::accept();
}

void ActivityReportConfigurationDialog::showReportPreviewDialog()
{
    QDate start, end;
    const int index = m_ui->comboBox->currentIndex();
    if ( index == m_timespans.size() - 1 ) { //Range
        start = m_ui->dateEditStart->date();
        end = m_ui->dateEditEnd->date().addDays( 1 );
    } else {
        start = m_timespans[index].timespan.first;
        end = m_timespans[index].timespan.second;
    }

    auto report = new ActivityReport();
    report->timeSpanSelection(  m_timespans[index] );
    report->setReportProperties( start, end, m_rootTasks, m_rootExcludeTasks );
    report->show();
}

ActivityReport::ActivityReport( QWidget* parent )
    : ReportPreviewWindow( parent )
{
    saveToXmlButton()->hide();
    saveToTextButton()->hide();
    uploadButton()->hide();
    connect( this, SIGNAL(anchorClicked(QUrl)), SLOT(slotLinkClicked(QUrl)) );
}

ActivityReport::~ActivityReport()
{
}

void ActivityReport::setReportProperties( const QDate& start, const QDate& end, QSet<TaskId> rootTasks, QSet<TaskId> rootExcludeTasks )
{
    m_start = start;
    m_end = end;
    m_rootTasks = rootTasks;
    m_rootExcludeTasks = rootExcludeTasks;
    slotUpdate();
}

void ActivityReport::timeSpanSelection( NamedTimeSpan timeSpanSelection )
{
    m_timeSpanSelection = timeSpanSelection;
}

void ActivityReport::slotUpdate()
{
    // retrieve matching events:
    EventIdList matchingEvents = DATAMODEL->eventsThatStartInTimeFrame( m_start, m_end );
    
    if( !m_rootTasks.isEmpty() ) {
        QSet<EventId> filteredEvents;
        Q_FOREACH( TaskId include,  m_rootTasks ) {
           filteredEvents |= Charm::filteredBySubtree( matchingEvents, include ).toSet();
        }
        matchingEvents = filteredEvents.toList();
    }
    matchingEvents = Charm::eventIdsSortedByStartTime( matchingEvents );

    // filter unproductive events:
    Q_FOREACH( TaskId exclude,  m_rootExcludeTasks ) {
        matchingEvents = Charm::filteredBySubtree( matchingEvents, exclude, true );
    }

    // calculate total:
    int totalSeconds = 0;
    Q_FOREACH( EventId id, matchingEvents ) {
        const Event& event = DATAMODEL->eventForId( id );
        Q_ASSERT( event.isValid() );
        totalSeconds += event.duration();
    }

    // which TimeSpan type
    QString timeSpanTypeName;
    switch( m_timeSpanSelection.timeSpanType ) {
    case Day:
        timeSpanTypeName = tr ( "Day");
        break;
    case Week:
        timeSpanTypeName = tr ( "Week" );
        break;
    case Month:
        timeSpanTypeName = tr ( "Month" );
        break;
    case Year:
        timeSpanTypeName = tr ( "Year" );
        break;
    case Range:
        timeSpanTypeName = tr ( "Range" );
        break;
    default:
        Q_ASSERT( false ); // should not happen
    }

    auto report = new QTextDocument( this );
    QDomDocument doc = createReportTemplate();
    QDomElement root = doc.documentElement();
    QDomElement body = root.firstChildElement( QStringLiteral("body") );

    // create the caption:
    {
        QDomElement headline = doc.createElement( QStringLiteral("h1") );
        QDomText text = doc.createTextNode( tr( "Activity Report" ) );
        headline.appendChild( text );
        body.appendChild( headline );
    }
    {
        QDomElement headline = doc.createElement( QStringLiteral("h3") );
        QString content = tr( "Report for %1, from %2 to %3" )
                          .arg( CONFIGURATION.user.name(),
                                m_start.toString( Qt::TextDate ),
                                m_end.toString( Qt::TextDate ) );
        QDomText text = doc.createTextNode( content );
        headline.appendChild( text );
        body.appendChild( headline );
        QDomElement previousLink = doc.createElement( QStringLiteral("a") );
        previousLink.setAttribute( QStringLiteral("href") , QStringLiteral("Previous") );
        QDomText previousLinkText = doc.createTextNode( tr( "<Previous %1>" ).arg( timeSpanTypeName ) );
        previousLink.appendChild( previousLinkText );
        body.appendChild( previousLink );
        QDomElement nextLink = doc.createElement( QStringLiteral("a") );
        nextLink.setAttribute( QStringLiteral("href") , QStringLiteral("Next") );
        QDomText nextLinkText = doc.createTextNode( tr( "<Next %1>" ).arg( timeSpanTypeName ) );
        nextLink.appendChild( nextLinkText );
        body.appendChild( nextLink );
        {
            QDomElement paragraph = doc.createElement( QStringLiteral("h4") );
            QString totalsText = tr( "Total: %1" ).arg( hoursAndMinutes( totalSeconds ) );
            QDomText totalsElement = doc.createTextNode( totalsText );
            paragraph.appendChild( totalsElement );
            body.appendChild( paragraph );
        }
        if ( !m_rootTasks.isEmpty() ) {
            QDomElement paragraph = doc.createElement( QStringLiteral("p") );
            QString rootTaskText = tr( "Activity under tasks:" );

            Q_FOREACH( TaskId taskId, m_rootTasks ) {
                const Task& task = DATAMODEL->getTask( taskId );
                rootTaskText.append( QStringLiteral( " ( %1 ),").arg( DATAMODEL->fullTaskName( task ) ) );
            }
            rootTaskText = rootTaskText.mid(0, rootTaskText.length() - 1 );
            QDomText rootText = doc.createTextNode( rootTaskText );
            paragraph.appendChild( rootText );
            body.appendChild( paragraph );
        }

        QDomElement paragraph = doc.createElement( QStringLiteral("br") );
        body.appendChild( paragraph );
    }
    {
        const QString Headlines[] = {
            tr( "Date and Time, Task, Description" )
        };
        const int NumberOfColumns = sizeof Headlines / sizeof Headlines[0];

        // now for a table
        QDomElement table = doc.createElement( QStringLiteral("table") );
        table.setAttribute( QStringLiteral("width"), QStringLiteral("100%") );
        table.setAttribute( QStringLiteral("align"), QStringLiteral("left") );
        table.setAttribute( QStringLiteral("cellpadding"), QStringLiteral("3") );
        table.setAttribute( QStringLiteral("cellspacing"), QStringLiteral("0") );
        body.appendChild( table );
        // table header
        QDomElement tableHead = doc.createElement( QStringLiteral("thead") );
        table.appendChild( tableHead );
        QDomElement headerRow = doc.createElement( QStringLiteral("tr") );
        headerRow.setAttribute( QStringLiteral("class"), QStringLiteral("header_row") );
        tableHead.appendChild( headerRow );
        // column headers
        for ( int i = 0; i < NumberOfColumns; ++i )
        {
            QDomElement header = doc.createElement( QStringLiteral("th") );
            QDomText text = doc.createTextNode( Headlines[i] );
            header.appendChild( text );
            headerRow.appendChild( header );
        }
        QDomElement tableBody = doc.createElement( QStringLiteral("tbody") );
        table.appendChild( tableBody );
        // rows
        Q_FOREACH( EventId id, matchingEvents ) {
            const Event& event = DATAMODEL->eventForId( id );
            Q_ASSERT( event.isValid() );
            const TaskTreeItem& item = DATAMODEL->taskTreeItem( event.taskId() );
            const Task& task = item.task();
            Q_ASSERT( task.isValid() );

            const auto paddedId = QStringLiteral("%1").arg( QString::number( task.id() ).trimmed(), Configuration::instance().taskPaddingLength, QLatin1Char('0') );

            const QString row1Texts[] = {
                tr( "%1 %2-%3 (%4) -- [%5] %6" )
                .arg( event.startDateTime().date().toString( Qt::SystemLocaleShortDate ).trimmed(),
                      event.startDateTime().time().toString( Qt::SystemLocaleShortDate ).trimmed(),
                      event.endDateTime().time().toString( Qt::SystemLocaleShortDate ).trimmed(),
                      hoursAndMinutes( event.duration() ),
                      paddedId,
                      task.name().trimmed() )
            };

            QDomElement row1 = doc.createElement( QStringLiteral("tr") );
            row1.setAttribute( QStringLiteral("class"), QStringLiteral("event_attributes_row") );
            QDomElement row2 = doc.createElement( QStringLiteral("tr") );
            for ( int index = 0; index < NumberOfColumns; ++index ) {
                QDomElement cell = doc.createElement( QStringLiteral("td") );
                cell.setAttribute( QStringLiteral("class"), QStringLiteral("event_attributes") );
                QDomText text = doc.createTextNode( row1Texts[index] );
                cell.appendChild( text );
                row1.appendChild( cell );
            }
            QDomElement cell2 = doc.createElement( QStringLiteral("td") );
            cell2.setAttribute( QStringLiteral("class"), QStringLiteral("event_description") );
            cell2.setAttribute( QStringLiteral("align"), QStringLiteral("left") );
            QDomElement preElement = doc.createElement( QStringLiteral("pre") );
            QDomText preText = doc.createTextNode( event.comment() );
            preElement.appendChild( preText );
            cell2.appendChild( preElement );
            row2.appendChild( cell2 );

            tableBody.appendChild( row1 );
            tableBody.appendChild( row2 );
        }
    }

    // NOTE: seems like the style sheet has to be set before the html
    // code is pushed into the QTextDocument
    report->setDefaultStyleSheet(Charm::reportStylesheet(palette()));

    report->setHtml( doc.toString() );
    setDocument( report );
}

void ActivityReport::slotLinkClicked( const QUrl& which )
{
    QDate start, end;
    switch( m_timeSpanSelection.timeSpanType ) {
    case Day: {
        start = which.toString() == QLatin1String("Previous") ? m_start.addDays( -1 ) : m_start.addDays( 1 );
        end = which.toString() == QLatin1String("Previous") ? m_end.addDays( -1 ) : m_end.addDays( 1 );
    }
    break;
    case Week: {
        start = which.toString() == QLatin1String("Previous") ? m_start.addDays( -7 ) : m_start.addDays( 7 );
        end = which.toString() == QLatin1String("Previous") ? m_end.addDays( -7 ) : m_end.addDays( 7 );
    }
    break;
    case Month: {
        start = which.toString() == QLatin1String("Previous") ? m_start.addMonths( -1 ) : m_start.addMonths( 1 );
        end = which.toString() == QLatin1String("Previous") ? m_end.addMonths( -1 ) : m_end.addMonths( 1 );
    }
    case Year: {
        start = which.toString() == QLatin1String("Previous") ? m_start.addYears( -1 ) : m_start.addYears( 1 );
        end = which.toString() == QLatin1String("Previous") ? m_end.addYears( -1 ) : m_end.addYears( 1 );
    }
    break;
    case Range: {
        int spanRange = m_start.daysTo(m_end);
        start = which.toString() == QLatin1String("Previous") ? m_start.addDays( -spanRange ) : m_start.addDays( spanRange );
        end = which.toString() == QLatin1String("Previous") ? m_end.addDays( -spanRange ) : m_end.addDays( spanRange );
    }
    break;
    default:
        Q_ASSERT( false ); // should not happen
    }
    setReportProperties( start, end, m_rootTasks, m_rootExcludeTasks );
}
#include "moc_ActivityReport.cpp"
