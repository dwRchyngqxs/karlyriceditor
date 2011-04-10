/**************************************************************************
 *  Karlyriceditor - a lyrics editor and CD+G / video export for Karaoke  *
 *  songs.                                                                *
 *  Copyright (C) 2009-2011 George Yunaev, support@karlyriceditor.com     *
 *                                                                        *
 *  This program is free software: you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation, either version 3 of the License, or     *
 *  (at your option) any later version.                                   *
 *																	      *
 *  This program is distributed in the hope that it will be useful,       *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *  GNU General Public License for more details.                          *
 *                                                                        *
 *  You should have received a copy of the GNU General Public License     *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/

#include <QDesktopServices>
#include <QWhatsThis>
#include <QMessageBox>
#include <QFileDialog>

#include "audioplayer.h"
#include "project.h"
#include "wizard_newproject.h"

namespace WizardNewProject
{

//
// "Select lyrics format" page
//
PageLyricType::PageLyricType( Project * project, QWidget *parent )
	: QWizardPage( parent ), Ui::WizNewProject_LyricType()
{
	setupUi( this );

	m_project = project;
	connect( lblHelp, SIGNAL( linkActivated ( const QString &) ), this, SLOT( showhelp() ) );
}

bool PageLyricType::validatePage()
{
	// At least one should be selected
	if ( rbLRC1->isChecked() )
		m_project->setType( Project::LyricType_LRC1 );
	else if ( rbLRC2->isChecked() )
		m_project->setType( Project::LyricType_LRC2 );
	else if ( rbLRC3->isChecked() )
		m_project->setType( Project::LyricType_UStar );
	else
		return false;

	return true;
}

void PageLyricType::showhelp()
{
	QString help = tr( "LRC1 is the first version of LRC format, containing a single "
					   "line with timing mark at the beginning. This format is supported by most players\n\n"
					   "LRC2 is the second version, which can contain timing marks inside "
					   "the line. Supported by less players.\n\n"
					   "UStar/UltraStar format is lyrics format used in SingStar, Sinatra, Performous and "
					   "similar games.\n\nXBMC supports all those formats" );

	QWhatsThis::showText ( mapToGlobal( lblHelp->pos() ), help );
}


//
// "Choose music file" page
//
PageMusicFile::PageMusicFile( Project * project, QWidget *parent )
	: QWizardPage( parent ), Ui::WizNewProject_MusicFile()
{
	setupUi( this );

	m_project = project;
	lblPicture->setPixmap( QPixmap( ":/images/nocover.png" ) );

	connect( btnBrowse, SIGNAL( clicked() ), this, SLOT( browse() ) );
}

PageMusicFile::~PageMusicFile()
{
}

void PageMusicFile::browse()
{
	QString filename = QFileDialog::getOpenFileName( 0,
			tr("Choose a music file to load"), "." );

	if ( filename.isEmpty() )
		return;

	// Try to open it
	if ( pAudioPlayer->open( filename ) )
	{
		leSongFile->setText( filename );
		pAudioPlayer->close();
	}
}

bool PageMusicFile::validatePage()
{
	if ( leSongFile->text().isEmpty() )
	{
		QMessageBox::critical( 0,
							   tr("Music file not selected"),
							   tr("You must select a music file to continue.") );
		return false;
	}

	if ( !pAudioPlayer->open( leSongFile->text() ) )
		return false;

	if ( leTitle->text().isEmpty() )
	{
		QMessageBox::critical( 0,
							   tr("Title field is empty"),
							   tr("You must type song title to continue.") );
		return false;
	}

	if ( leArtist->text().isEmpty() )
	{
		QMessageBox::critical( 0,
							   tr("Artist field is empty"),
							   tr("You must type song artist to continue.") );
		return false;
	}

	m_project->setMusicFile( leSongFile->text() );
	m_project->setTag( Project::Tag_Title, leTitle->text() );
	m_project->setTag( Project::Tag_Artist, leArtist->text() );

	if ( !leAlbum->text().isEmpty() )
		m_project->setTag( Project::Tag_Album, leAlbum->text() );

	return true;
}


//
// "Choose lyrics" page
//
PageLyrics::PageLyrics( Project * project, QWidget *parent )
	: QWizardPage( parent ), Ui::WizNewProject_Lyrics()
{
	setupUi( this );

	connect( btnBrowse, SIGNAL( clicked() ), this, SLOT( browse() ) );

	m_project = project;
}

void PageLyrics::initializePage()
{
	// Reading embedded lyrics is not supported by Phonon yet
	rbnEmbeddedLyrics->setEnabled( false );
}

void PageLyrics::browse()
{
	QString fileName = QFileDialog::getOpenFileName( this,
			tr("Open a lyric file"),
			".",
			tr("LRC files (*.lrc);;UltraStar files (*.txt)") );

	if ( fileName.isEmpty() )
		return;

	leFileName->setText( fileName );
}

bool PageLyrics::validatePage()
{
	// If lyrics file is selected, it must exist and be valid
	if ( rbnLoadFromFile->isChecked() )
	{
		if ( !QFile::exists( leFileName->text() ) )
		{
			QMessageBox::critical( 0,
				tr("Lyrics file not found"),
				tr("Selected lyrics file is not found.") );

			return false;
		}

		if ( !m_project->importLyrics( leFileName->text(), leFileName->text().endsWith( "txt" ) ? Project::LyricType_UStar : Project::LyricType_LRC2 ) )
			return false;
	}

	return true;
}


Wizard::Wizard( Project * project, QWidget *parent )
	: QWizard( parent )
{
	addPage( new PageIntro( project, this ) );
	addPage( new PageLyricType( project, this ) );
	addPage( new PageMusicFile( project, this ) );
	addPage( new PageLyrics( project, this ) );
	addPage( new PageFinish( project, this ) );

#ifndef Q_WS_MAC
	setWizardStyle(ModernStyle);
#endif

	setOption( HaveHelpButton, false );

	setWindowTitle( tr("New karaoke lyrics project") );
	setPixmap( QWizard::WatermarkPixmap, QPixmap(":/images/casio.jpg") );
}


} // namespace
