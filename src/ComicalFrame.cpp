/*
 * ComicalFrame.cpp
 * Copyright (c) 2003-2011 James Athey
 */

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   In addition, as a special exception, the author gives permission to   *
 *   link the code of his release of Comical with Rarlabs' "unrar"         *
 *   library (or with modified versions of it that use the same license    *
 *   as the "unrar" library), and distribute the linked executables. You   *
 *   must obey the GNU General Public License in all respects for all of   *
 *   the code used other than "unrar". If you modify this file, you may    *
 *   extend this exception to your version of the file, but you are not    *
 *   obligated to do so. If you do not wish to do so, delete this          *
 *   exception statement from your version.                                *
 *                                                                         *
 ***************************************************************************/

#include "ComicalFrame.h"
#include "ComicBookRAR.h"
#include "ComicBookZIP.h"
#include "ComicBookDir.h"
#include "Exceptions.h"

#include <wx/artprov.h>
#include <wx/dirdlg.h>
#include <wx/event.h>
#include <wx/filedlg.h>
#include <wx/log.h>
#include <wx/mimetype.h>
#include <wx/minifram.h>
#include <wx/msgdlg.h>
#include <wx/scrolbar.h>
#include <wx/numdlg.h>
#include <wx/utils.h>
#include <wx/wfstream.h>

// and the icons
#include "firstpage.h"
#include "prevpage.h"
#include "prev.h"
#include "next.h"
#include "nextpage.h"
#include "lastpage.h"
#include "rot_cw.h"
#include "rot_ccw.h"
#include "fullscreen.h"

