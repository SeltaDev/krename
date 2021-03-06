/***************************************************************************
                    filedialogextwidget.cpp -  description
                             -------------------
    begin                : Tue Apr 17 2007
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

#include "filedialogextwidget.h"

#include <KLocalizedString>

// Qt includes
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QUrl>

#include <QDialogButtonBox>
#include <QPushButton>

FileDialogExtWidget::FileDialogExtWidget(QWidget *parent)
    : QDialog(parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout;
    setLayout(mainLayout);

    m_fileWidget = new KFileWidget(QUrl());
    m_fileWidget->setOperationMode(KFileWidget::Opening);
    m_fileWidget->setMode(KFile::Files | KFile::Directory | KFile::ExistingOnly);

    connect(m_fileWidget, &KFileWidget::accepted, [&]() {
        m_fileWidget->accept();

        // We have to do this manually for some reason
        accept();
    });
    layout()->addWidget(m_fileWidget);

    QWidget *extraWidget = new QWidget;
    QVBoxLayout *extraLayout = new QVBoxLayout;
    extraWidget->setLayout(extraLayout);
    checkDir       = new QCheckBox(i18n("Add folder names &with filenames"), this);
    checkRecursive = new QCheckBox(i18n("Add subfolders &recursively"), this);
    checkHidden    = new QCheckBox(i18n("Add &hidden folders"), this);
    checkOnlyDir   = new QCheckBox(i18n("Add folder names only"), this);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addSpacing(20);
    hbox->addWidget(checkHidden);
    hbox->setStretchFactor(checkHidden, 4);

    extraLayout->addWidget(checkDir);
    extraLayout->addWidget(checkRecursive);
    extraLayout->addLayout(hbox);
    extraLayout->addWidget(checkOnlyDir);
    m_fileWidget->setCustomWidget(extraWidget);

    connect(checkRecursive, &QCheckBox::clicked,
            this, &FileDialogExtWidget::enableControls);

    checkRecursive->setToolTip(i18n("Walk recursively through the folder tree and also add the content "
                                    "of all subfolders to the list of files to rename."));
    checkHidden->setToolTip(i18n("If not checked, KRename will ignore folders starting "
                                 "with a dot during recursive adding."));
    checkOnlyDir->setToolTip(i18n("Add only the folder names and not the names "
                                  "of the files in the folder to KRename."));
    checkDir->setToolTip(i18n("This option causes KRename to also add the name of the base "
                              "folder of the selected files to its list."));

    enableControls();

    QDialogButtonBox *buttonBox = new QDialogButtonBox;
    buttonBox->addButton(m_fileWidget->okButton(), QDialogButtonBox::AcceptRole);
    buttonBox->addButton(m_fileWidget->cancelButton(), QDialogButtonBox::RejectRole);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted,
            m_fileWidget, &KFileWidget::slotOk);
    layout()->addWidget(buttonBox);
}

void FileDialogExtWidget::enableControls()
{
    checkHidden->setEnabled(checkRecursive->isChecked());
}
