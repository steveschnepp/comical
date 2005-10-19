/***************************************************************************
              ComicalCanvas.cpp - ComicalCanvas implementation
                             -------------------
    begin                : Thu Dec 18 2003
    copyright            : (C) 2004-2005 by James Athey
    email                : jathey@comcast.net
 ***************************************************************************/

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

#include "ComicalCanvas.h"
#include "ComicalApp.h"

IMPLEMENT_DYNAMIC_CLASS(ComicalCanvas, wxScrolledWindow)

#if wxCHECK_VERSION(2, 5, 1)
ComicalCanvas::ComicalCanvas(wxWindow *prnt, const wxPoint &pos, const wxSize &size) : wxScrolledWindow(prnt, -1, pos, size, wxNO_BORDER | wxFULL_REPAINT_ON_RESIZE)
#else
ComicalCanvas::ComicalCanvas(wxWindow *prnt, const wxPoint &pos, const wxSize &size) : wxScrolledWindow(prnt, -1, pos, size, wxNO_BORDER)
#endif
{
	parent = prnt;
	SetBackgroundColour(* wxBLACK);
	wxConfigBase *config = wxConfigBase::Get();
	// Each of the long values is followed by the letter L not the number one
	zoom = (COMICAL_ZOOM) config->Read(wxT("/Comical/Zoom"), 2l); // Fit-to-Width is default
	mode = (COMICAL_MODE) config->Read(wxT("/Comical/Mode"), 1l); // Double-Page is default
	filter = (FREE_IMAGE_FILTER) config->Read(wxT("/Comical/Filter"), 4l); // Catmull-Rom is default
	leftPage = rightPage = centerPage = NULL;
	theBook = NULL;
	
	// Get the thickness of scrollbars.  Knowing this, we can precalculate
	// whether the current page(s) will need scrollbars.
	wxScrollBar *tempBar = new wxScrollBar(this, -1);
	scrollBarThickness = tempBar->GetSize().y;
	tempBar->Destroy();
}

BEGIN_EVENT_TABLE(ComicalCanvas, wxScrolledWindow)
	EVT_PAINT(ComicalCanvas::OnPaint)
	EVT_KEY_DOWN(ComicalCanvas::OnKeyDown)
	EVT_SIZE(ComicalCanvas::OnSize)
END_EVENT_TABLE()

ComicalCanvas::~ComicalCanvas()
{
	clearBitmaps();
	wxConfigBase *config = wxConfigBase::Get();
	config->Write(wxT("/Comical/Zoom"), zoom);
	config->Write(wxT("/Comical/Mode"), mode);
	config->Write(wxT("/Comical/Filter"), filter);
}

void ComicalCanvas::clearBitmap(wxBitmap *&bitmap)
{
	if (bitmap && bitmap->Ok())
	{
		delete bitmap;
		bitmap = NULL;
	}
}

void ComicalCanvas::clearBitmaps()
{
	clearBitmap(leftPage);
	clearBitmap(centerPage);
	clearBitmap(rightPage);
}

