/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2001 Apple Computer, Inc.  All Rights Reserved. The
 * contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License.  Please
 * obtain a copy of the License at http://www.apple.com/publicsource and
 * read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  Please
 * see the License for the specific language governing rights and
 * limitations under the License.
 *
 *
 * @APPLE_LICENSE_HEADER_END@
 *
 */
/*
	File:		FilePrefsSource.h

	Contains:	Implements the PrefsSource interface, getting the prefs from a file.

	Written by:	Chris LeCroy

	Copyright:	� 1998 by Apple Computer, Inc., all rights reserved.


	

*/

#ifndef __FILEPREFSSOURCE_H__
#define __FILEPREFSSOURCE_H__

#include "PrefsSource.h"
#include "OSHeaders.h"

class KeyValuePair; //only used in the implementation

class FilePrefsSource : public PrefsSource
{
	public:
	
		FilePrefsSource( Bool16 allowDuplicates = false );
		virtual ~FilePrefsSource(); 
	
		virtual int		GetValue(const char* inKey, char* ioValue);
		virtual int		GetValueByIndex(const char* inKey, UInt32 inIndex, char* ioValue);

		// Allows caller to iterate over all the values in the file.
		char*			GetValueAtIndex(UInt32 inIndex);
		char*			GetKeyAtIndex(UInt32 inIndex);
		UInt32			GetNumKeys() { return fNumKeys; }
		
        int InitFromConfigFile(const char* configFilePath);
        void WriteToConfigFile(const char* configFilePath);

        void SetValue(const char* inKey, const char* inValue);
        void DeleteValue(const char* inKey);

	private:
	
		static Bool16 FilePrefsConfigSetter( const char* paramName, const char* paramValue[], void* userData );
		
        KeyValuePair* 	FindValue(const char* inKey, char* ioValue, UInt32 index = 0);
        KeyValuePair* 	fKeyValueList;
        UInt32 			fNumKeys;
        Bool16 fAllowDuplicates;
};

#endif //__FILEPREFSSOURCE_H__