ComicalFrame::ComicalFrame(const wxString& title, const wxPoint& pos, const wxSize& size):
wxFrame(NULL, -1, title, pos, size, wxDEFAULT_FRAME_STYLE | wxNO_FULL_REPAINT_ON_RESIZE),
m_timerToolbarHide(this, ID_HideTimer),
theCanvas(NULL),
theBook(NULL),
theBrowser(NULL),
toolBarNav(NULL),
labelLeft(NULL),
labelRight(NULL),
toolbarSizer(new wxBoxSizer(wxHORIZONTAL))
{
	m_auiManager.SetManagedWindow(this);

	config = wxConfig::Get();
	
	// Get the thickness of scrollbars.  Knowing this, we can precalculate
	// whether the current page(s) will need scrollbars.
	wxScrollBar *tempBar = new wxScrollBar(this, -1);
	scrollbarThickness = tempBar->GetSize().y;
	tempBar->Destroy();

	menuFile = new wxMenu();

	wxMenuItem *openMenu = new wxMenuItem(menuFile, wxID_OPEN, wxT("&Open\tAlt-O"), wxT("Open a Comic Book."));
	wxMenuItem *exitMenu = new wxMenuItem(menuFile, wxID_EXIT, wxT("E&xit\tAlt-X"), wxT("Quit Comical."));

	wxBitmap openIcon = wxArtProvider::GetBitmap(wxART_FILE_OPEN, wxART_MENU, wxDefaultSize);
	wxBitmap exitIcon = wxArtProvider::GetBitmap(wxART_QUIT, wxART_MENU, wxDefaultSize);
	openMenu->SetBitmap(openIcon);
	exitMenu->SetBitmap(exitIcon);
	
	menuFile->Append(openMenu);
	menuFile->Append(ID_OpenDir, wxT("Open &Directory"), wxT("Open a directory of images."));
	menuFile->AppendSeparator();
	menuFile->Append(exitMenu);

	menuGo = new wxMenu();

	wxMenuItem *prevMenu = new wxMenuItem(menuGo, ID_PrevSlide, wxT("Previous Page"), wxT("Display the previous page."));
	wxMenuItem *nextMenu = new wxMenuItem(menuGo, ID_NextSlide, wxT("Next Page"), wxT("Display the next page."));
	wxMenuItem *prevTurnMenu = new wxMenuItem(menuGo, ID_PrevTurn, wxT("&Previous Page Turn"), wxT("Display the previous two pages."));
	wxMenuItem *nextTurnMenu = new wxMenuItem(menuGo, ID_NextTurn, wxT("&Next Page Turn"), wxT("Display the next two pages."));
	wxMenuItem *firstMenu = new wxMenuItem(menuGo, ID_First, wxT("&First Page"), wxT("Display the first page."));
	wxMenuItem *lastMenu = new wxMenuItem(menuGo, ID_Last, wxT("&Last Page"), wxT("Display the last page."));

	prevMenu->SetBitmap(wxGetBitmapFromMemory(prev));
	nextMenu->SetBitmap(wxGetBitmapFromMemory(next));
	prevTurnMenu->SetBitmap(wxGetBitmapFromMemory(prevpage));
	nextTurnMenu->SetBitmap(wxGetBitmapFromMemory(nextpage));
	firstMenu->SetBitmap(wxGetBitmapFromMemory(firstpage));
	lastMenu->SetBitmap(wxGetBitmapFromMemory(lastpage));

	menuGo->Append(prevMenu);
	menuGo->Append(nextMenu);
	menuGo->AppendSeparator();
	menuGo->Append(prevTurnMenu);
	menuGo->Append(nextTurnMenu);
	menuGo->AppendSeparator();
	menuGo->Append(firstMenu);
	menuGo->Append(lastMenu);
	menuGo->Append(ID_GoTo, wxT("&Go to page..."), wxT("Jump to another page number."));
	menuGo->AppendSeparator();
	menuGo->Append(ID_Buffer, wxT("&Page Buffer Length..."), wxT("Set the number of pages Comical prefetches."));
	menuView = new wxMenu();

	menuZoom = new wxMenu();
	menuZoom->AppendRadioItem(ID_Fit, wxT("Fit"), wxT("Scale pages to fit the window."));
	menuZoom->AppendRadioItem(ID_FitV, wxT("Fit to Height"), wxT("Scale pages to fit the window's height."));
	menuZoom->AppendRadioItem(ID_FitH, wxT("Fit to Width"), wxT("Scale pages to fit the window's width."));
	menuZoom->AppendRadioItem(ID_Unzoomed, wxT("Original Size"), wxT("Show pages without resizing them."));
	menuZoom->AppendRadioItem(ID_Custom, wxT("Custom Zoom"), wxT("Scale pages to a custom percentage of their original size."));
	menuZoom->AppendSeparator();
	wxMenuItem *fitOnlyOversizeMenu = menuZoom->AppendCheckItem(ID_FitOnlyOversize, wxT("Fit Oversized Pages Only"), wxT("Check to see if a page will fit on screen before resizing it."));
	menuZoom->Append(ID_SetCustom, wxT("Set Custom Zoom Level..."), wxT("Choose the percentage that the Custom Zoom mode will use."));
	menuView->Append(ID_S, wxT("&Zoom"), menuZoom);
	
	menuRotate = new wxMenu();
	menuRotate->AppendRadioItem(ID_North, wxT("Normal"), wxT("No rotation."));
	menuRotate->AppendRadioItem(ID_East, wxT("90 Clockwise"), wxT("Rotate 90 degrees clockwise."));
	menuRotate->AppendRadioItem(ID_South, wxT("180"), wxT("Rotate 180 degrees."));
	menuRotate->AppendRadioItem(ID_West, wxT("90 Counter-Clockwise"), wxT("Rotate 90 degrees counter-clockwise."));
	menuView->Append(ID_Rotate, wxT("&Rotate"), menuRotate);

	menuRotateLeft = new wxMenu();
	menuRotateLeft->AppendRadioItem(ID_NorthLeft, wxT("Normal"), wxT("No rotation."));
	menuRotateLeft->AppendRadioItem(ID_EastLeft, wxT("90 Clockwise"), wxT("Rotate 90 degrees clockwise."));
	menuRotateLeft->AppendRadioItem(ID_SouthLeft, wxT("180"), wxT("Rotate 180 degrees."));
	menuRotateLeft->AppendRadioItem(ID_WestLeft, wxT("90 Counter-Clockwise"), wxT("Rotate 90 degrees counter-clockwise."));
	menuView->Append(ID_RotateLeft, wxT("Rotate Left Page"), menuRotateLeft);

	menuRotateRight = new wxMenu();
	menuRotateRight->AppendRadioItem(ID_North, wxT("Normal"), wxT("No rotation."));
	menuRotateRight->AppendRadioItem(ID_East, wxT("90 Clockwise"), wxT("Rotate 90 degrees clockwise."));
	menuRotateRight->AppendRadioItem(ID_South, wxT("180"), wxT("Rotate 180 degrees."));
	menuRotateRight->AppendRadioItem(ID_West, wxT("90 Counter-Clockwise"), wxT("Rotate 90 degrees counter-clockwise."));
	menuView->Append(ID_RotateRight, wxT("Rotate Ri&ght Page"), menuRotateRight);

	menuMode = new wxMenu();
	menuMode->AppendRadioItem(ID_Single, wxT("Single Page"), wxT("Show only a single page at a time."));
	menuMode->AppendRadioItem(ID_Double, wxT("Double Page"), wxT("Show two pages at a time."));
	menuView->Append(ID_M, wxT("&Mode"), menuMode);

	menuDirection = new wxMenu();
	menuDirection->AppendRadioItem(ID_LeftToRight, wxT("&Left-to-Right"), wxT("Turn pages from left-to-right, suitable for Western comics."));
	menuDirection->AppendRadioItem(ID_RightToLeft, wxT("&Right-to-Left"), wxT("Turn pages from right-to-left, suitable for Manga."));
	menuView->Append(ID_D, wxT("&Direction"), menuDirection);
	
	menuFilter = new wxMenu();
	menuFilter->AppendRadioItem(ID_Box, wxT("Box"), wxT("Use the Box filter."));
	menuFilter->AppendRadioItem(ID_Bilinear, wxT("Bilinear"), wxT("Use the Bilinear filter."));
	menuFilter->AppendRadioItem(ID_Bicubic, wxT("Bicubic"), wxT("Use the Bicubic filter."));
	menuFilter->AppendRadioItem(ID_BSpline, wxT("B-Spline"), wxT("Use the B-Spline filter."));
	menuFilter->AppendRadioItem(ID_CatmullRom, wxT("Catmull-Rom"), wxT("Use the Catmull-Rom filter."));
	menuFilter->AppendRadioItem(ID_Lanczos, wxT("Lanczos 3"), wxT("Use the Box filter."));
	menuView->Append(ID_S, wxT("&Image Filter"), menuFilter);

	wxMenuItem *fsMenu = new wxMenuItem(menuView, ID_Full, wxT("Full &Screen\tAlt-Return"), wxT("Display Full Screen."));
	fsMenu->SetBitmap(wxGetBitmapFromMemory(fullscreen));
	
	menuView->AppendSeparator();
	menuView->Append(fsMenu);
	menuView->AppendSeparator();
	wxMenuItem *browserMenu = menuView->AppendCheckItem(ID_Browser, wxT("Thumbnail Browser"), wxT("Show/Hide the thumbnail browser"));

	menuHelp = new wxMenu();
	menuHelp->Append(wxID_ABOUT, wxT("&About...\tF1"), wxT("Display About Dialog."));
	menuHelp->Append(ID_Homepage, wxT("&Comical Homepage"), wxT("Go to http://comical.sourceforge.net/"));

	menuBar = new wxMenuBar();
	menuBar->Append(menuFile, wxT("&File"));
	menuBar->Append(menuGo, wxT("&Go"));
	menuBar->Append(menuView, wxT("&View"));
	menuBar->Append(menuHelp, wxT("&Help"));

	SetMenuBar(menuBar);

	// Each of the long values is followed by the letter L not the number one
	cacheLen = (wxUint32) config->Read(wxT("CacheLength"), 10l); // Fit-to-Width is default
	zoom = (COMICAL_ZOOM) config->Read(wxT("Zoom"), 2l); // Fit-to-Width is default
	zoomLevel = config->Read(wxT("ZoomLevel"), 100l); // 100% is the default
	mode = (COMICAL_MODE) config->Read(wxT("Mode"), 1l); // Double-Page is default
	filter = (FREE_IMAGE_FILTER) config->Read(wxT("Filter"), 4l); // Catmull-Rom is default
	direction = (COMICAL_DIRECTION) config->Read(wxT("Direction"), 0l); // Left-To-Right by default
	fitOnlyOversize = (bool) config->Read(wxT("FitOnlyOversize"), 0l); // off by default
	browserActive = (bool) config->Read(wxT("BrowserActive"), 1l); // Shown by default

	// Record the settings from the config in the menus
	menuZoom->FindItemByPosition(zoom)->Check(true);
	menuMode->FindItemByPosition(mode)->Check(true);
	menuFilter->FindItemByPosition(filter)->Check(true);
	menuDirection->FindItemByPosition(direction)->Check(true);
	fitOnlyOversizeMenu->Check(fitOnlyOversize);
	browserMenu->Check(browserActive);
	if (mode == ONEPAGE) {
		prevTurnMenu->Enable(false);
		nextTurnMenu->Enable(false);
	} // else they're already enabled

	theBrowser = new ComicalBrowser(this, 110);
	
	theCanvas = new ComicalCanvas(this, mode, direction, scrollbarThickness);
	theCanvas->Connect(EVT_PAGE_SHOWN, wxCommandEventHandler(ComicalFrame::OnPageShown), NULL, this);
	theCanvas->Connect(ID_ContextFull, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(ComicalFrame::OnFull), NULL, this);

	theBrowser->SetComicalCanvas(theCanvas);

	wxMiniFrame *toolbarFrame = new wxMiniFrame(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
			wxCLIP_CHILDREN | wxFRAME_NO_TASKBAR | wxFRAME_FLOAT_ON_PARENT | wxFRAME_TOOL_WINDOW);
	m_frameNav = toolbarFrame;

	toolBarNav = new wxToolBar(toolbarFrame, -1, wxDefaultPosition, wxDefaultSize,
			wxNO_BORDER | wxTB_HORIZONTAL | wxTB_FLAT);
	toolBarNav->SetToolBitmapSize(wxSize(16, 16));
	toolBarNav->AddTool(ID_CCWL, wxT("Rotate Counter-Clockwise (left page)"), wxGetBitmapFromMemory(rot_ccw), wxT("Rotate Counter-Clockwise (left page)"));
	toolBarNav->AddTool(ID_CWL, wxT("Rotate Clockwise (left page)"), wxGetBitmapFromMemory(rot_cw), wxT("Rotate Clockwise (left page)"));
	toolBarNav->AddSeparator();
	toolBarNav->AddTool(ID_First, wxT("First Page"), wxGetBitmapFromMemory(firstpage), wxT("First Page"));
	toolBarNav->AddTool(ID_PrevTurn, wxT("Previous Page Turn"), wxGetBitmapFromMemory(prevpage), wxT("Previous Page Turn"));
	toolBarNav->AddTool(ID_PrevSlide, wxT("Previous Page"), wxGetBitmapFromMemory(prev), wxT("Previous Page"));
//	toolBarNav->AddSeparator();
//	toolBarNav->AddTool(ID_ZoomBox, wxT("Zoom"), wxGetBitmapFromMemory(rot_cw), wxT("Zoom"), wxITEM_CHECK); // Zoom Box disabled for now
//	toolBarNav->AddSeparator();
	toolBarNav->AddTool(ID_NextSlide, wxT("Next Page"), wxGetBitmapFromMemory(next), wxT("Next Page"));
	toolBarNav->AddTool(ID_NextTurn, wxT("Next Page Turn"), wxGetBitmapFromMemory(nextpage), wxT("Next Page Turn"));
	toolBarNav->AddTool(ID_Last, wxT("Last Page"), wxGetBitmapFromMemory(lastpage), wxT("Last Page"));
	toolBarNav->AddSeparator();
	toolBarNav->AddTool(ID_CCW, wxT("Rotate Counter-Clockwise"), wxGetBitmapFromMemory(rot_ccw), wxT("Rotate Counter-Clockwise"));
	toolBarNav->AddTool(ID_CW, wxT("Rotate Clockwise"), wxGetBitmapFromMemory(rot_cw), wxT("Rotate Clockwise"));
	toolBarNav->Enable(false);
	toolBarNav->Fit();
	toolBarNav->Realize();

	labelLeft = new wxStaticText(this, -1, wxT(""), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT | wxST_NO_AUTORESIZE);
	labelRight = new wxStaticText(this, -1, wxT(""), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT | wxST_NO_AUTORESIZE);
	wxFont font = labelLeft->GetFont();
	font.SetPointSize(10);
	labelLeft->SetFont(font);
	labelRight->SetFont(font);
	
	toolbarSizer->Add(labelLeft, 1, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 0);
	toolbarSizer->AddSpacer(10);
	toolbarSizer->Add(toolBarNav, 0, wxALIGN_CENTER, 0);
	toolbarSizer->AddSpacer(10);
	toolbarSizer->Add(labelRight, 1, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL, 0);
	toolbarSizer->Layout();
	
	toolbarFrame->SetSizer(toolbarSizer);
	toolbarFrame->SetClientSize(toolbarSizer->ComputeFittingClientSize(toolbarFrame));

	wxAuiPaneInfo canvasPaneInfo;
	canvasPaneInfo.CenterPane()
		.DestroyOnClose(true)
		.Resizable(true);
	m_auiManager.AddPane(theCanvas, canvasPaneInfo);

	wxAuiPaneInfo browserPaneInfo;
	browserPaneInfo.CloseButton(true)
			.Dock()
			.Floatable(false)
			.Left()
			.MaximizeButton(false)
			.MinimizeButton(false)
			.MinSize(wxSize(50, 1))
			.Movable(true)
			.Name(wxT("Browser"))
			.Resizable(true);
	m_auiManager.AddPane(theBrowser, browserPaneInfo);

	m_auiManager.Update();

	RepositionToolbar();
}