void ComicalCanvas::createBitmaps()
{
	wxInt32 xScroll = 0, yScroll = 0, xWindow, yWindow;
	bool leftOk = false, rightOk = false;
	
	ComicalFrame *cParent = (ComicalFrame *) parent;

	if (mode == ONEPAGE || theBook->GetPageCount() == 1 || leftNum == rightNum) {
		if (mode == ONEPAGE || theBook->GetPageCount() == 1) {
			if (centerPage && centerPage->Ok()) {
				xScroll = centerPage->GetWidth();
				yScroll = centerPage->GetHeight();
			}
		} else {
			if (rightPage && rightPage->Ok()) {
				xScroll = rightPage->GetWidth();
				yScroll = rightPage->GetHeight();
			}
			if (leftPage && leftPage->Ok()) {
				xScroll = (leftPage->GetWidth() > xScroll) ? leftPage->GetWidth() : xScroll;
				yScroll = (leftPage->GetHeight() > yScroll) ? leftPage->GetHeight() : yScroll;
			}
			xScroll *= 2;
		}
		cParent->menuView->FindItem(ID_RotateLeft)->Enable(false);
		cParent->menuView->FindItem(ID_RotateRight)->Enable(false);
		cParent->menuView->FindItem(ID_Rotate)->Enable(true);
		cParent->menuRotate->FindItemByPosition(theBook->Orientations[theBook->Current])->Check();
		cParent->toolBarNav->EnableTool(ID_CCWL, false);
		cParent->toolBarNav->EnableTool(ID_CWL, false);
		cParent->toolBarNav->EnableTool(ID_CCW, true);
		cParent->toolBarNav->EnableTool(ID_CW, true);
		
		cParent->labelLeft->SetLabel(wxT(""));
		cParent->labelRight->SetLabel(wxString::Format(wxT("%d of %d"), theBook->Current + 1, theBook->GetPageCount()));
	} else {
		cParent->menuView->FindItem(ID_Rotate)->Enable(false);

		if (rightPage && (rightOk = rightPage->Ok()))
		{
			xScroll = rightPage->GetWidth();
			yScroll = rightPage->GetHeight();

			cParent->menuView->FindItem(ID_RotateRight)->Enable(true);
			cParent->menuRotateRight->FindItemByPosition(theBook->Orientations[rightNum])->Check();
			cParent->toolBarNav->EnableTool(ID_CCW, true);
			cParent->toolBarNav->EnableTool(ID_CW, true);

			cParent->labelRight->SetLabel(wxString::Format(wxT("%d of %d"), rightNum + 1, theBook->GetPageCount()));
		}
		else
		{
			cParent->menuView->FindItem(ID_RotateRight)->Enable(false);
			cParent->toolBarNav->EnableTool(ID_CCW, false);
			cParent->toolBarNav->EnableTool(ID_CW, false);
			cParent->labelRight->SetLabel(wxT(""));
		}
	
		if (leftPage && (leftOk = leftPage->Ok()))
		{
			xScroll = (leftPage->GetWidth() > xScroll) ? leftPage->GetWidth() : xScroll;
			//xScroll += leftPage->GetWidth();
			yScroll = (leftPage->GetHeight() > yScroll) ? leftPage->GetHeight() : yScroll;

			cParent->menuView->FindItem(ID_RotateLeft)->Enable(true);
			cParent->menuRotateLeft->FindItemByPosition(theBook->Orientations[leftNum])->Check();
			cParent->toolBarNav->EnableTool(ID_CCWL, true);
			cParent->toolBarNav->EnableTool(ID_CWL, true);

			cParent->labelLeft->SetLabel(wxString::Format(wxT("%d of %d"), leftNum + 1, theBook->GetPageCount()));
		}
		else
		{
			cParent->menuView->FindItem(ID_RotateLeft)->Enable(false);
			cParent->toolBarNav->EnableTool(ID_CCWL, false);
			cParent->toolBarNav->EnableTool(ID_CWL, false);

			cParent->labelLeft->SetLabel(wxT(""));
		}

		xScroll *= 2;
	}

	cParent->toolBarNav->Realize();

	GetSize(&xWindow, &yWindow);

	if (xScroll <= xWindow && yScroll <= yWindow) { // no scrollbars neccessary
		xScroll = xWindow;
		yScroll = yWindow;
	}
	else {
		if (xScroll < (xWindow - scrollBarThickness))
			xScroll = xWindow - scrollBarThickness;
		if (yScroll < (yWindow - scrollBarThickness))
			yScroll = yWindow - scrollBarThickness;
	}
	SetVirtualSize(xScroll, yScroll);
	wxInt32 xStep = 1, yStep = 1;
	// if the pages will fit, make sure the scroll bars don't show up by making
	// the scroll step == 1 pixel.  Otherwise, make the scroll step 10 so that
	// one can navigate quickly using the arrow keys.
	if (xScroll > xWindow - scrollBarThickness)
		xStep = 10;
	if (yScroll > yWindow - scrollBarThickness)
		yStep = 10;
	SetScrollbars(xStep, yStep, xScroll / xStep, yScroll / yStep, 0, 0, TRUE);
	Scroll((xScroll / (2 * xStep)) - (xWindow / (2 * xStep)), 0); // center horizontally
	Refresh();
}

