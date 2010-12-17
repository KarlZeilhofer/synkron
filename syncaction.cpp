#include "syncaction.h"

#include "folders.h"
#include "filecompare.h"
#include "folderactiongroup.h"
#include "syncfile.h"
#include "syncexceptionbundle.h"
#include "syncoutmessage.h"
#include "exceptiongroup.h"
#include "mtfile.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

#include <QElapsedTimer>

SyncAction::SyncAction(Folders * folders, SyncExceptionBundle * bundle) : QThread()
{
    base_folders = folders;
    exception_bundle = bundle;
    skipped_count = 0;
    file_compare = new FileCompare();
}

SyncAction::~SyncAction()
{
    delete exception_bundle;
}

void SyncAction::start(Priority priority)
{
    QThread::start(priority);
    moveToThread(this);
}

void SyncAction::run()
{
    QObject::connect(this, SIGNAL(finished()), this, SLOT(deleteLater()), Qt::DirectConnection);

    SyncFile * sf = new SyncFile("/");
    FolderActionGroup * fag = base_folders->folderActionGroup();
    skipped_count = 0;

    exception_bundle->updateRootFolder(fag);

    QElapsedTimer timer;
    timer.start();
    createSyncFileFromFolders(sf, fag);
    fag = base_folders->folderActionGroup();
    sync(sf, fag);

    //emit filesCounted(sf->count());
    //emit messageBox(QString("COUNT %1").arg(sf->count()));

    //emit messageBox(QString("TIME %1").arg(timer.elapsed()));
    emit finished();

    delete sf;
}

/**
  * Fills up the parent sync file with all the content of folders in the folder action group.
  * Recursively calls itself when finds a folder.
  */
void SyncAction::createSyncFileFromFolders(SyncFile * parent, FolderActionGroup *& fag)
{
    QSet<QString> entries;

    for (int i = 0; i < fag->count(); ++i) {
        //Creating a set of all file names ---------------------------------

        QDir sync_dir(fag->at(i));
        if (!sync_dir.exists()) continue;

        entries.unite(sync_dir.entryList(exception_bundle->filters(), QDir::NoDotAndDotDot | QDir::Files | QDir::AllDirs).toSet());
    }

    QFileInfo fi;
    FolderActionGroup * child_dirs_fag = NULL;
    SyncFile * sf = NULL;

    foreach (QString entry, entries) {

        if (exception_bundle->cdAndCheck(entry)) { // +++ Blacklisted +++
            skipped_count++;
        }
        else { // +++ Not in blacklist +++
            sf = parent->addChild(entry);

            for (int n = 0; n < fag->count(); ++n) {
                // Obtaining absolute paths for the file names ---------------------
                fi.setFile(QDir(fag->at(n)).absoluteFilePath(entry));

                if (fi.exists()) {
                    sf->addFolder(fag->idAt(n));

                    if (fi.isDir()) {
                        sf->setDir(true);
                        if (!child_dirs_fag) child_dirs_fag = new FolderActionGroup;
                        child_dirs_fag->insert(fag->idAt(n), fi.absoluteFilePath());
                    }
                }
            }

            //sf->setBlacklisted(exception_bundle->isInBlacklist());
            if (child_dirs_fag) createSyncFileFromFolders(sf, child_dirs_fag);
        }

        exception_bundle->cdUp();
    }

    delete fag;
    fag = NULL;
}