BEGIN_EVENT_TABLE(ComicalFrame, wxFrame)
	EVT_MENU(wxID_EXIT,	ComicalFrame::OnQuit)
	EVT_MENU(wxID_ABOUT,	ComicalFrame::OnAbout)
	EVT_MENU(wxID_OPEN,	ComicalFrame::OnOpen)
	EVT_MENU(ID_OpenDir,	ComicalFrame::OnOpenDir)
	EVT_MENU(ID_GoTo,	ComicalFrame::OnGoTo)
	EVT_MENU(ID_Buffer,	ComicalFrame::OnBuffer)
	EVT_MENU(ID_Full,	ComicalFrame::OnFull)
	EVT_MENU_RANGE(ID_Single, ID_Double, ComicalFrame::OnMode)
	EVT_MENU_RANGE(ID_Fit, ID_Custom, ComicalFrame::OnZoom)
	EVT_MENU(ID_FitOnlyOversize, ComicalFrame::OnFitOnlyOversize)
	EVT_MENU(ID_SetCustom, ComicalFrame::OnSetCustom)
	EVT_MENU_RANGE(ID_Box, ID_Lanczos, ComicalFrame::OnFilter)
	EVT_MENU_RANGE(ID_LeftToRight, ID_RightToLeft, ComicalFrame::OnDirection)