void ComicalCanvas::FirstPage()
{
	if (theBook == NULL)
		return;

	setPage(0);
	clearBitmaps();

	if (mode == ONEPAGE || theBook->GetPageCount() == 1)
		centerPage = theBook->GetPage(0);
	else
	{
		if (theBook->IsPageLandscape(0))
		{
			leftNum = 0;
			rightNum = 0;
			leftPart = LEFT_HALF;
			rightPart = RIGHT_HALF;
			leftPage = theBook->GetPageLeftHalf(0);
			rightPage = theBook->GetPageRightHalf(0);
		}
		else
		{
			setPage(1);
			leftNum = 0;
			rightNum = 1;
			leftPart = FULL_PAGE;
			leftPage = theBook->GetPage(0);
			if (theBook->IsPageLandscape(1)) {
				rightPart = LEFT_HALF;
				rightPage = theBook->GetPageLeftHalf(1);
			}
			else {
				rightPart = FULL_PAGE;
				rightPage = theBook->GetPage(1);
			}
		}
	}
	createBitmaps();
}

void ComicalCanvas::LastPage()
{
	if (theBook == NULL)
		return;

	setPage(theBook->GetPageCount() - 1);
	clearBitmaps();

	if (mode == ONEPAGE || theBook->GetPageCount() == 1)
		centerPage = theBook->GetPage(theBook->Current);
	else if (theBook->IsPageLandscape(theBook->Current)) {
		leftNum = theBook->Current;
		rightNum = theBook->Current;
		leftPart = LEFT_HALF;
		rightPart = RIGHT_HALF;
		leftPage = theBook->GetPageLeftHalf(theBook->Current);
		rightPage = theBook->GetPageRightHalf(theBook->Current);
	} else {
		leftNum = theBook->Current - 1;
		rightNum = theBook->Current;
		rightPart = FULL_PAGE;
		rightPage = theBook->GetPage(rightNum);
		if (theBook->IsPageLandscape(leftNum)) {
			leftPart = RIGHT_HALF;
			leftPage = theBook->GetPageRightHalf(theBook->Current - 1);
		} else {
			leftPart = FULL_PAGE;
			leftPage = theBook->GetPage(theBook->Current - 1);
		}
	}
	createBitmaps();
}

void ComicalCanvas::GoToPage(wxUint32 pagenumber)
{
	if (theBook == NULL)
		return;
	if (pagenumber >= theBook->GetPageCount())
		throw new PageOutOfRangeException(pagenumber, theBook->GetPageCount());

	if (pagenumber == 0)
	{
		FirstPage();
		return;
	}
	if (pagenumber == theBook->GetPageCount() - 1)
	{
		LastPage();
		return;
	}

	setPage(pagenumber);
	clearBitmaps();

	if (mode == ONEPAGE)
		centerPage = theBook->GetPage(pagenumber);
	else
	{
		if (theBook->IsPageLandscape(pagenumber))
		{
			leftNum = pagenumber;
			rightNum = pagenumber;
			leftPart = LEFT_HALF;
			rightPart = RIGHT_HALF;
			leftPage = theBook->GetPageLeftHalf(pagenumber);
			rightPage = theBook->GetPageRightHalf(pagenumber);
		}
		else
		{
			leftNum = pagenumber - 1;
			rightNum = pagenumber;
			if (theBook->IsPageLandscape(pagenumber - 1))
			{
				leftPart = RIGHT_HALF;
				leftPage = theBook->GetPageRightHalf(pagenumber - 1);
			}
			else
			{
				leftPart = FULL_PAGE;
				leftPage = theBook->GetPage(pagenumber - 1);
			}
			rightPart = FULL_PAGE;
			rightPage = theBook->GetPage(pagenumber);
		}
	}
	createBitmaps();
}

void ComicalCanvas::resetView()
{
	clearBitmaps();

	if (mode == ONEPAGE)
		centerPage = theBook->GetPage(theBook->Current);
	else
	{
		if (leftPart == LEFT_HALF)
			leftPage = theBook->GetPageLeftHalf(leftNum);
		else if (leftPart == RIGHT_HALF)
			leftPage = theBook->GetPageRightHalf(leftNum);
		else // leftPart == FULL_PAGE
			leftPage = theBook->GetPage(leftNum);
			
		if (rightPart == LEFT_HALF)
			rightPage = theBook->GetPageLeftHalf(rightNum);
		else if (rightPart == RIGHT_HALF)
			rightPage = theBook->GetPageRightHalf(rightNum);
		else // rightPart == FULL_PAGE
			rightPage = theBook->GetPage(rightNum);
	}
	createBitmaps();
}