void SyncAction::sync(SyncFile * parent, FolderActionGroup *& fag)
{
    if (!parent)
        return;

    SyncFile * sf = NULL;
    QFileInfo fi;
    QDir dir;

    //int * latest_index_arr = (int *) malloc(fag->count() * sizeof(int));
    int latest_index_arr[fag->count()];
    int latest_index = 0, c;
    QFileInfo * newest_fi = NULL;
    FolderActionGroup * sub_fag = NULL;

    for (int i = 0; i < parent->childCount(); ++i) {
        sf = parent->childAt(i);
        latest_index = 0;
        if (newest_fi) {
            delete newest_fi;
            newest_fi = NULL;
        }

        if (sf->isDir()) {
            sub_fag = new FolderActionGroup;
            for (int n = 0; n < fag->count(); ++n) {
                dir.setPath(fag->at(n));
                if (!sf->existsInFolder(fag->idAt(n))) {
                    if (dir.exists()) {
                        if (!dir.mkdir(sf->getName())) {
                            emit this->syncOutMessage(new SyncOutMessage(SyncOutMessage::FolderCreated, dir.absoluteFilePath(sf->getName()), QString(), true));
                            continue;
                        } else {
                            emit this->syncOutMessage(new SyncOutMessage(SyncOutMessage::FolderCreated, dir.absoluteFilePath(sf->getName())));
                        }
                    }
                }
                sub_fag->insert(fag->idAt(n), dir.absoluteFilePath(sf->getName()));
            }
            sync(sf, sub_fag);
            delete sub_fag;
        }
        else { // is not dir
            for (int n = 0; n < fag->count(); ++n) {
                if (sf->existsInFolder(fag->idAt(n))) {
                    dir.setPath(fag->at(n));
                    fi.setFile(dir.absoluteFilePath(sf->getName()));
                    if (!newest_fi) {
                        newest_fi = new QFileInfo(fi);
                        latest_index_arr[n] = latest_index;
                    }
                    else {
                        if ((c = file_compare->compareTwoFiles(&fi, newest_fi)) == 0) { // equal to the newest
                            latest_index_arr[n] = latest_index;
                        }
                        else if (c > 0) { // fi is newer
                            newest_fi->setFile(fi.absoluteFilePath());
                            latest_index_arr[n] = ++latest_index;
                        }
                        else { // fi is older
                            latest_index_arr[n] = latest_index - 1;
                        }
                    }
                } else {
                    latest_index_arr[n] = -2;
                }
            }

            if (latest_index < 0) // probably never happens...
                continue;

            for (int n = 0; n < fag->count(); ++n) {
                dir.setPath(fag->at(n));
                if (latest_index_arr[n] == -2) { // does not exist
                    copyFile(newest_fi, dir.absoluteFilePath(sf->getName()));
                }
                else if (latest_index_arr[n] < latest_index) { // is obsolete
                    updateFile(newest_fi, dir.absoluteFilePath(sf->getName()));
                }
            }
        }
    }
}

void SyncAction::copyFile(QFileInfo * source, const QString & dest_string)
{
    MTFile source_file(source->absoluteFilePath());
    if (source_file.copy(dest_string)) {
        emit this->syncOutMessage(new SyncOutMessage(SyncOutMessage::FileCopied, source->absoluteFilePath(), dest_string));
    } else {
        emit this->syncOutMessage(new SyncOutMessage(SyncOutMessage::FileCopied, source->absoluteFilePath(), dest_string, true, source_file.errorString()));
    }
}

void SyncAction::updateFile(QFileInfo * source, const QString & dest_string)
{
    MTFile file(dest_string);

    if (!backupFile(&file)) {
        return;
    }

    if (!file.remove()) {
        emit this->syncOutMessage(new SyncOutMessage(SyncOutMessage::FileUpdated, source->absoluteFilePath(), dest_string, true, file.errorString()));
        return;
    }

    file.setFileName(source->absoluteFilePath());
    if (!file.copy(dest_string)) {
        emit this->syncOutMessage(new SyncOutMessage(SyncOutMessage::FileUpdated, source->absoluteFilePath(), dest_string, true, file.errorString()));
    } else {
        emit this->syncOutMessage(new SyncOutMessage(SyncOutMessage::FileUpdated, source->absoluteFilePath(), dest_string));
    }
}

bool SyncAction::backupFile(MTFile * file)
{
    /*QDir(sp->mp_parent->temp_path).mkdir(update_time);
        if (!file->copy(QString("%1/%2/%3.%4").arg(sp->mp_parent->temp_path).arg(update_time).arg(file_info2->fileName()).arg(synced_files))) {
            sp->errorCopyingFile(file_info2->absoluteFilePath(), file->errorString(), true);
            delete file;
            continue;
        }
        sp->saveBackedUpFile(*file_info2);*/

    return true;
}