//	EVT_MENU(ID_ZoomBox, ComicalFrame::OnZoomBox)
	EVT_MENU(ID_Browser, ComicalFrame::OnBrowser)
	EVT_MENU(ID_Homepage, ComicalFrame::OnHomepage)
	EVT_CLOSE(ComicalFrame::OnClose)
	EVT_MOVE(ComicalFrame::OnMove)
	EVT_TIMER(ID_HideTimer, ComicalFrame::OnToolbarHideTimer)
END_EVENT_TABLE()


ComicalFrame::~ComicalFrame()
{
	m_auiManager.UnInit();
}


void ComicalFrame::OnClose(wxCloseEvent& event)
{
	clearComicBook();
	theCanvas->ClearCanvas();
	delete theCanvas;

	wxRect frameDim = GetRect();
	config->Write(wxT("CacheLength"), (int) cacheLen);
	config->Write(wxT("Zoom"), int(zoom));
	config->Write(wxT("ZoomLevel"), zoomLevel);
	config->Write(wxT("FitOnlyOversize"), fitOnlyOversize);
	config->Write(wxT("Filter"), int(filter));
	config->Write(wxT("Mode"), int(mode));
	config->Write(wxT("Direction"), int(direction));
	config->Write(wxT("FrameWidth"), frameDim.width);
	config->Write(wxT("FrameHeight"), frameDim.height);
	config->Write(wxT("FrameX"), frameDim.x);
	config->Write(wxT("FrameY"), frameDim.y);
	config->Write(wxT("BrowserActive"), browserActive);
	Destroy();	// Close the window
}

