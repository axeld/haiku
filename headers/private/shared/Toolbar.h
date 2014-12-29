/*
 * Copyright 2011-2014, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _TOOLBAR_H
#define _TOOLBAR_H

#include <GroupView.h>


class BButton;

namespace BPrivate {

class BToolbar : public BGroupView {
public:
								BToolbar(BRect frame,
									orientation ont = B_HORIZONTAL);
	virtual						~BToolbar();

	virtual	void				Hide();

			void				AddAction(uint32 command, BHandler* target,
									const BBitmap* icon,
									const char* toolTipText = NULL,
									bool lockable = false);
			void				AddAction(BMessage* message, BHandler* target,
									const BBitmap* icon,
									const char* toolTipText = NULL,
									bool lockable = false);
			void				AddSeparator();
			void				AddGlue();

			void				SetActionEnabled(uint32 command, bool enabled);
			void				SetActionPressed(uint32 command, bool pressed);
			void				SetActionVisible(uint32 command, bool visible);

private:
	virtual	void				Pulse();
	virtual	void				FrameResized(float width, float height);

			void				_AddView(BView* view);
			BButton*			_FindButton(uint32 command) const;
			void				_HideToolTips() const;
			
			orientation			fOrientation;
};

} // namespace BPrivate

using BPrivate::BToolbar;

#endif // _TOOLBAR_H