void ComicalCanvas::PrevPageTurn()
{
	if (theBook == NULL)
		return;
		
	if (theBook->Current <= 1) {
		FirstPage();
		return;
	}
	
	if (mode == ONEPAGE) {
		PrevPageSlide();
		return;
	}

	if (leftPart != FULL_PAGE) // this covers two different cases
		setPage(theBook->Current - 1);
	else if (theBook->Current == 2) {
		FirstPage();
		return;
	} else
		setPage(theBook->Current - 2);

	clearBitmaps();

	rightNum = theBook->Current;
	if (theBook->IsPageLandscape(rightNum)) {
		if (leftPart == RIGHT_HALF) { // i.e., if the old left page is the right half of the new current
			rightPart = LEFT_HALF;
			rightPage = theBook->GetPageLeftHalf(rightNum);
			leftNum = theBook->Current - 1;
			if (theBook->IsPageLandscape(leftNum)) {
				leftPart = RIGHT_HALF;
				leftPage = theBook->GetPageRightHalf(leftNum);
			} else {
				leftPart = FULL_PAGE;
				leftPage = theBook->GetPage(leftNum);
			}
		} else {
			leftPart = LEFT_HALF;
			leftNum = theBook->Current;
			leftPage = theBook->GetPageLeftHalf(leftNum);
			rightPart = RIGHT_HALF;
			rightPage = theBook->GetPageRightHalf(rightNum);
		}
	} else {
		rightPart = FULL_PAGE;
		rightPage = theBook->GetPage(rightNum);
		leftNum = rightNum - 1;
		if (theBook->IsPageLandscape(leftNum)) {
			leftPart = RIGHT_HALF;
			leftPage = theBook->GetPageRightHalf(leftNum);
		} else {
			leftPart = FULL_PAGE;
			leftPage = theBook->GetPage(leftNum);
		}
	}
	createBitmaps();
}

void ComicalCanvas::NextPageTurn()
{
	if (theBook == NULL)
		return;
	
	if (theBook->Current >= theBook->GetPageCount() - 1) {
		LastPage();
		return;
	} else if (mode == ONEPAGE) {
		NextPageSlide();
		return;
	} else if (rightPart == LEFT_HALF) { // right page is old left half of current
		setPage(rightNum + 1);
		clearBitmaps();
		leftPart = RIGHT_HALF;
		leftNum = rightNum;
		leftPage = theBook->GetPageRightHalf(leftNum);
		rightNum = theBook->Current;
		if (theBook->IsPageLandscape(rightNum)) {
			rightPart = LEFT_HALF;
			rightPage = theBook->GetPageLeftHalf(rightNum);
		} else {
			rightPart = FULL_PAGE;
			rightPage = theBook->GetPage(rightNum);
		}
	} else if (theBook->IsPageLandscape(rightNum + 1)) {
		setPage(rightNum + 1);
		clearBitmaps();
		leftNum = theBook->Current;
		leftPart = LEFT_HALF;
		leftPage = theBook->GetPageLeftHalf(leftNum);
		rightNum = theBook->Current;
		rightPart = RIGHT_HALF;
		rightPage = theBook->GetPageRightHalf(rightNum);
	} else if (theBook->Current == theBook->GetPageCount() - 2) {
		LastPage();
		return;
	} else {
		setPage(rightNum + 2);
		clearBitmaps();
		leftNum = theBook->Current - 1;
		leftPart = FULL_PAGE;
		leftPage = theBook->GetPage(leftNum);
		rightNum = theBook->Current;
		if (theBook->IsPageLandscape(rightNum)) {
			rightPart = LEFT_HALF;
			rightPage = theBook->GetPageLeftHalf(rightNum);
		} else {
			rightPart = FULL_PAGE;
			rightPage = theBook->GetPage(rightNum);
		}
	}
	
	createBitmaps();
}