void ComicalFrame::OnQuit(wxCommandEvent& event)
{
	Close(TRUE);
}

void ComicalFrame::OnAbout(wxCommandEvent& event)
{
	wxMessageDialog AboutDlg(this, wxT("Comical 0.9, (c) 2003-2011 James Athey.\nComical is licensed under the GPL, version 2 or later,\nwith a linking exception; see README for details."), wxT("About Comical"), wxOK);
	AboutDlg.ShowModal();
}

void ComicalFrame::OnOpen(wxCommandEvent& event)
{
	wxString cwd;
	config->Read(wxT("CWD"), &cwd);
	wxString filename = wxFileSelector(wxT("Open a Comic Book"), cwd, wxT(""), wxT(""), wxT("Comic Books (*.cbr,*.cbz,*.rar,*.zip)|*.cbr;*.CBR;*.cbz;*.CBZ;*.rar;*.RAR;*.zip;*.ZIP"), wxFD_OPEN | wxFD_CHANGE_DIR | wxFD_FILE_MUST_EXIST, this);

	if (!filename.empty())
		OpenFile(filename);
}

void ComicalFrame::OnOpenDir(wxCommandEvent& event)
{
	wxString cwd;
	config->Read(wxT("CWD"), &cwd);
	wxString dir = wxDirSelector(wxT("Open a Directory"), cwd, 0, wxDefaultPosition, this);

	if (!dir.empty())
		OpenDir(dir);
}

