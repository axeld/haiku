/*
 * Copyright 2013, Axel Dörfler, axeld@pinc-software.de.
 * This file may be used under the terms of the MIT License.
 */


#include <stdio.h>
#include <string.h>

#include <Application.h>
#include <Bitmap.h>
#include <Screen.h>
#include <TranslationUtils.h>


int
main(int argc, char** argv)
{
	if (argc < 2)
		return 1;

	BApplication app("application/x-vnd.Haiku-setbackground");
	BScreen screen;

	BBitmap* bitmap = BTranslationUtils::GetBitmap(argv[1]);
	if (bitmap != NULL) {
		status_t status = screen.SetDesktopBitmap(bitmap, 0);
		printf("Setting bitmap %g:%g: %s", bitmap->Bounds().Width(),
			bitmap->Bounds().Height(), strerror(status));
	}

	return 0;
}