void ComicalCanvas::PrevPageSlide()
{
	if (theBook == NULL)
		return;
	if (theBook->Current <= 0)
		return;
	if (theBook->Current == 1)
	{
		FirstPage();
		return;
	}
	if (mode == ONEPAGE) {
		GoToPage(theBook->Current - 1);
		return;
	}

	if (leftNum != rightNum)
		setPage(theBook->Current - 1);

	clearBitmaps();

	rightNum = theBook->Current;
	if (leftPart == RIGHT_HALF) {
		leftNum = theBook->Current;
		leftPart = LEFT_HALF;
		leftPage = theBook->GetPageLeftHalf(leftNum);
		rightPart = RIGHT_HALF;
		rightPage =theBook->GetPageRightHalf(rightNum);
	} else {
		if (leftPart == LEFT_HALF) {
			rightPart = LEFT_HALF;
			rightPage = theBook->GetPageLeftHalf(rightNum);
		} else {
			rightPart = FULL_PAGE;
			rightPage = theBook->GetPage(rightNum);
		}
		leftNum = theBook->Current - 1;
		if (theBook->IsPageLandscape(leftNum)) {
			leftPart = RIGHT_HALF;
			leftPage = theBook->GetPageRightHalf(leftNum);
		} else {
			leftPart = FULL_PAGE;
			leftPage = theBook->GetPage(leftNum);
		}
	}
	createBitmaps();
}

void ComicalCanvas::NextPageSlide()
{
	if (theBook == NULL)
		return;
	if (theBook->Current >= theBook->GetPageCount() - 1)
		return;
	if (theBook->Current == theBook->GetPageCount() - 2 && rightPart != LEFT_HALF) {
		LastPage();
		return;
	}
	if (mode == ONEPAGE) {
		GoToPage(theBook->Current + 1);
		return;
	}

	if (rightPart != LEFT_HALF)
		setPage(rightNum + 1);

	clearBitmaps();

	rightNum = theBook->Current;
	if (rightPart == LEFT_HALF) {
		leftNum = rightNum;
		leftPart = LEFT_HALF;
		leftPage = theBook->GetPageLeftHalf(leftNum);
		rightPart = RIGHT_HALF;
		rightPage = theBook->GetPageRightHalf(rightNum);
	} else {
		leftNum = rightNum - 1;
		if (rightPart == RIGHT_HALF) {
			leftPart = RIGHT_HALF;
			leftPage = theBook->GetPageRightHalf(leftNum);
		} else {
			leftPart = FULL_PAGE;
			leftPage = theBook->GetPage(leftNum);
		}
		if (theBook->IsPageLandscape(rightNum)) {
			rightPart = LEFT_HALF;
			rightPage = theBook->GetPageLeftHalf(rightNum);
		} else {
			rightPart = FULL_PAGE;
			rightPage = theBook->GetPage(rightNum);
		}
	}
	createBitmaps();
}

void ComicalCanvas::Zoom(COMICAL_ZOOM newZoom)
{
	zoom = newZoom;
	if (zoom == FITH)
		EnableScrolling(false, true); // Horizontal fit, no horizontal scrolling
	else if (zoom == FITV)
		EnableScrolling(true, false); // Vertical fit, no vertical scrolling
	else if (zoom == FIT)
		EnableScrolling(false, false); // Fit, no scrolling
	else
		EnableScrolling(true, true);	
	if (theBook) {
		SetParams();
	}
}

void ComicalCanvas::Filter(FREE_IMAGE_FILTER newFilter)
{
	filter = newFilter;
	if (theBook) {
		SetParams();
	}
}

void ComicalCanvas::Mode(COMICAL_MODE newMode)
{
	if (theBook) {
		if (mode == ONEPAGE && newMode == TWOPAGE) {
			mode = newMode;
			if (theBook->Current == 0) {
		 		SetParams();
				FirstPage();
				return;
			} else { // theBook->Current >= 1
				leftNum = theBook->Current - 1;
				rightNum = theBook->Current;
		 		SetParams();
			}
		} else if (mode == TWOPAGE && newMode == ONEPAGE) {
			mode = newMode;
			SetParams();
		} // else nothing has changed !
	} else
		mode = newMode;
}

void ComicalCanvas::SetParams()
{
	wxSize canvasSize = GetSize();
	if (theBook->SetParams(mode, filter, zoom, canvasSize.x, canvasSize.y, scrollBarThickness) && theBook->IsRunning()) // if the parameters are actually different
		resetView();
}

void ComicalCanvas::Rotate(bool clockwise)
{
	COMICAL_ROTATE direction = theBook->Orientations[theBook->Current];
	if (clockwise) {
		switch (direction) {
		case NORTH:
			theBook->RotatePage(theBook->Current, EAST);
			break;
		case EAST:
			theBook->RotatePage(theBook->Current, SOUTH);
			break;
		case SOUTH:
			theBook->RotatePage(theBook->Current, WEST);
			break;
		case WEST:
			theBook->RotatePage(theBook->Current, NORTH);
			break;
		}
	} else {
		switch (direction) {
		case NORTH:
			theBook->RotatePage(theBook->Current, WEST);
			break;
		case EAST:
			theBook->RotatePage(theBook->Current, NORTH);
			break;
		case SOUTH:
			theBook->RotatePage(theBook->Current, EAST);
			break;
		case WEST:
			theBook->RotatePage(theBook->Current, SOUTH);
			break;
		}
	}
	GoToPage(theBook->Current);
}