void ComicalFrame::OpenFile(const wxString& filename)
{
	ComicBook *newBook;
	
	static const wxUint8 zipHeader[] = { 0x50, 0x4b, 0x03, 0x04 };
	static const wxUint8 rarHeader[] = { 0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x00 };

	wxUint8 fileHeader[sizeof(rarHeader)];

	if (filename.empty())
		return;
		
	clearComicBook();
	try {
		wxFileInputStream filestream(filename);
		if (!filestream.IsOk())
			throw ArchiveException(filename, wxString(wxT("Could not open the file.")));

		filestream.Read(fileHeader, sizeof(fileHeader));
		if (filestream.LastRead() != sizeof(fileHeader))
			throw ArchiveException(filename, wxString(wxT("Not a valid comic book archive.")));

		if (memcmp(fileHeader, rarHeader, sizeof(rarHeader)) == 0)
			newBook = new ComicBookRAR(filename, cacheLen, zoom, zoomLevel, fitOnlyOversize, mode, filter, direction, scrollbarThickness);
		else if (memcmp(fileHeader, zipHeader, sizeof(zipHeader)) == 0)
			newBook = new ComicBookZIP(filename, cacheLen, zoom, zoomLevel, fitOnlyOversize, mode, filter, direction, scrollbarThickness);
		else
			throw ArchiveException(filename, wxString(wxT("Not a valid comic book archive.")));
		
		if (newBook->GetPageCount() == 0) {
			wxLogError(wxT("The archive \"") + filename + wxT("\" does not contain any pages."));
			wxLog::FlushActive();
			return;
		}
		setComicBook(newBook);
		startBook();
		SetTitle(wxT("Comical - " + filename));
		config->Write(wxT("CWD"), wxPathOnly(filename));
	} catch (ArchiveException &ae) {
		clearComicBook();
		wxMessageDialog errDlg(this, ae.Message, wxT("Open Failed"), wxOK | wxICON_ERROR);
		errDlg.ShowModal();
	}
}

void ComicalFrame::OpenDir(const wxString& directory)
{
	ComicBook *newBook;
	
	if (directory.empty())
		return;
		
	clearComicBook();
	try {
		newBook = new ComicBookDir(directory, cacheLen, zoom, zoomLevel, fitOnlyOversize, mode, filter, direction, scrollbarThickness);
		setComicBook(newBook);
		startBook();
		SetTitle(wxT("Comical - " + directory));
		config->Write(wxT("CWD"), directory);
	} catch (ArchiveException &ae) {
		wxLogError(ae.Message);
		wxLog::FlushActive();
	}
}

void ComicalFrame::startBook()
{
	if (!theBook)
		return;

	theCanvas->SetComicBook(theBook);
	theBrowser->SetComicBook(theBook);
	theBrowser->SetItemCount(theBook->GetPageCount());

	toolBarNav->Enable(true);

	theBook->Run(); // start the thread

	theCanvas->FirstPage();
	theCanvas->Scroll(-1, 0); // scroll to the top for the first page
}

void ComicalFrame::OnGoTo(wxCommandEvent& event)
{
	wxString message ;
	long pagenumber;
	
	if (!theBook)
		return;
		
	message = wxT("Enter a page number from 1 to ");
	message += wxString::Format(wxT("%d"), theBook->GetPageCount());
	pagenumber = wxGetNumberFromUser(message, wxT("Page"), wxT("Go To Page"), theBook->GetCurrentPage() + 1, 1, theBook->GetPageCount(), this);
	if (pagenumber != -1)
		theCanvas->GoToPage(pagenumber - 1);

}

void ComicalFrame::OnBuffer(wxCommandEvent& event)
{
	wxString message;
	long buffer;
	
	if (!theBook)
		return;
		
	message = wxT("Set the number of pages you would like Comical to prefetch.");
	buffer = wxGetNumberFromUser(message, wxT("Buffer Length"), wxT("Set Buffer Length"), theBook->GetCacheLen(), 3, 20, this);
	if (buffer != -1)
		theBook->SetCacheLen(buffer);
}

void ComicalFrame::OnFull(wxCommandEvent& event)
{
	if (IsFullScreen()) {
		//if (theBrowser)
			//frameSizer->Show(theBrowser, browserActive);
		ShowFullScreen(false, wxFULLSCREEN_ALL);
	} else {
		//bookPanelSizer->Show(toolbarSizer, false, true);
		//if (theBrowser)
			//frameSizer->Show(theBrowser, false);
		ShowFullScreen(true, wxFULLSCREEN_ALL);		
	}
}

void ComicalFrame::OnMode(wxCommandEvent& event)
{
	wxMenuItem *prev = menuGo->FindItem(ID_PrevTurn);
	wxMenuItem *next = menuGo->FindItem(ID_NextTurn);
	
	switch (event.GetId()) {
	
	case ID_Single:
		prev->Enable(false);
		next->Enable(false);
		mode = ONEPAGE;
		break;
	
	case ID_Double:
		prev->Enable(true);
		next->Enable(true);
		mode = TWOPAGE;
		break;
	
	default:
		return;
	}

	if (theBook)
		theBook->SetMode(mode);
	if (theCanvas)
		theCanvas->SetMode(mode); // In this case, theCanvas decides how to redraw
}

void ComicalFrame::OnZoomBox(wxCommandEvent &event)
{
	if (theCanvas != NULL)
		theCanvas->SetZoomEnable(event.IsChecked());
}

void ComicalFrame::OnBrowser(wxCommandEvent &event)
{
	browserActive = event.IsChecked();
	m_auiManager.GetPane(wxT("Browser")).Show(browserActive);
	m_auiManager.Update();
}

