/*
 * Copyright 2011 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		John Scipione, jscipione@gmail.com
 */
#ifndef MODIFIER_KEYS_WINDOW_H
#define MODIFIER_KEYS_WINDOW_H


#include <Bitmap.h>
#include <Button.h>
#include <CheckBox.h>
#include <InterfaceDefs.h>
#include <MenuField.h>
#include <PopUpMenu.h>
#include <StringView.h>
#include <Window.h>


class ConflictView;

class ModifierKeysWindow : public BWindow {
public:
									ModifierKeysWindow();
	virtual							~ModifierKeysWindow();

	virtual	void					MessageReceived(BMessage* message);

protected:
			BMenuField*				_CreateShiftMenuField();
			BMenuField*				_CreateControlMenuField();
			BMenuField*				_CreateOptionMenuField();
			BMenuField*				_CreateCommandMenuField();

			void					_MarkMenuItems();
			const char*				_KeyToString(int32 key);
			uint32					_KeyToKeyCode(int32 key,
										bool right = false);
			int32					_LastKey();
			void					_ValidateDuplicateKeys();
			uint32					_DuplicateKeys();

private:
			BStringView*			fShiftStringView;
			BStringView*			fControlStringView;
			BStringView*			fOptionStringView;
			BStringView*			fCommandStringView;

			BPopUpMenu*				fShiftMenu;
			BPopUpMenu*				fControlMenu;
			BPopUpMenu*				fOptionMenu;
			BPopUpMenu*				fCommandMenu;

			ConflictView*			fShiftConflictView;
			ConflictView*			fControlConflictView;
			ConflictView*			fOptionConflictView;
			ConflictView*			fCommandConflictView;

			BButton*				fRevertButton;
			BButton*				fCancelButton;
			BButton*				fOkButton;

			key_map*				fCurrentMap;
			key_map*				fSavedMap;

			char*					fCurrentBuffer;
			char*					fSavedBuffer;
};


class ConflictView : public BView {
	public:
									ConflictView(const char* name);
									~ConflictView();
				BBitmap*			Icon();
				void				ShowIcon(bool show);

	protected:
		virtual void				Draw(BRect updateRect);
		void						_FillSavedIcon();

	private:
		BBitmap*					fIcon;
		BBitmap*					fSavedIcon;
};


#endif	// MODIFIER_KEYS_WINDOW_H
