/***************************************************************************
                         krenamemodel.cpp  -  description
                             -------------------
    begin                : Sun Apr 25 2007
    copyright            : (C) 2007 by Dominik Seichter
    email                : domseichter@web.de
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "krenamemodel.h"
#include "threadedlister.h"

#include <QMimeData>
#include <QPixmap>
#include <QApplication>

#include <KLocalizedString>
#include <krun.h>
#include <kio/previewjob.h>

KRenameModel::KRenameModel(KRenameFile::List *vector)
    : QAbstractListModel(),
      m_vector(vector),
      m_preview(false),
      m_text(false),
      m_maxDots(0),
      m_mimeType("text/uri-list"),
      m_eSortMode(eSortMode_Unsorted),
      m_customSortToken("creationdate;yyyyMMddHHmm"),
      m_eCustomSortMode(KRenameTokenSorter::eSimpleSortMode_Ascending)
{

}

KRenameModel::~KRenameModel()
{

}

int KRenameModel::rowCount(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return m_vector->size();
    }

    return 0;
}

QVariant KRenameModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (index.row() >= m_vector->size()) {
        return QVariant();
    }

    if (role == Qt::DisplayRole) {
        if (!m_preview) {
            // Only return path
            return m_vector->at(index.row()).toString();
        } else if (m_preview && m_text) {
            // Short filename as first line in bold
            // Path as second line
            const KRenameFile &file = m_vector->at(index.row());
            QString filename = file.srcFilename();
            if (!file.srcExtension().isEmpty()) {
                filename = filename + '.' + file.srcExtension();
            }

            const QString &prettyUrl = file.toString();
            return "<qt><b>" + filename + "</b><br/>" +
                   prettyUrl + "</qt>";
        }
    } else if (role == Qt::DecorationRole && m_preview) {
        return m_vector->at(index.row()).icon();
    } else if (role == Qt::UserRole) {
        return m_vector->at(index.row()).toString();
    }

    return QVariant();
}

Qt::ItemFlags KRenameModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::ItemIsDropEnabled;
    }

    return QAbstractItemModel::flags(index) | Qt::ItemIsEditable | Qt::ItemIsDropEnabled;
}

Qt::DropActions KRenameModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

QStringList KRenameModel::mimeTypes() const
{
    QStringList types;
    types << m_mimeType;
    return types;
}

bool KRenameModel::dropMimeData(const QMimeData *data,
                                Qt::DropAction action,
                                int, int,
                                const QModelIndex &)
{
    if (action == Qt::IgnoreAction) {
        return true;
    }

    if (!data->hasFormat(m_mimeType)) {
        return false;
    }

    QList<QUrl>                  dirs;
    KRenameFile::List           files;
    QList<QUrl>                 urls = data->urls();
    QList<QUrl>::const_iterator it   = urls.constBegin();

    QApplication::setOverrideCursor(Qt::BusyCursor);

    while (it != urls.constEnd()) {
        if ((*it).isValid()) {
            KRenameFile file(*it, m_eSplitMode, m_dot);

            if (file.isValid() && !file.isDirectory()) {
                files.append(file);
            } else if (file.isValid() && file.isDirectory())
                // Add directories recursively
            {
                dirs.append(*it);
            }
        }

        ++it;
    }

    this->addFiles(files);
    if (dirs.count()) {
        QList<QUrl>::const_iterator it = dirs.constBegin();

        while (it != dirs.constEnd()) {
            ThreadedLister *thl = new ThreadedLister(*it, nullptr, this);
            connect(thl, &ThreadedLister::listerDone,
                    this, &KRenameModel::slotListerDone);

            thl->setListDirnamesOnly(false);
            thl->setListHidden(false);
            thl->setListRecursively(true);
            thl->setListDirnames(false);

            thl->start();

            ++it;
        }
    } else {
        QApplication::restoreOverrideCursor();
        emit filesDropped();
    }

    return true;
}

void KRenameModel::slotListerDone(ThreadedLister *lister)
{
    // Delete the listener
    delete lister;

    // restore cursor
    QApplication::restoreOverrideCursor();

    emit filesDropped();
}

bool KRenameModel::setData(const QModelIndex &index,
                           const QVariant &, int role)
{
    if (index.isValid() && role == Qt::EditRole) {

        //stringList.replace(index.row(), value.toString());
        emit dataChanged(index, index);
        return true;
    }

    return false;
}

void KRenameModel::addFiles(const KRenameFile::List &files)
{
    if (files.count()) {
        int oldMaxDots = m_maxDots;
        m_vector->reserve(m_vector->count() + files.count());

        this->beginInsertRows(QModelIndex(), m_vector->size(), m_vector->size() + files.count() - 1);

        KRenameFile::List::const_iterator it = files.begin();
        while (it != files.end()) {
            m_vector->push_back(*it);

            int dots  = (*it).dots();
            if (dots > m_maxDots) {
                m_maxDots = dots;
            }

            ++it;
        }
        this->endInsertRows();

        if (m_maxDots > oldMaxDots) {
            emit maxDotsChanged(m_maxDots);
        }

        // Update sorting
        this->sortFiles(m_eSortMode, m_customSortToken, m_eCustomSortMode);

        // Generate previews if necessary
        if (m_preview) {
            // Construct a list of KFileItems
            // Only do this is necessary,
            // as this might create new KFileItems which is slow.
            KFileItemList fileItems;
            it = files.begin();
            while (it != files.end()) {
                fileItems << (*it).fileItem();

                ++it;
            }

            // TODO: Enable this job, it currently crashes for me

            // Start a job to create the real file previews
            KIO::PreviewJob *job = KIO::filePreview(fileItems, QSize(KRenameFile::iconSize(), KRenameFile::iconSize()));

            connect(job, &KIO::PreviewJob::gotPreview,
                    this, &KRenameModel::gotPreview);
            job->start();
        }
    }
}

void KRenameModel::gotPreview(const KFileItem &item, const QPixmap &preview)
{
    /*
    const KRenameFile* file =
        static_cast<const KRenameFile*>(item.extraData(KRenameFile::EXTRA_DATA_KEY));
    */

    KRenameFile *file = nullptr;
    // TODO: Find a more optimal "search algorithm" ....
    KRenameFile::List::iterator it = m_vector->begin();
    while (it != m_vector->end()) {
        if ((*it).srcUrl() == item.url()) {
            file = &(*it);
            break;
        }

        ++it;
    }

    //it = find( m_vector->begin(), m_vector->end(), item );
    if (file != nullptr) { // && file->fileItem() == item )
        file->setIcon(preview);
    }
}