void ComicalFrame::OnPageError(wxCommandEvent &event)
{
	wxLogError(event.GetString());
	wxLog::FlushActive();
}

void ComicalFrame::OnHomepage(wxCommandEvent &event)
{
	wxLaunchDefaultBrowser(wxT("http://comical.sourceforge.net/"));
}

void ComicalFrame::OnZoom(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_Fit:
		zoom = ZOOM_FIT;
		if (theCanvas)
			theCanvas->EnableScrolling(false, false); // Fit, no scrolling
		break;
	case ID_FitV:
		zoom = ZOOM_HEIGHT;
		if (theCanvas)
			theCanvas->EnableScrolling(true, false); // Vertical fit, no vertical scrolling
		break;
	case ID_FitH:
		zoom = ZOOM_WIDTH;
		if (theCanvas)
			theCanvas->EnableScrolling(false, true); // Horizontal fit, no horizontal scrolling
		break;
	case ID_Unzoomed:
		zoom = ZOOM_FULL;
		if (theCanvas)
			theCanvas->EnableScrolling(true, true);	
		break;
	case ID_Custom:
		zoom = ZOOM_CUSTOM;
		if (theCanvas)
			theCanvas->EnableScrolling(true, true);	
		break;
	default:
		wxLogError(wxT("Zoom mode %d is undefined."), event.GetId()); // we shouldn't be here... honest!
		return;
	}

	if (theBook && theBook->SetZoom(zoom) && theBook->IsRunning())
		theCanvas->ResetView();
}

void ComicalFrame::OnSetCustom(wxCommandEvent& event)
{
	long newZoomLevel = wxGetNumberFromUser(wxT("Set the zoom level (in %)"), wxT("Zoom"), wxT("Custom Zoom"), zoomLevel, 1, 200);
	if (newZoomLevel < 0) {
		return;
	}
	zoomLevel = newZoomLevel;
	if (theBook && theBook->SetZoomLevel(zoomLevel) && theBook->IsRunning() && theCanvas && zoom == ZOOM_CUSTOM)
		theCanvas->ResetView();
}

void ComicalFrame::OnFilter(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_Box:
		filter = FILTER_BOX;
		break;
	case ID_Bilinear:
		filter = FILTER_BILINEAR;
		break;
	case ID_Bicubic:
		filter = FILTER_BICUBIC;
		break;
	case ID_BSpline:
		filter = FILTER_BSPLINE;
		break;
	case ID_CatmullRom:
		filter = FILTER_CATMULLROM;
		break;
	case ID_Lanczos:
		filter = FILTER_LANCZOS3;
		break;
	default:
		wxLogError(wxT("Filter %d is undefined."), event.GetId()); // we shouldn't be here... honest!
		return;
	}
	
	if (theBook && theBook->SetFilter(filter) && theBook->IsRunning())
		theCanvas->ResetView();
}

void ComicalFrame::OnFitOnlyOversize(wxCommandEvent& event)
{
	fitOnlyOversize = event.IsChecked();
	if (theBook && theBook->SetFitOnlyOversize(fitOnlyOversize) && theBook->IsRunning())
		theCanvas->ResetView();
}

void ComicalFrame::OnDirection(wxCommandEvent& event)
{
	switch (event.GetId()) {
	case ID_LeftToRight:
		direction = COMICAL_LTR;
		break;
	case ID_RightToLeft:
		direction = COMICAL_RTL;
		break;
	default:
		wxLogError(wxT("I don't know what direction this is: %d"), event.GetId());
		return;
	}
	if (theCanvas)
		theCanvas->SetDirection(direction);
	if (theBook && theBook->SetDirection(direction) && theBook->IsRunning())
		theCanvas->ResetView();	
}