void ComicalCanvas::Rotate(COMICAL_ROTATE direction)
{
	if (theBook) {
		theBook->RotatePage(theBook->Current, direction);
		GoToPage(theBook->Current);
	}
}

void ComicalCanvas::RotateLeft(bool clockwise)
{
	if(theBook->Current > 0)
	{
		COMICAL_ROTATE direction = theBook->Orientations[leftNum];
		if (clockwise) {
			switch (direction) {
			case NORTH:
				theBook->RotatePage(leftNum, EAST);
				break;
			case EAST:
				theBook->RotatePage(leftNum, SOUTH);
				break;
			case SOUTH:
				theBook->RotatePage(leftNum, WEST);
				break;
			case WEST:
				theBook->RotatePage(leftNum, NORTH);
				break;
			}
		} else {
			switch (direction) {
			case NORTH:
				theBook->RotatePage(leftNum, WEST);
				break;
			case EAST:
				theBook->RotatePage(leftNum, NORTH);
				break;
			case SOUTH:
				theBook->RotatePage(leftNum, EAST);
				break;
			case WEST:
				theBook->RotatePage(leftNum, SOUTH);
				break;
			}
		}
		GoToPage(leftNum);
	}
}

void ComicalCanvas::RotateLeft(COMICAL_ROTATE direction)
{
	if (theBook) {
		theBook->RotatePage(leftNum, direction);
		GoToPage(leftNum);
	}
}

void ComicalCanvas::OnPaint(wxPaintEvent &WXUNUSED(event))
{
	wxInt32 xCanvas, yCanvas;

	wxPaintDC dc(this);
	PrepareDC(dc);

	GetVirtualSize(&xCanvas, &yCanvas);
	
	if (centerPage && centerPage->Ok()) {
		dc.DrawBitmap(*centerPage, xCanvas/2 - centerPage->GetWidth()/2, 0, false);
	} else {
		if (leftPage && leftPage->Ok())
			dc.DrawBitmap(*leftPage, xCanvas/2 - leftPage->GetWidth(), 0, false);
		if (rightPage && rightPage->Ok())
			dc.DrawBitmap(*rightPage, xCanvas/2, 0, false);
	}

	SetFocus(); // This is so we can grab keydown events

}

void ComicalCanvas::OnKeyDown(wxKeyEvent& event)
{
	switch(event.GetKeyCode()) {

	case WXK_PRIOR:
		PrevPageTurn();
		break;

	case WXK_NEXT:
		NextPageTurn();
		break;

	case WXK_BACK:
		PrevPageSlide();
		break;

	case WXK_SPACE:
		NextPageSlide();
		break;

	case WXK_HOME:
		FirstPage();
		break;

	case WXK_END:
		LastPage();
		break;

	default:
		event.Skip();
	}
}

void ComicalCanvas::OnSize(wxSizeEvent& event)
{
	ComicalFrame *cParent = (ComicalFrame *) parent;
	if (cParent->toolBarNav != NULL && cParent->labelLeft != NULL && cParent->labelRight != NULL) {
		wxSize canvasSize = GetSize();
		wxSize clientSize = GetClientSize();
		wxSize toolBarSize = cParent->toolBarNav->GetSize();
		wxInt32 tbxPos = (clientSize.x - toolBarSize.x) / 2;
		cParent->toolBarNav->SetSize(tbxPos, canvasSize.y, toolBarSize.x, -1);
		cParent->labelLeft->SetSize(tbxPos - 70, canvasSize.y + 6, 50, toolBarSize.y);
		cParent->labelRight->SetSize(tbxPos + toolBarSize.x + 20, canvasSize.y + 6, 50, toolBarSize.y);		
	}
	if (theBook) {
		SetParams();
	}
}

void ComicalCanvas::setPage(wxInt32 pagenumber)
{
	if (theBook) {
		theBook->Current = pagenumber;
	}
}