void KRenameModel::removeFiles(const QList<int> &remove)
{
    int offset = 0;

    QList<int> copy(remove);
    std::sort(copy.begin(), copy.end());

    QList<int>::const_iterator it = copy.constBegin();
    this->beginRemoveRows(QModelIndex(), *it, copy.back());
    while (it != copy.constEnd()) {
        m_vector->erase(m_vector->begin() + *it - offset);

        ++offset;
        ++it;
    }

    this->endRemoveRows();
}

void KRenameModel::sortFiles(ESortMode mode, const QString &customSortToken, KRenameTokenSorter::ESimpleSortMode customSortMode)
{
    beginResetModel();
    const QString dateSortToken = "creationdate;yyyyMMddHHmm";

    m_eSortMode = mode;
    m_customSortToken = customSortToken;
    m_eCustomSortMode = customSortMode;

    if (mode == eSortMode_Ascending) {
        std::sort(m_vector->begin(), m_vector->end(), ascendingKRenameFileLessThan);
    } else if (mode == eSortMode_Descending) {
        std::sort(m_vector->begin(), m_vector->end(), descendingKRenameFileLessThan);
    } else if (mode == eSortMode_Numeric) {
        std::sort(m_vector->begin(), m_vector->end(), numericKRenameFileLessThan);
    } else if (mode == eSortMode_Random) {
        std::sort(m_vector->begin(), m_vector->end(), randomKRenameFileLessThan);
    } else if (mode == eSortMode_AscendingDate) {
        KRenameTokenSorter sorter(m_renamer, dateSortToken, *m_vector,
                                  KRenameTokenSorter::eSimpleSortMode_Ascending);
        std::sort(m_vector->begin(), m_vector->end(), sorter);
    } else if (mode == eSortMode_DescendingDate) {
        KRenameTokenSorter sorter(m_renamer, dateSortToken, *m_vector,
                                  KRenameTokenSorter::eSimpleSortMode_Descending);
        std::sort(m_vector->begin(), m_vector->end(), sorter);
    } else if (mode == eSortMode_Token) {
        KRenameTokenSorter sorter(m_renamer, customSortToken, *m_vector,
                                  customSortMode);
        std::sort(m_vector->begin(), m_vector->end(), sorter);
    }

    endResetModel();
}

