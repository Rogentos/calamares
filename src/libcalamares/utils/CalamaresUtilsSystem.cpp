/* === This file is part of Calamares - <http://github.com/calamares> ===
 *
 *   Copyright 2014, Teo Mrnjavac <teo@kde.org>
 *
 *   Calamares is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Calamares is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Calamares. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CalamaresUtilsSystem.h"

#include "utils/Logger.h"
#include "JobQueue.h"
#include "GlobalStorage.h"

#include <QDir>
#include <QProcess>
#include <QRegularExpression>

namespace CalamaresUtils
{

int
mount( const QString& devicePath,
       const QString& mountPoint,
       const QString& filesystemName,
       const QString& options )
{
    if ( devicePath.isEmpty() || mountPoint.isEmpty() )
        return -3;

    QDir mountPointDir( mountPoint );
    if ( !mountPointDir.exists() )
    {
        bool ok = mountPointDir.mkpath( mountPoint );
        if ( !ok )
            return -3;
    }

    QString program( "mount" );
    QStringList args = { devicePath, mountPoint };

    if ( !filesystemName.isEmpty() )
        args << "-t" << filesystemName;

    if ( !options.isEmpty() )
        args << "-o" << options;

    return QProcess::execute( program, args );
}

int
chrootCall( const QStringList& args,
            const QString& workingPath,
            const QString& stdInput,
            int timeoutSec )
{
    QString discard;
    return chrootOutput( args,
                         discard,
                         workingPath,
                         stdInput,
                         timeoutSec );
}


int
chrootCall( const QString& command,
            const QString& workingPath,
            const QString& stdInput,
            int timeoutSec )
{
    return chrootCall( QStringList{ command },
                       workingPath,
                       stdInput,
                       timeoutSec );
}


int
chrootOutput( const QStringList& args,
              QString& output,
              const QString& workingPath,
              const QString& stdInput,
              int timeoutSec )
{
    output.clear();

    if ( !Calamares::JobQueue::instance() )
        return -3;

    Calamares::GlobalStorage* gs = Calamares::JobQueue::instance()->globalStorage();
    if ( !gs || !gs->contains( "rootMountPoint" ) )
    {
        cLog() << "No rootMountPoint in global storage";
        return -3;
    }

    QString destDir = gs->value( "rootMountPoint" ).toString();
    if ( !QDir( destDir ).exists() )
    {
        cLog() << "rootMountPoint points to a dir which does not exist";
        return -3;
    }

    QString program( "chroot" );
    QStringList arguments = { destDir };
    arguments << args;

    QProcess process;
    process.setProgram( program );
    process.setArguments( arguments );
    process.setProcessChannelMode( QProcess::MergedChannels );

    if ( !workingPath.isEmpty() )
    {
        if ( QDir( workingPath ).exists() )
            process.setWorkingDirectory( QDir( workingPath ).absolutePath() );
        else
            cLog() << "Invalid working directory:" << workingPath;
            return -3;
    }

    cLog() << "Running" << program << arguments;
    process.start();
    if ( !process.waitForStarted() )
    {
        cLog() << "Process failed to start" << process.error();
        return -2;
    }

    if ( !stdInput.isEmpty() )
    {
        process.write( stdInput.toLocal8Bit() );
        process.closeWriteChannel();
    }

    if ( !process.waitForFinished( timeoutSec ? ( timeoutSec * 1000 ) : -1 ) )
    {
        cLog() << "Timed out. output so far:";
        cLog() << process.readAllStandardOutput();
        return -4;
    }

    output.append( QString::fromLocal8Bit( process.readAllStandardOutput() ).trimmed() );

    if ( process.exitStatus() == QProcess::CrashExit )
    {
        cLog() << "Process crashed";
        return -1;
    }

    cLog() << "Finished. Exit code:" << process.exitCode();
    return process.exitCode();
}


int
chrootOutput( const QString& command,
              QString& output,
              const QString& workingPath,
              const QString& stdInput,
              int timeoutSec )
{
    return chrootOutput( QStringList{ command },
                         output,
                         workingPath,
                         stdInput,
                         timeoutSec );
}


qint64
getPhysicalMemoryB()
{
        QProcess p;
        p.start( "dmidecode", { "-t", "17" } );
        p.waitForFinished();
        QStringList lines = QString::fromLocal8Bit( p.readAllStandardOutput() ).split( '\n' );
        lines = lines.filter( QRegularExpression( "^\\W*Size:\\W\\d*\\WMB" ) );
        if ( !lines.isEmpty() )
            return 0;

        qint64 availableRamMb = 0;
        foreach( const QString& line, lines )
        {
            bool ok = false;
            availableRamMb += line.simplified()
                                  .split( ' ' )
                                  .value( 1 )
                                  .toInt( &ok );
            if ( !ok )
                return 0;
        }
        qint64 availableRam = availableRamMb * 1024 * 1024;
        return availableRam;
}


qint64
getTotalMemoryB()
{
    // A line in meminfo looks like this, with {print $2} we grab the second column.
    // MemTotal:        8133432 kB

    QProcess p;
    p.start( "awk", { "/MemTotal/ {print $2}", "/proc/meminfo" } );
    p.waitForFinished();
    QString memoryLine = p.readAllStandardOutput().simplified();

    bool ok = false;
    qint64 availableRam = memoryLine.toLongLong( &ok ) * 1024;
    if ( !ok )
        return 0;

    return availableRam;
}


}
