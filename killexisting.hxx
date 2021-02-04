// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef KILLEXISTING_HXX
#define KILLEXISTING_HXX

#include <QtGlobal>

#ifdef Q_OS_LINUX
#define HAS_KILLEXISTING
#endif

#ifdef HAS_KILLEXISTING
void setupKillExisting();
#endif

#endif	// KILLEXISTING_HXX