void KRenameModel::run(const QModelIndex &index, QWidget *window) const
{
    KRenameFile file = m_vector->at(index.row());
    new KRun(file.srcUrl(), window);
}

const QModelIndex KRenameModel::createIndex(int row) const
{
    return QAbstractItemModel::createIndex(row, 0);
}

void KRenameModel::moveFilesUp(const QList<int> &files)
{
    int         index;
    KRenameFile tmp;

    QList<int> copy(files);
    std::sort(copy.begin(), copy.end());

    beginResetModel();
    QList<int>::const_iterator it = copy.constBegin();
    while (it != copy.constEnd()) {
        index                     = *it;
        if (index <= 0) { // cannot swap top item
            ++it;
            continue;
        }

        // swap items
        tmp                    = m_vector->at(index);
        (*m_vector)[index]     = KRenameFile(m_vector->at(index - 1));
        (*m_vector)[index - 1] = tmp;

        ++it;
    }

    endResetModel();
}

void KRenameModel::moveFilesDown(const QList<int> &files)
{
    int         index;
    KRenameFile tmp;

    QList<int> copy(files);
    // sort the list in reverse order
    std::sort(copy.begin(), copy.end(), std::greater<int>());

    beginResetModel();
    QList<int>::const_iterator it = copy.constBegin();
    while (it != copy.constEnd()) {
        index                     = *it;
        if (index + 1 >= m_vector->size()) { // cannot swap bottom item
            ++it;
            continue;
        }

        // swap items
        tmp                    = m_vector->at(index);
        (*m_vector)[index]     = KRenameFile(m_vector->at(index + 1));
        (*m_vector)[index + 1] = tmp;

        ++it;
    }

    endResetModel();
}

//////////////////////////////////////////////////////////////
// Preview model starts below
//////////////////////////////////////////////////////////////
KRenamePreviewModel::KRenamePreviewModel(KRenameFile::List *vector)
    : m_vector(vector)
{

}

KRenamePreviewModel::~KRenamePreviewModel()
{

}

int KRenamePreviewModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return m_vector->size();
    }

    return 0;
}

int KRenamePreviewModel::columnCount(const QModelIndex &) const
{
    return 2;
}

QVariant KRenamePreviewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || section > 1 || role != Qt::DisplayRole) {
        return QVariant();
    }

    return (section == 0) ? i18n("Origin") : i18n("Renamed");
}

QVariant KRenamePreviewModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (index.row() >= m_vector->size()) {
        return QVariant();
    }

    if (index.column() >= 2) {
        return QVariant();
    }

    if (role == Qt::DisplayRole) {
        const KRenameFile &file = m_vector->at(index.row());
        QString filename;
        QString extension;
        QString manual;

        if (index.column()) {
            manual    = file.manualChanges();
            if (manual.isNull()) {
                filename  = file.dstFilename();
                extension = file.dstExtension();
            } else {
                filename = manual;
            }
        } else {
            filename  = file.srcFilename();
            extension = file.srcExtension();
        }

        if (!extension.isEmpty()) {
            filename += '.';
            filename += extension;
        }

        if (file.isDirectory()) {
            filename = (index.column() ? file.dstDirectory() : file.srcDirectory()) + '/' + filename;
        }

        return filename;
    } else if (role == Qt::ForegroundRole) {
        const KRenameFile &file = m_vector->at(index.row());
        if (!file.manualChanges().isNull()) {
            return QVariant(QColor(Qt::blue));
        }
    }
    /*
      Icons are to large, so this is disabled
    else if( role == Qt::DecorationRole && index.column() == 0 )
    {
        return m_vector->at(index.row()).icon();
    }
    */

    return QVariant();

}

QModelIndex KRenamePreviewModel::parent(const QModelIndex &) const
{
    return QModelIndex();
}

QModelIndex KRenamePreviewModel::sibling(int, int, const QModelIndex &) const
{
    return QModelIndex();
}

void KRenamePreviewModel::refresh()
{
    beginResetModel();
    endResetModel();
}
