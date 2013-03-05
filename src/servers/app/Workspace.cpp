/*
 * Copyright 2005-2013, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 */


#include "Workspace.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <Debug.h>

#include "Desktop.h"
#include "WorkspacePrivate.h"
#include "Window.h"


static rgb_color kDefaultColor = (rgb_color){ 51, 102, 152, 255 };


Workspace::Private::Private()
{
	_SetDefaults();
}


Workspace::Private::~Private()
{
	if (fBitmap != NULL)
		fBitmap->ReleaseReference();
}


void
Workspace::Private::SetDisplaysFromDesktop(Desktop* desktop)
{
}


void
Workspace::Private::SetColor(const rgb_color& color)
{
	fColor = color;
}


void
Workspace::Private::SetImage(const char* path, ServerBitmap* bitmap,
	uint32 options, const BPoint& offset)
{
	if (fBitmap != NULL)
		fBitmap->ReleaseReference();

	fImagePath = path;
	fBitmap = bitmap;
	fBitmapOptions = options;
	fBitmapOffset = offset;

	if (fBitmap != NULL)
		fBitmap->AcquireReference();
}


void
Workspace::Private::RestoreConfiguration(const BMessage& settings)
{
	rgb_color color;
	if (settings.FindInt32("color", (int32*)&color) == B_OK)
		fColor = color;

	fImagePath = settings.GetString("imagePath", NULL);
	fBitmapOptions = settings.GetInt32("bitmapOptions", 0);
	fBitmapOffset = settings.GetPoint("bitmapOffset", BPoint(0, 0));

	fStoredImagePath = fImagePath;
	fStoredBitmapOptions = fBitmapOptions;
	fStoredBitmapOffset = fBitmapOffset;

	fStoredScreenConfiguration.Restore(settings);
	fCurrentScreenConfiguration.Restore(settings);
}


/*!	\brief Store the workspace configuration in a message
*/
void
Workspace::Private::StoreConfiguration(BMessage& settings)
{
	fStoredScreenConfiguration.Store(settings);

	settings.SetInt32("color", *(int32*)&fColor);

	if (!fImagePath.IsEmpty()) {
		settings.SetString("imagePath", fImagePath);
		settings.SetInt32("bitmapOptions", BitmapOptions());
		settings.SetPoint("bitmapOffset", BitmapOffset());
	}

	fStoredImagePath = fImagePath;
	fStoredBitmapOptions = fBitmapOptions;
	fStoredBitmapOffset = fBitmapOffset;
}


void
Workspace::Private::_SetDefaults()
{
	fColor = kDefaultColor;
	fBitmap = NULL;
}


//	#pragma mark -


Workspace::Workspace(Desktop& desktop, int32 index, bool readOnly)
	:
	fWorkspace(desktop.WorkspaceAt(index)),
	fDesktop(desktop),
	fCurrentWorkspace(index == desktop.CurrentWorkspace())
{
	ASSERT(desktop.WindowLocker().IsWriteLocked()
		|| ( readOnly && desktop.WindowLocker().IsReadLocked()));
	RewindWindows();
}


Workspace::~Workspace()
{
}


const rgb_color&
Workspace::Color() const
{
	return fWorkspace.Color();
}


void
Workspace::SetColor(const rgb_color& color, bool makeDefault)
{
	if (color == Color() && (!makeDefault || color == fWorkspace.StoredColor()))
		return;

	fWorkspace.SetColor(color);
	fDesktop.RedrawBackground();
	if (makeDefault)
		fDesktop.StoreWorkspaceConfiguration(fWorkspace.Index());
}


const char*
Workspace::ImagePath() const
{
	return fWorkspace.ImagePath().String();
}

ServerBitmap*
Workspace::Bitmap() const
{
	return fWorkspace.Bitmap();
}


uint32
Workspace::BitmapOptions() const
{
	return fWorkspace.BitmapOptions();
}


const BPoint&
Workspace::BitmapOffset() const
{
	return fWorkspace.BitmapOffset();
}


void
Workspace::SetImage(const char* path, ServerBitmap* bitmap, uint32 options,
	const BPoint& offset, bool makeDefault)
{
	if ((fWorkspace.ImagePath() == path && bitmap == Bitmap()
			&& options == BitmapOptions() && offset == BitmapOffset())
		&& (!makeDefault || (fWorkspace.StoredImagePath() == path
			&& options == fWorkspace.StoredBitmapOptions()
			&& offset == fWorkspace.StoredBitmapOffset())))
		return;

	fWorkspace.SetImage(path, bitmap, options, offset);
	fDesktop.RedrawBackground();
	if (makeDefault)
		fDesktop.StoreWorkspaceConfiguration(fWorkspace.Index());
}


status_t
Workspace::GetNextWindow(Window*& _window, BPoint& _leftTop)
{
	if (fCurrent == NULL)
		fCurrent = fWorkspace.Windows().FirstWindow();
	else
		fCurrent = fCurrent->NextWindow(fWorkspace.Index());

	if (fCurrent == NULL)
		return B_ENTRY_NOT_FOUND;

	_window = fCurrent;

	if (fCurrentWorkspace)
		_leftTop = fCurrent->Frame().LeftTop();
	else
		_leftTop = fCurrent->Anchor(fWorkspace.Index()).position;

	return B_OK;
}


status_t
Workspace::GetPreviousWindow(Window*& _window, BPoint& _leftTop)
{
	if (fCurrent == NULL)
		fCurrent = fWorkspace.Windows().LastWindow();
	else
		fCurrent = fCurrent->PreviousWindow(fWorkspace.Index());

	if (fCurrent == NULL)
		return B_ENTRY_NOT_FOUND;

	_window = fCurrent;

	if (fCurrentWorkspace)
		_leftTop = fCurrent->Frame().LeftTop();
	else
		_leftTop = fCurrent->Anchor(fWorkspace.Index()).position;

	return B_OK;
}


void
Workspace::RewindWindows()
{
	fCurrent = NULL;
}

