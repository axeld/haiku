/*
 * Copyright 2011 Stephan Aßmus <superstippi@gmx.de>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include "Toolbar.h"

#include <Button.h>
#include <ControlLook.h>
#include <Message.h>
#include <SeparatorView.h>
#include <SpaceLayoutItem.h>


namespace BPrivate {


class LockableButton: public BButton {
public:
			LockableButton(const char* name, const char* label,
				BMessage* message);

	void	MouseDown(BPoint point);
};


LockableButton::LockableButton(const char* name, const char* label,
	BMessage* message)
	:
	BButton(name, label, message)
{
}


void
LockableButton::MouseDown(BPoint point)
{
	if ((modifiers() & B_SHIFT_KEY) != 0 || Value() == B_CONTROL_ON)
		SetBehavior(B_TOGGLE_BEHAVIOR);
	else
		SetBehavior(B_BUTTON_BEHAVIOR);

	Message()->SetInt32("behavior", Behavior());
	BButton::MouseDown(point);
}


BToolbar::BToolbar(BRect frame, orientation ont)
	:
	BGroupView(ont),
	fOrientation(ont)
{
	float inset = ceilf(be_control_look->DefaultItemSpacing() / 2);
	GroupLayout()->SetInsets(inset, 0, inset, 0);
	GroupLayout()->SetSpacing(1);

	SetFlags(Flags() | B_FRAME_EVENTS | B_PULSE_NEEDED);

	MoveTo(frame.LeftTop());
	ResizeTo(frame.Width(), frame.Height());
	SetResizingMode(B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP);
}


BToolbar::~BToolbar()
{
}


void
BToolbar::Hide()
{
	BView::Hide();
	// TODO: This could be fixed in BView instead. Looking from the 
	// BButtons, they are not hidden though, only their parent is...
	_HideToolTips();
}


void
BToolbar::AddAction(uint32 command, BHandler* target, const BBitmap* icon,
	const char* toolTipText, bool lockable)
{
	AddAction(new BMessage(command), target, icon, toolTipText, lockable);
}


void
BToolbar::AddAction(BMessage* message, BHandler* target,
	const BBitmap* icon, const char* toolTipText, bool lockable)
{

	BButton* button;
	if (lockable)
		button = new LockableButton(NULL, NULL, message);
	else
		button = new BButton(NULL, NULL, message);
	button->SetIcon(icon);
	button->SetFlat(true);
	if (toolTipText != NULL)
		button->SetToolTip(toolTipText);
	_AddView(button);
	button->SetTarget(target);
}


void
BToolbar::AddSeparator()
{
	orientation ont = (fOrientation == B_HORIZONTAL) ?
		B_VERTICAL : B_HORIZONTAL;
	_AddView(new BSeparatorView(ont, B_PLAIN_BORDER));
}


void
BToolbar::AddGlue()
{
	GroupLayout()->AddItem(BSpaceLayoutItem::CreateGlue());
}


void
BToolbar::SetActionEnabled(uint32 command, bool enabled)
{
	if (BButton* button = _FindButton(command))
		button->SetEnabled(enabled);
}


void
BToolbar::SetActionPressed(uint32 command, bool pressed)
{
	if (BButton* button = _FindButton(command))
		button->SetValue(pressed);
}


void
BToolbar::SetActionVisible(uint32 command, bool visible)
{
	BButton* button = _FindButton(command);
	if (button == NULL)
		return;
	for (int32 i = 0; BLayoutItem* item = GroupLayout()->ItemAt(i); i++) {
		if (item->View() != button)
			continue;
		item->SetVisible(visible);
		break;
	}
}


void
BToolbar::Pulse()
{
	// TODO: Perhaps this could/should be addressed in BView instead.
	if (IsHidden())
		_HideToolTips();
}


void
BToolbar::FrameResized(float width, float height)
{
	// TODO: There seems to be a bug in app_server which does not
	// correctly trigger invalidation of views which are shown, when
	// the resulting dirty area is somehow already part of an update region.
	Invalidate();
}


void
BToolbar::_AddView(BView* view)
{
	GroupLayout()->AddView(view);
}


BButton*
BToolbar::_FindButton(uint32 command) const
{
	for (int32 i = 0; BView* view = ChildAt(i); i++) {
		BButton* button = dynamic_cast<BButton*>(view);
		if (button == NULL)
			continue;
		BMessage* message = button->Message();
		if (message == NULL)
			continue;
		if (message->what == command) {
			return button;
			// Assumes there is only one button with this message...
			break;
		}
	}
	return NULL;
}


void
BToolbar::_HideToolTips() const
{
	for (int32 i = 0; BView* view = ChildAt(i); i++)
		view->HideToolTip();
}


} // namespace BPrivate
