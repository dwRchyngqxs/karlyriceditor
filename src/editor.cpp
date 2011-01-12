/**************************************************************************
 *  Karlyriceditor - a lyrics editor for Karaoke songs                    *
 *  Copyright (C) 2009 George Yunaev, support@karlyriceditor.com          *
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

#include <QTextBlock>
#include <QDataStream>
#include <QMessageBox>
#include <QMouseEvent>
#include <QTextDocumentFragment>
#include <QMimeData>
#include <QToolTip>
#include <QScrollBar>
#include <QStack>

#include "mainwindow.h"
#include "project.h"
#include "editor.h"
#include "settings.h"
#include "editorhighlighting.h"
#include "cdggenerator.h"

const char * Editor::PLACEHOLDER = "[--:--]";

Editor::Editor( QWidget * parent )
	: QTextEdit( parent )
{
	m_project = 0;
	m_timeId = 0;

	connect( this, SIGNAL( undoAvailable(bool)), pMainWindow, SLOT( editor_undoAvail(bool)));
	connect( this, SIGNAL( redoAvailable(bool)), pMainWindow, SLOT( editor_redoAvail(bool)));

	connect( this, SIGNAL(textChanged()), this, SLOT(textModified()) );

	setAcceptRichText( false );

	QFont font( pSettings->m_editorFontFamily, pSettings->m_editorFontSize );
	setFont( font );

	EditorHighlighting * hl = new EditorHighlighting( this );
}

void Editor::setProject( Project* proj )
{
	m_project = proj;
}

static inline bool isCRLF( QChar ch )
{
	return (ch == QChar::LineSeparator) || (ch == QChar::ParagraphSeparator ) || (ch == '\n' );
}

static inline QString markToTime( qint64 mark )
{
	int min = mark / 60000;
	int sec = (mark - min * 60000) / 1000;
	int msec = mark - (min * 60000 + sec * 1000 );

	return QString().sprintf( "%02d:%02d.%02d", min, sec, msec / 10 );
}

static inline qint64 infoToMark( QString data )
{
	QRegExp rxtime( "^(\\d+):(\\d+)\\.(\\d+)$");

	if ( data.indexOf( rxtime ) == -1 )
		return -1;

	return rxtime.cap( 1 ).toInt() * 60000 + rxtime.cap( 2 ).toInt() * 1000 + rxtime.cap( 3 ).toInt() * 10;
}

void Editor::textModified()
{
	if ( !m_project )
		return;

	m_project->setModified();
}

QString	Editor::exportToString()
{
	return toPlainText();
}

bool Editor::importFromString( const QString& lyricstr )
{
	setPlainText( lyricstr );
	return true;
}

bool Editor::importFromOldString( const QString& lyricstr )
{
	QString strlyrics;

	clear();
	setEnabled( true );

	// A simple state machine
	bool timing = false;
	QString saved;

	for ( int i = 0; ; ++i )
	{
		// Store the presaved text
		if ( i == lyricstr.length() || lyricstr[i] == '<' )
		{
			// There is no qt::unescape
			saved.replace( "&lt;", "<" );
			saved.replace( "&gt;", ">" );
			saved.replace( "&amp;", "&" );

			strlyrics += saved;

			if ( i == lyricstr.length() )
				break;

			saved.clear();
			timing = true;
		}
		else if ( lyricstr[i] == '>' )
		{
			QString time;

			if ( saved.contains( '|' ) )
			{
				QStringList values = saved.split( '|' );
				time = "[" + markToTime( values[0].toLongLong() ) + "]";
			}
			else
				time = "[" + markToTime( saved.toLongLong() ) + "]";

			strlyrics += time;
			saved.clear();
			timing = false;
		}
		else
			saved.push_back( lyricstr[i] );
	}

	setPlainText( strlyrics );
	return true;
}

Lyrics Editor::exportLyrics()
{
	Lyrics lyrics;

	if ( !validate() )
		return lyrics;

	QString text = toPlainText();
	QStringList lines = text.split( '\n' );

	lyrics.beginLyrics();

	foreach( QString line, lines )
	{
		if ( line.trimmed().isEmpty() )
		{
			// end of paragraph
			lyrics.curLyricAddEndOfLine();
			continue;
		}

		QStringList parts = line.split( '[' );

		// First part must be empty
		if ( !parts[0].isEmpty() )
			return Lyrics();

		parts.removeAt( 0 );

		foreach ( QString part, parts )
		{
			int timeoff = part.indexOf( ']' );

			QString timing = part.left( timeoff );
			QString text = part.mid( timeoff + 1 );

			lyrics.curLyricSetTime( infoToMark( timing ) );
			lyrics.curLyricAppendText( text );
			lyrics.curLyricAdd();
		}

		lyrics.curLyricAddEndOfLine();
	}

	lyrics.endLyrics();
	return lyrics;
}

void Editor::importLyrics( const Lyrics& lyrics )
{
	// clear the editor
	clear();
	setEnabled( true );

	QString strlyrics;

	// Fill the editor
	for ( int bl = 0; bl < lyrics.totalBlocks(); bl++ )
	{
		const Lyrics::Block& block = lyrics.block( bl );

		for ( int ln = 0; ln < block.size(); ln++ )
		{
			const Lyrics::Line& line = block[ln];

			for ( int pos = 0; pos < line.size(); pos++ )
			{
				Lyrics::Syllable lentry = line[pos];

				strlyrics += "[" + markToTime( lentry.timing ) + "]" + lentry.timing;
			}

			strlyrics += "\n";
		}

		strlyrics += "\n";
	}

	setPlainText( strlyrics );
}

void Editor::cursorToLine( int line, int column )
{
	QTextCursor cur = textCursor();
	cur.movePosition( QTextCursor::Start, QTextCursor::MoveAnchor );
	cur.movePosition( QTextCursor::Down, QTextCursor::MoveAnchor, line - 1 );

	if ( column )
	{
		cur.movePosition( QTextCursor::Left, QTextCursor::MoveAnchor );
		cur.movePosition( QTextCursor::Right, QTextCursor::MoveAnchor, column );
	}

	setTextCursor( cur );
	ensureCursorVisible();
}

// validators
bool Editor::validate()
{
	QList<ValidatorError> errors;
	validate( errors );

	if ( !errors.isEmpty() )
	{
		QMessageBox::critical( 0,
							   tr("Validation error found"),
								  tr("Error at line %1: ").arg( errors.front().line )
								  + errors.front().error );

		cursorToLine( errors.front().line, errors.front().column );
		return false;
	}

	return true;
}

void Editor::validate( QList<ValidatorError>& errors )
{
	int linesinblock = 0;
	qint64 last_time = 0;
	QString paragraphtext;

	CDGGenerator gen ( m_project );
	gen.init();

	// Get the lyrics
	QString text = toPlainText();
	QStringList lines = text.split( '\n' );

	for ( int linenumber = 1; linenumber <= lines.size(); linenumber++ )
	{
		const QString& line = lines[ linenumber - 1];

		// Empty line is a paragraph separator. Handle it.
		if ( line.trimmed().isEmpty() )
		{
			// Is it enabled?
			if ( !pSettings->m_editorSupportBlocks )
			{
				errors.push_back(
						ValidatorError(
								linenumber,
								0,
								tr("Empty line found.\n"
								   "An empty line represents a block boundary, but blocks "
								   "are currently disabled in settings") ) );

				// recoverable
				goto cont_paragraph;
			}

			// Repeat?
			if ( paragraphtext.isEmpty() )
			{
				// A new paragraph already started; duplicate empty line, not allowed
				errors.push_back(
						ValidatorError(
								linenumber,
								0,
								tr("Double empty line found.\n"
									"A single empty line represents a block boundary; "
								"double lines are not supported.") ) );

				// recoverable
				goto cont_paragraph;
			}

			// Paragraph-specific checks
			if ( m_project->type() == Project::LyricType_CDG )
			{
				QList<ValidatorError> cdgerrors;

				gen.validateParagraph( paragraphtext, cdgerrors );

				foreach ( ValidatorError err, cdgerrors )
				{
					// Adjust the line number
					err.line += (linenumber - linesinblock);

					errors.push_back( err );
				}
			}

cont_paragraph:
			linesinblock = 0;
			paragraphtext = "";
			continue;
		}

		// If we're here, this is not an empty line.
		linesinblock++;

		// Check if we're out of block line limit
		if ( pSettings->m_editorSupportBlocks && linesinblock > pSettings->m_editorMaxBlock )
		{
			errors.push_back(
					ValidatorError(
							linenumber,
							0,
							tr("Block size exceeded. The block contains more than %1 lines.\n"
								"Most karaoke players cannot show too large blocks because of "
								"limited screen space.\n\nPlease split the block by adding a "
								"block separator (an empty line).\n") .arg(pSettings->m_editorMaxBlock) ) );
		}

		// Should not have lyrics before the first [
		if ( line[0] != '[' )
		{
			errors.push_back(
					ValidatorError(
							linenumber,
							0,
							tr("Missing opening time tag. Every line must start with a [mm:ss.ms] time tag") ) );
		}

		// LRCv2, UStar and CD+G must also end with ]
		if ( m_project->type() != Project::LyricType_LRC1 && !line.trimmed().endsWith( ']' ) )
		{
			errors.push_back(
					ValidatorError(
							linenumber,
							0,
							tr("Missing closing time tag. For this lyrics type every line must end with a [mm:ss.ms] time tag") ) );
		}

		// Go through the line, and verify all time tags
		int time_tag_start = 0;
		bool in_time_tag = false;

		for ( int col = 0; col < line.size(); col++ )
		{
			if ( in_time_tag )
			{
				// Time tag ends?
				if ( line[col] == ']' )
				{
					// Verify that the tag is valid
					QString time = line.mid( time_tag_start, col - time_tag_start );

					if ( time == PLACEHOLDER )
					{
						errors.push_back(
								ValidatorError(
										linenumber,
										time_tag_start,
										tr("Placeholders should not be present in the production file.") ) );
					}
					else
					{
						QRegExp rxtime( "^(\\d+):(\\d+)\\.(\\d+)$" );

						if ( time.indexOf( rxtime ) != -1 )
						{
							if ( rxtime.cap( 2 ).toInt() >= 60 )
							{
								errors.push_back(
										ValidatorError(
												linenumber,
												time_tag_start,
												tr("Invalid time, number of seconds cannot exceed 59.") ) );
							}

							qint64 timing = infoToMark( time );

							if ( timing < last_time )
							{
								errors.push_back(
										ValidatorError(
												linenumber,
												time_tag_start,
												tr("Time goes backward, previous time value is greater than current value.") ) );
							}

							last_time = timing;
						}
						else
						{
							errors.push_back(
									ValidatorError(
											linenumber,
											time_tag_start,
											tr("Invalid time tag. Time tag must be in format [mm:ss.ms] where mm is minutes, ss is seconds and ms is milliseconds * 10") ) );
						}
					}

					in_time_tag = false;
					continue;
				}

				// Only accept those characters
				if ( !line[col].isDigit() && line[col] != ':' && line[col] != '.' )
				{
					errors.push_back(
							ValidatorError(
									linenumber,
									col,
									tr("Invalid character in the time tag. Time tag must be in format [mm:ss.ms] where mm is minutes, ss is seconds and ms is milliseconds * 10") ) );

					in_time_tag = false;
					break; // end with this line
				}
			}
			else if ( line[col] == '[' )
			{
				in_time_tag = true;
				time_tag_start = col + 1;
				continue;
			}
			else if ( line[col] == ']' )
			{
				errors.push_back(
						ValidatorError(
								linenumber,
								col,
								tr("Invalid closing bracket usage outside the time block") ) );
			}
			else
				paragraphtext += line[col];
		}

		paragraphtext += "\n";

		// Verify opened time block
		if ( in_time_tag )
		{
			errors.push_back(
					ValidatorError(
							linenumber,
							line.size() - 1,
							tr("Time tag is not closed properly") ) );
		}
	}
}

void Editor::ensureCursorMiddle()
{
	// Adjust for non-common cases and horizontally
	ensureCursorVisible();

	// Now adjust vertically
	QScrollBar * vbar = verticalScrollBar();
	QRect crect = cursorRect( textCursor() );
	const int halfHeight = viewport()->height() / 2;
	const int curBottom = crect.y() + crect.height() + vbar->value();

	if ( curBottom > vbar->value() + halfHeight )
		vbar->setValue( qMax( 0, curBottom - halfHeight ) );
}


bool Editor::canInsertFromMimeData ( const QMimeData * source ) const
{
	return source->hasText() && !source->text().isEmpty();
}

QMimeData * Editor::createMimeDataFromSelection () const
{
	const QTextDocumentFragment fragment( textCursor() );
	QString text = fragment.toPlainText();

	QMimeData * m = new QMimeData();
	m->setText( text );

	return m;
}

void Editor::insertFromMimeData ( const QMimeData * source )
{
	QString text = source->text();

	text.replace( QChar::LineSeparator, "\n" );
	text.replace( QChar::ParagraphSeparator, "\n" );
	text.remove( "\r" );

	if ( !text.isNull() )
	{
		QTextDocumentFragment fragment = QTextDocumentFragment::fromPlainText( text );
		textCursor().insertFragment( fragment );
		ensureCursorVisible();
	}
}

void Editor::removeAllTimeTags()
{
	// No need to do it in a more complex way than a simple regexp
	QString text = toPlainText();

	// Placeholders
	text.remove( PLACEHOLDER );
	text.remove( QRegExp( "\\[\\d+:\\d+\\.\\d+\\]" ) );

	setPlainText( text );
}

void Editor::removeExtraWhitespace()
{
	// No need to do it in a more complex way than a simple regexp
	QStringList lyrics = toPlainText().split( '\n' );

	foreach ( QString lyric, lyrics )
	{
		lyric = lyric.trimmed();
	}

	setPlainText( lyrics.join( "\n") );
}

static bool isTimingMark( const QString& text, int * length = 0 )
{
	if ( text.startsWith( Editor::PLACEHOLDER ) )
	{
		if ( length )
			*length = strlen( Editor::PLACEHOLDER );

		return true;
	}

	QRegExp rx( "^\\[\\d+:\\d+\\.\\d+\\]" );

	if ( text.indexOf( rx ) != -1 )
	{
		if ( length )
			*length = rx.matchedLength();

		return true;
	}

	return false;
}


void Editor::insertTimeTag( qint64 timing )
{
	// If we're replacing existing time tag, remove it first
	bool was_time_mark_deleted = false;

	QTextCursor cur = textCursor();
	cur.beginEditBlock();

	QString text = cur.block().text().mid( cur.position() - cur.block().position() );
	int length;

	if ( timing > 0 && isTimingMark( text, &length ) )// only when playing
	{
		if ( text.startsWith( Editor::PLACEHOLDER ) )
			was_time_mark_deleted = true;

		while ( length-- )
			cur.deleteChar();

		text = cur.block().text().mid( cur.position() - cur.block().position() );
	}

	// Add the time
	if ( timing == 0 )
		cur.insertText( PLACEHOLDER );
	else
		cur.insertText( "[" + markToTime( timing ) + "]" );

	cur.endEditBlock();

	// Move the cursor according to policy
	if ( pSettings->m_editorDoubleTimeMark && was_time_mark_deleted )
		return; // the cursor has been moved already

	int curPos = cur.position();
	bool separator_found = false, tagged_word_ended = false;
	int word_start_offset = -1;

	while ( 1 )
	{
		// Find the block
		QTextBlock block = document()->findBlock( curPos );

		// Cursor position in the block
		int blockPos = curPos - block.position();
		QChar ch;

		// If we're out of range, this is the end of block.
		if ( blockPos >= block.text().size() )
		{
			// Text end?
			if ( !block.next().isValid() )
				break;

			// Tell the rest of the code this is end of line
			ch = QChar::LineSeparator;
		}
		else
			ch = block.text().at( blockPos );

		//qDebug("char: %s (%d), pos %d, blockpos %d", qPrintable( QString(ch)), ch.unicode(), curPos, blockPos );

		// Time mark?
		if ( isTimingMark( block.text().mid( blockPos ) ) )
			break;

		// We check for separator_found here because if the first character is timing mark, we want
		// it to be skipped too by a conditional check above
		if ( separator_found )
		{
			if ( pSettings->m_editorSkipEmptyLines && isCRLF( ch ) )
			{
				curPos++;
				continue;
			}

			break;
		}

		// New line is always a separator
		if ( isCRLF( ch ) )
		{
			// If this is not the first character, stop on previous one
			if ( cur.position() != curPos )
			{
				if ( pSettings->m_editorStopAtLineEnd )
					break;
			}

			separator_found = true;
		}

		// Timing mark is always before a word, so if we find a space, this means the word is ended,
		// and a new word is about to start. So if we have multiple tags per line enabled, check it.
		if ( pSettings->m_editorStopNextWord )
		{
			if ( ch.isSpace() )
			{
				// If word_start_offset is not -1, this means this is the second word which is ended
				if ( word_start_offset != -1 )
				{
					// Check the word length
					if ( curPos - word_start_offset > pSettings->m_editorWordChars )
					{
						// Word size is more than needed, and there was no time mark.
						// Roll the cursor back abd break
						curPos = word_start_offset;
						break;
					}
					else
					{
						// The word is too small. Reset the word_start_offset and continue
						word_start_offset = -1;
					}
				}
				else
					tagged_word_ended = true;
			}
			else
			{
				if ( tagged_word_ended && word_start_offset == -1 )
					word_start_offset = curPos;
			}
		}

		curPos++;
	}

	cur.setPosition( curPos, QTextCursor::MoveAnchor );
	setTextCursor( cur );
	ensureCursorMiddle();
}

void Editor::removeLastTimeTag()
{
	undo();
}