void ComicalFrame::OnPageShown(wxCommandEvent& event)
{
	wxUint32 leftNum = theCanvas->GetLeftNum();
	wxUint32 rightNum = theCanvas->GetRightNum();
	
	if (mode == ONEPAGE || theBook->GetPageCount() == 1 || leftNum == rightNum) {
		menuView->Enable(ID_RotateLeft, false);
		menuView->Enable(ID_RotateRight, false);
		menuView->Enable(ID_Rotate, true);
		menuRotate->FindItemByPosition(theBook->GetPageOrientation(theBook->GetCurrentPage()))->Check();
		toolBarNav->EnableTool(ID_CCWL, false);
		toolBarNav->EnableTool(ID_CWL, false);
		toolBarNav->EnableTool(ID_CCW, true);
		toolBarNav->EnableTool(ID_CW, true);
		
		labelLeft->SetLabel(wxT(""));
		labelRight->SetLabel(wxString::Format(wxT("%d of %d"), theBook->GetCurrentPage() + 1, theBook->GetPageCount()));
	} else {
		menuView->Enable(ID_Rotate, false);

		if (theCanvas->IsRightPageOk()) {
			menuView->Enable(ID_RotateRight, true);
			menuRotateRight->FindItemByPosition(theBook->GetPageOrientation(rightNum))->Check();
			toolBarNav->EnableTool(ID_CCW, true);
			toolBarNav->EnableTool(ID_CW, true);
			if (direction == COMICAL_LTR)
				labelRight->SetLabel(wxString::Format(wxT("%d of %d"), rightNum + 1, theBook->GetPageCount()));
			else // direction == COMICAL_RTL
				labelLeft->SetLabel(wxString::Format(wxT("%d of %d"), rightNum + 1, theBook->GetPageCount()));
		} else {
			menuView->Enable(ID_RotateRight, false);
			toolBarNav->EnableTool(ID_CCW, false);
			toolBarNav->EnableTool(ID_CW, false);
			if (direction == COMICAL_LTR)
				labelRight->SetLabel(wxT(""));
			else // direction == COMICAL_RTL
				labelLeft->SetLabel(wxT(""));
		}
	
		if (theCanvas->IsLeftPageOk()) {
			menuView->Enable(ID_RotateLeft, true);
			menuRotateLeft->FindItemByPosition(theBook->GetPageOrientation(leftNum))->Check();
			toolBarNav->EnableTool(ID_CCWL, true);
			toolBarNav->EnableTool(ID_CWL, true);
			
			if (direction == COMICAL_LTR)
				labelLeft->SetLabel(wxString::Format(wxT("%d of %d"), leftNum + 1, theBook->GetPageCount()));
			else // direction == COMICAL_RTL
				labelRight->SetLabel(wxString::Format(wxT("%d of %d"), leftNum + 1, theBook->GetPageCount()));
		} else {
			menuView->Enable(ID_RotateLeft, false);
			toolBarNav->EnableTool(ID_CCWL, false);
			toolBarNav->EnableTool(ID_CWL, false);
			if (direction == COMICAL_LTR)
				labelLeft->SetLabel(wxT(""));
			else // direction == COMICAL_RTL
				labelRight->SetLabel(wxT(""));
		}
	}

	toolBarNav->Realize();

}


void ComicalFrame::OnMove(wxMoveEvent& event)
{
	RepositionToolbar();
}

void ComicalFrame::RepositionToolbar()
{
	if (!m_frameNav)
		return;

	wxSize canvasSize = theCanvas->GetClientSize();
	wxPoint canvasPos = theCanvas->GetScreenPosition();

	wxSize toolbarSize = m_frameNav->GetSize();

	m_frameNav->Move(wxPoint(canvasPos.x + ((canvasSize.x - toolbarSize.x) / 2),
			canvasPos.y + canvasSize.y - (toolbarSize.y * 2)));
}


void ComicalFrame::ShowToolbar()
{
	m_frameNav->Show();
	m_frameNav->SetTransparent(255);

	// hide the toolbar after 2 seconds, only fire the event once
	m_timerToolbarHide.Start(2000, true);
}


void ComicalFrame::OnToolbarHideTimer(wxTimerEvent& event)
{
	// Only start hiding if the mouse is not hovering over the toolbar
	wxPoint mousePos = wxGetMousePosition();
	wxPoint framePos = m_frameNav->GetScreenPosition();
	wxSize frameSize = m_frameNav->GetSize();

	if (mousePos.x < framePos.x || mousePos.x > framePos.x + frameSize.x ||
			mousePos.y < framePos.y || mousePos.y > framePos.y + frameSize.y)
		m_frameNav->Hide();
}


void ComicalFrame::setComicBook(ComicBook *newBook)
{
	if (theBook)
		clearComicBook();
	theBook = newBook;
	if (theBook) {
		theBook->Connect(EVT_PAGE_ERROR, wxCommandEventHandler(ComicalFrame::OnPageError), NULL, this);
	}
}

void ComicalFrame::clearComicBook()
{
	if (theCanvas)
		theCanvas->SetComicBook(NULL);
	if (theBrowser)
		theBrowser->SetComicBook(NULL);
	if (theBook) {
		theBook->Disconnect(EVT_PAGE_ERROR, wxCommandEventHandler(ComicalFrame::OnPageError), NULL, this);
		theBook->Delete(); // delete the ComicBook thread
		delete theBook; // clear out the rest of the ComicBook
		theBook = NULL;
	}
	if (theCanvas)
		theCanvas->ClearCanvas();
	if (theBrowser)
		theBrowser->ClearBrowser();
}
