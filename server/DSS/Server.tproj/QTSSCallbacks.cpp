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
	File:		QTSSCallbacks.cpp

	Contains:	Implements QTSS Callback functions.
					
	
*/

#include "QTSSCallbacks.h"
#include "QTSSDictionary.h"
#include "QTSSStream.h"
#include "OSMemory.h"
#include "RTSPRequestInterface.h"
#include "RTPSession.h"
#include "OS.h"
#include "EventContext.h"
#include "Socket.h"
#include "QTSSFile.h"
#include "QTSSSocket.h"
#include "QTSServerInterface.h"
#include "QTSSDataConverter.h"

#include <errno.h>

#pragma mark __MEMORY_ROUTINES__

void* 	QTSSCallbacks::QTSS_New(FourCharCode /*inMemoryIdentifier*/, UInt32 inSize)
{
	//
	// This callback is now deprecated because the server no longer uses FourCharCodes
	// for memory debugging. For clients that still use it, the default, non-debugging
	// version of New is used.
	
	//return OSMemory::New(inSize, inMemoryIdentifier, false);
	return OSMemory::New(inSize); 
}

void	QTSSCallbacks::QTSS_Delete(void* inMemory)
{
	OSMemory::Delete(inMemory);
}

void	QTSSCallbacks::QTSS_Milliseconds(SInt64* outMilliseconds)
{
	if (outMilliseconds != NULL)
		*outMilliseconds = OS::Milliseconds();
}

void	QTSSCallbacks::QTSS_ConvertToUnixTime(SInt64 *inQTSS_MilliSecondsPtr, time_t* outSecondsPtr)
{
	if ( (NULL != outSecondsPtr) && (NULL != inQTSS_MilliSecondsPtr) )
		*outSecondsPtr = OS::TimeMilli_To_UnixTimeSecs(*inQTSS_MilliSecondsPtr);
}


#pragma mark __STARTUP_ROUTINES__

QTSS_Error 	QTSSCallbacks::QTSS_AddRole(QTSS_Role inRole)
{
	QTSS_ModuleState* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != NULL)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();
		
	// Roles can only be added before modules have had their Initialize role invoked.
	if ((theState == NULL) ||  (theState->curRole != QTSS_Register_Role))
		return QTSS_OutOfState;
		
	return theState->curModule->AddRole(inRole);
}


#pragma mark __DICTIONARY_ROUTINES__

QTSS_Error	QTSSCallbacks::QTSS_AddAttribute(QTSS_ObjectType inType, const char* inName, void* inUnused)
{
	//
	// This call is deprecated, make the new call with sensible default arguments
	return QTSSCallbacks::QTSS_AddStaticAttribute(inType, inName, inUnused, qtssAttrDataTypeUnknown);
}

QTSS_Error	QTSSCallbacks::QTSS_AddStaticAttribute(QTSS_ObjectType inObjectType, const char* inAttrName, void* inUnused, QTSS_AttrDataType inAttrDataType)
{
	Assert(inUnused == NULL);
	QTSS_ModuleState* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != NULL)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();

	// Static attributes can only be added before modules have had their Initialize role invoked.
	if ((theState == NULL) || (theState->curRole != QTSS_Register_Role))
		return QTSS_OutOfState;

	UInt32 theDictionaryIndex = QTSSDictionaryMap::GetMapIndex(inObjectType);
	if (theDictionaryIndex == QTSSDictionaryMap::kIllegalDictionary)
		return QTSS_BadArgument;
		
	QTSSDictionaryMap* theMap = QTSSDictionaryMap::GetMap(theDictionaryIndex);
	return theMap->AddAttribute(inAttrName, NULL, inAttrDataType, qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe);
}

QTSS_Error	QTSSCallbacks::QTSS_AddInstanceAttribute(QTSS_Object inObject, const char* inAttrName, void* inUnused, QTSS_AttrDataType inAttrDataType)
{
	Assert(inUnused == NULL);
	if ((inObject == NULL) || (inAttrName == NULL))
		return QTSS_BadArgument;
	
	return ((QTSSDictionary*)inObject)->AddInstanceAttribute(inAttrName, NULL, inAttrDataType, qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe);
}

QTSS_Error QTSSCallbacks::QTSS_RemoveInstanceAttribute(QTSS_Object inObject, QTSS_AttributeID inID)
{
	if (inObject == NULL)
		return QTSS_BadArgument;
	
	return ((QTSSDictionary*)inObject)->RemoveInstanceAttribute(inID);
}


QTSS_Error	QTSSCallbacks::QTSS_IDForAttr(QTSS_ObjectType inType, const char* inName, QTSS_AttributeID* outID)
{
	if (outID == NULL)
		return QTSS_BadArgument;
		
	UInt32 theDictionaryIndex = QTSSDictionaryMap::GetMapIndex(inType);
	if (theDictionaryIndex == QTSSDictionaryMap::kIllegalDictionary)
		return QTSS_BadArgument;
	
	return QTSSDictionaryMap::GetMap(theDictionaryIndex)->GetAttrID(inName, outID);
}

QTSS_Error QTSSCallbacks::QTSS_GetAttrInfoByIndex(QTSS_Object inObject, UInt32 inIndex, QTSS_Object* outAttrInfoObject)
{
	if (inObject == NULL)
		return QTSS_BadArgument;
	
	return ((QTSSDictionary*)inObject)->GetAttrInfoByIndex(inIndex, (QTSSAttrInfoDict**)outAttrInfoObject);
}

QTSS_Error QTSSCallbacks::QTSS_GetAttrInfoByID(QTSS_Object inObject, QTSS_AttributeID inAttrID, QTSS_Object* outAttrInfoObject)
{
	if (inObject == NULL)
		return QTSS_BadArgument;
	
	return ((QTSSDictionary*)inObject)->GetAttrInfoByID(inAttrID, (QTSSAttrInfoDict**)outAttrInfoObject);
}

QTSS_Error QTSSCallbacks::QTSS_GetAttrInfoByName(QTSS_Object inObject, const char* inAttrName, QTSS_Object* outAttrInfoObject)
{
	if (inObject == NULL)
		return QTSS_BadArgument;
	
	return ((QTSSDictionary*)inObject)->GetAttrInfoByName(inAttrName, (QTSSAttrInfoDict**)outAttrInfoObject);
}


QTSS_Error 	QTSSCallbacks::QTSS_GetValuePtr (QTSS_Object inDictionary, QTSS_AttributeID inID, UInt32 inIndex, void** outBuffer, UInt32* outLen)
{
	if ((inDictionary == NULL) || (outBuffer == NULL) || (outLen == NULL))
		return QTSS_BadArgument;
	return ((QTSSDictionary*)inDictionary)->GetValuePtr(inID, inIndex, outBuffer, outLen);
}


QTSS_Error 	QTSSCallbacks::QTSS_GetValue (QTSS_Object inDictionary, QTSS_AttributeID inID, UInt32 inIndex, void* ioBuffer, UInt32* ioLen)
{
	if (inDictionary == NULL)
		return QTSS_BadArgument;
	return ((QTSSDictionary*)inDictionary)->GetValue(inID, inIndex, ioBuffer, ioLen);
}

QTSS_Error 	QTSSCallbacks::QTSS_GetValueAsString (QTSS_Object inDictionary, QTSS_AttributeID inID, UInt32 inIndex, char** outString)
{
	if (inDictionary == NULL)
		return QTSS_BadArgument;
	return ((QTSSDictionary*)inDictionary)->GetValueAsString(inID, inIndex, outString);
}

QTSS_Error	QTSSCallbacks::QTSS_TypeToTypeString(const QTSS_AttrDataType inType, char** outTypeString)
{
	if (outTypeString == NULL)
		return QTSS_BadArgument;
	
	*outTypeString = QTSSDataConverter::TypeToTypeString(inType);
	return QTSS_NoErr;
}

QTSS_Error	QTSSCallbacks::QTSS_TypeStringToType(const char* inTypeString, QTSS_AttrDataType* outType)
{
	if ((inTypeString == NULL) || (outType == NULL))
		return QTSS_BadArgument;
		
	*outType = QTSSDataConverter::TypeStringToType(inTypeString);
	return QTSS_NoErr;
}

QTSS_Error	QTSSCallbacks::QTSS_StringToValue(const char* inValueAsString, const QTSS_AttrDataType inType, void* ioBuffer, UInt32* ioBufSize)
{
	return 	QTSSDataConverter::StringToValue(inValueAsString,inType,ioBuffer,ioBufSize);
}

QTSS_Error	QTSSCallbacks::QTSS_ValueToString(const void* inValue, const UInt32 inValueLen, const QTSS_AttrDataType inType, char** outString)
{
	if ((inValue == NULL) || (outString == NULL))
		return QTSS_BadArgument;
		
	 *outString = QTSSDataConverter::ValueToString(inValue,inValueLen,inType);
	 return QTSS_NoErr;
}

QTSS_Error 	QTSSCallbacks::QTSS_SetValue (QTSS_Object inDictionary, QTSS_AttributeID inID, UInt32 inIndex, const void* inBuffer,  UInt32 inLen)
{
	if ((inDictionary == NULL) || ((inBuffer == NULL) && (inLen > 0)))
		return QTSS_BadArgument;
	return ((QTSSDictionary*)inDictionary)->SetValue(inID, inIndex, inBuffer, inLen);
}

QTSS_Error 	QTSSCallbacks::QTSS_GetNumValues (QTSS_Object inObject, QTSS_AttributeID inID, UInt32* outNumValues)
{
	if ((inObject == NULL) || (outNumValues == NULL))
		return QTSS_BadArgument;
		
	*outNumValues = ((QTSSDictionary*)inObject)->GetNumValues(inID);
	return QTSS_NoErr;
}

QTSS_Error QTSSCallbacks::QTSS_GetNumAttributes(QTSS_Object inObject,  UInt32* outNumValues)
{
		
	if (outNumValues == NULL)
		return QTSS_BadArgument;

	if (inObject == NULL)
		return QTSS_BadArgument;

	OSMutexLocker locker(((QTSSDictionary*)inObject)->GetMutex());

	QTSSDictionaryMap* theMap = NULL;
	*outNumValues = 0;

	// Get the Static Attribute count
	theMap = ((QTSSDictionary*)inObject)->GetDictionaryMap();
	if (theMap != NULL)
		*outNumValues += theMap->GetNumNonRemovedAttrs();
	// Get the Instance Attribute count
	theMap = ((QTSSDictionary*)inObject)->GetInstanceDictMap();
	if (theMap != NULL)
		*outNumValues += theMap->GetNumNonRemovedAttrs();

	return QTSS_NoErr;
}

QTSS_Error 	QTSSCallbacks::QTSS_RemoveValue (QTSS_Object inObject, QTSS_AttributeID inID, UInt32 inIndex)
{
	if (inObject == NULL)
		return QTSS_BadArgument;
		
	return ((QTSSDictionary*)inObject)->RemoveValue(inID, inIndex);
}


#pragma mark __STREAM_ROUTINES__

QTSS_Error 	QTSSCallbacks::QTSS_Write(QTSS_StreamRef inStream, void* inBuffer, UInt32 inLen, UInt32* outLenWritten, UInt32 inFlags)
{
	if (inStream == NULL)
		return QTSS_BadArgument;
	QTSS_Error theErr = ((QTSSStream*)inStream)->Write(inBuffer, inLen, outLenWritten, inFlags);
	
	// Server internally propogates POSIX errorcodes such as EAGAIN and ENOTCONN up to this
	// level. The API guarentees that no POSIX errors get returned, so we have QTSS_Errors
	// to replace them. So we have to replace them here.
	if (theErr == EAGAIN)
		return QTSS_WouldBlock;
	else if (theErr > 0)
		return QTSS_NotConnected;
	else
		return theErr;
}

QTSS_Error 	QTSSCallbacks::QTSS_WriteV(QTSS_StreamRef inStream, iovec* inVec, UInt32 inNumVectors, UInt32 inTotalLength, UInt32* outLenWritten)
{
	if (inStream == NULL)
		return QTSS_BadArgument;
	QTSS_Error theErr = ((QTSSStream*)inStream)->WriteV(inVec, inNumVectors, inTotalLength, outLenWritten);

	// Server internally propogates POSIX errorcodes such as EAGAIN and ENOTCONN up to this
	// level. The API guarentees that no POSIX errors get returned, so we have QTSS_Errors
	// to replace them. So we have to replace them here.
	if (theErr == EAGAIN)
		return QTSS_WouldBlock;
	else if (theErr > 0)
		return QTSS_NotConnected;
	else
		return theErr;
}

QTSS_Error 	QTSSCallbacks::QTSS_Flush(QTSS_StreamRef inStream)
{
	if (inStream == NULL)
		return QTSS_BadArgument;
	QTSS_Error theErr = ((QTSSStream*)inStream)->Flush();

	// Server internally propogates POSIX errorcodes such as EAGAIN and ENOTCONN up to this
	// level. The API guarentees that no POSIX errors get returned, so we have QTSS_Errors
	// to replace them. So we have to replace them here.
	if (theErr == EAGAIN)
		return QTSS_WouldBlock;
	else if (theErr > 0)
		return QTSS_NotConnected;
	else
		return theErr;
}

QTSS_Error	QTSSCallbacks::QTSS_Read(QTSS_StreamRef inStream, void* ioBuffer, UInt32 inBufLen, UInt32* outLengthRead)
{
	if ((inStream == NULL) || (ioBuffer == NULL))
		return QTSS_BadArgument;
	QTSS_Error theErr = ((QTSSStream*)inStream)->Read(ioBuffer, inBufLen, outLengthRead);

	// Server internally propogates POSIX errorcodes such as EAGAIN and ENOTCONN up to this
	// level. The API guarentees that no POSIX errors get returned, so we have QTSS_Errors
	// to replace them. So we have to replace them here.
	if (theErr == EAGAIN)
		return QTSS_WouldBlock;
	else if (theErr > 0)
		return QTSS_NotConnected;
	else
		return theErr;
}

QTSS_Error	QTSSCallbacks::QTSS_Seek(QTSS_StreamRef inStream, UInt64 inNewPosition)
{
	if (inStream == NULL)
		return QTSS_BadArgument;
	return ((QTSSStream*)inStream)->Seek(inNewPosition);
}


QTSS_Error	QTSSCallbacks::QTSS_Advise(QTSS_StreamRef inStream, UInt64 inPosition, UInt32 inAdviseSize)
{
	if (inStream == NULL)
		return QTSS_BadArgument;
	return ((QTSSStream*)inStream)->Advise(inPosition, inAdviseSize);
}


#pragma mark __FILE_SYSTEM_ROUTINES__

QTSS_Error	QTSSCallbacks::QTSS_OpenFileObject(char* inPath, QTSS_OpenFileFlags inFlags, QTSS_Object* outFileObject)
{
	if ((inPath == NULL) || (outFileObject == NULL))
		return QTSS_BadArgument;
	
	//
	// Create a new file object
	QTSSFile* theNewFile = NEW QTSSFile();
	QTSS_Error theErr = theNewFile->Open(inPath, inFlags);

	if (theErr != QTSS_NoErr)
		delete theNewFile; // No module wanted to open the file.
	else
		*outFileObject = theNewFile;
	
	return theErr;
}

QTSS_Error	QTSSCallbacks::QTSS_CloseFileObject(QTSS_Object inFileObject)
{
	if (inFileObject == NULL)
		return QTSS_BadArgument;
		
	QTSSFile* theFile = (QTSSFile*)inFileObject;

	theFile->Close();
	delete theFile;
	return QTSS_NoErr;
}

#pragma mark __SOCKET_ROUTINES__

QTSS_Error	QTSSCallbacks::QTSS_CreateStreamFromSocket(int inFileDesc, QTSS_StreamRef* outStream)
{
	if (outStream == NULL)
		return QTSS_BadArgument;
	
	if (inFileDesc < 0)
		return QTSS_BadArgument;
		
	//
	// Create a new socket object
	*outStream = (QTSS_StreamRef)NEW QTSSSocket(inFileDesc);
	return QTSS_NoErr;
}

QTSS_Error	QTSSCallbacks::QTSS_DestroySocketStream(QTSS_StreamRef inStream)
{
	if (inStream == NULL)
		return QTSS_BadArgument;
	
	//
	// Note that the QTSSSocket destructor will call close on its file descriptor.
	// Calling module should not also close the file descriptor! (This is noted in the API)
	QTSSSocket* theSocket = (QTSSSocket*)inStream;
	delete theSocket;
	return QTSS_NoErr;
}

#pragma mark __SERVICE_ROUTINES__

QTSS_Error	QTSSCallbacks::QTSS_AddService(const char* inServiceName, QTSS_ServiceFunctionPtr inFunctionPtr)
{
	QTSS_ModuleState* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != NULL)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();

	// This may happen if this callback is occurring on module-created thread
	if (theState == NULL)
		return QTSS_OutOfState;
		
	// Roles can only be added before modules have had their Initialize role invoked.
	if (theState->curRole != QTSS_Register_Role)
		return QTSS_OutOfState;

	return QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kServiceDictIndex)->
				AddAttribute(inServiceName, (QTSS_AttrFunctionPtr)inFunctionPtr, qtssAttrDataTypeUnknown, qtssAttrModeRead);
}

QTSS_Error	QTSSCallbacks::QTSS_IDForService(const char* inTag, QTSS_ServiceID* outID)
{
	return QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kServiceDictIndex)->
				GetAttrID(inTag, outID);
}

QTSS_Error	QTSSCallbacks::QTSS_DoService(QTSS_ServiceID inID, QTSS_ServiceFunctionArgsPtr inArgs)
{
	// Make sure that the service ID is in fact valid
	
	QTSSDictionaryMap* theMap = QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kServiceDictIndex);
	SInt32 theIndex = theMap->ConvertAttrIDToArrayIndex(inID);
	if (theIndex < 0)
		return QTSS_IllegalService;
	
	// Get the service function	
	QTSS_ServiceFunctionPtr theFunction = (QTSS_ServiceFunctionPtr)theMap->GetAttrFunction(theIndex);

	// Invoke it, return the result.	
	return (theFunction)(inArgs);
}

#pragma mark __RTSP_ROUTINES__

QTSS_Error QTSSCallbacks::QTSS_SendRTSPHeaders(QTSS_RTSPRequestObject inRef)
{
	if (inRef == NULL)
		return QTSS_BadArgument;
		
	((RTSPRequestInterface*)inRef)->SendHeader();
	return QTSS_NoErr;
}

QTSS_Error QTSSCallbacks::QTSS_AppendRTSPHeader(QTSS_RTSPRequestObject inRef,
														QTSS_RTSPHeader inHeader,
														char* inValue,
														UInt32 inValueLen)
{
	if ((inRef == NULL) || (inValue == NULL))
		return QTSS_BadArgument;
	if (inHeader >= qtssNumHeaders)
		return QTSS_BadArgument;
	
	StrPtrLen theValue(inValue, inValueLen);
	((RTSPRequestInterface*)inRef)->AppendHeader(inHeader, &theValue);
	return QTSS_NoErr;
}

QTSS_Error QTSSCallbacks::QTSS_SendStandardRTSPResponse(QTSS_RTSPRequestObject inRTSPRequest,
															QTSS_Object inRTPInfo,
															UInt32 inFlags)
{
	if ((inRTSPRequest == NULL) || (inRTPInfo == NULL))
		return QTSS_BadArgument;
		
	switch (((RTSPRequestInterface*)inRTSPRequest)->GetMethod())
	{
		case qtssDescribeMethod:
			((RTPSession*)inRTPInfo)->SendDescribeResponse((RTSPRequestInterface*)inRTSPRequest);
			return QTSS_NoErr;
		case qtssSetupMethod:
		{
			// Because QTSS_SendStandardRTSPResponse supports sending a proper 304 Not Modified on a SETUP,
			// but a caller typically won't be adding a stream for a 304 response, we have the policy of
			// making the caller pass in the QTSS_ClientSessionObject instead. That means we need to do
			// different things here depending...
			if (((RTSPRequestInterface*)inRTSPRequest)->GetStatus() == qtssRedirectNotModified)
				(void)((RTPSession*)inRTPInfo)->DoSessionSetupResponse((RTSPRequestInterface*)inRTSPRequest);
			else
				((RTPStream*)inRTPInfo)->SendSetupResponse((RTSPRequestInterface*)inRTSPRequest);
			return QTSS_NoErr;
		}
		case qtssPlayMethod:
			((RTPSession*)inRTPInfo)->SendPlayResponse((RTSPRequestInterface*)inRTSPRequest, inFlags);
			return QTSS_NoErr;
		case qtssPauseMethod:
			((RTPSession*)inRTPInfo)->SendPauseResponse((RTSPRequestInterface*)inRTSPRequest);
			return QTSS_NoErr;
		case qtssTeardownMethod:
			((RTPSession*)inRTPInfo)->SendTeardownResponse((RTSPRequestInterface*)inRTSPRequest);
			return QTSS_NoErr;
		case qtssAnnounceMethod:
			((RTPSession*)inRTPInfo)->SendAnnounceResponse((RTSPRequestInterface*)inRTSPRequest);
			return QTSS_NoErr;
	}
	return QTSS_BadArgument;
}


#pragma mark __RTP_ROUTINES__


QTSS_Error	QTSSCallbacks::QTSS_AddRTPStream(QTSS_ClientSessionObject inClientSession, QTSS_RTSPRequestObject inRTSPRequest, QTSS_RTPStreamObject* outStream, QTSS_AddStreamFlags inFlags)
{
	if ((inClientSession == NULL) || (inRTSPRequest == NULL) ||(outStream == NULL))
		return QTSS_BadArgument;
	return ((RTPSession*)inClientSession)->AddStream((RTSPRequestInterface*)inRTSPRequest, (RTPStream**)outStream, inFlags);
}

QTSS_Error	QTSSCallbacks::QTSS_Play(QTSS_ClientSessionObject inClientSession, QTSS_RTSPRequestObject inRTSPRequest, QTSS_PlayFlags inPlayFlags)
{
	if (inClientSession == NULL)
		return QTSS_BadArgument;
	return ((RTPSession*)inClientSession)->Play((RTSPRequestInterface*)inRTSPRequest, inPlayFlags);
}

QTSS_Error	QTSSCallbacks::QTSS_Pause(QTSS_ClientSessionObject inClientSession)
{
	if (inClientSession == NULL)
		return QTSS_BadArgument;
	((RTPSession*)inClientSession)->Pause();
	return QTSS_NoErr;
}

QTSS_Error	QTSSCallbacks::QTSS_Teardown(QTSS_ClientSessionObject inClientSession)
{
	if (inClientSession == NULL)
		return QTSS_BadArgument;
	
	((RTPSession*)inClientSession)->Teardown();
	return QTSS_NoErr;
}

QTSS_Error	QTSSCallbacks::QTSS_RefreshTimeOut(QTSS_ClientSessionObject inClientSession)
{
	if (inClientSession == NULL)
		return QTSS_BadArgument;
	
	((RTPSession*)inClientSession)->RefreshTimeouts();
	return QTSS_NoErr;
}


#pragma mark __ASYNC_ROUTINES__

QTSS_Error	QTSSCallbacks::QTSS_RequestEvent(QTSS_StreamRef inStream, QTSS_EventType inEventMask)
{
	// First thing to do is to alter the thread's module state to reflect the fact
	// that an event is outstanding.
	QTSS_ModuleState* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != NULL)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();
		
	if (theState == NULL)
		return QTSS_RequestFailed;
		
	if (theState->curTask == NULL)
		return QTSS_OutOfState;
		
	theState->eventRequested = true;
	
	// Now, tell this stream to be ready for the requested event
	QTSSStream* theStream = (QTSSStream*)inStream;
	theStream->SetTask(theState->curTask);
	theStream->RequestEvent(inEventMask);
	return QTSS_NoErr;
}

QTSS_Error	QTSSCallbacks::QTSS_SignalStream(QTSS_StreamRef inStream)
{
	if (inStream == NULL)
		return QTSS_BadArgument;
	
	QTSSStream* theStream = (QTSSStream*)inStream;
	if (theStream->GetTask() != NULL)
		theStream->GetTask()->Signal(Task::kReadEvent);
	return QTSS_NoErr;
}

QTSS_Error	QTSSCallbacks::QTSS_SetIdleTimer(SInt64 inMsecToWait)
{
	QTSS_ModuleState* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != NULL)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();
		
	// This may happen if this callback is occurring on module-created thread
	if (theState == NULL)
		return QTSS_RequestFailed;
		
	if (theState->curTask == NULL)
		return QTSS_OutOfState;
		
	theState->eventRequested = true;
	theState->idleTime = inMsecToWait;
	return QTSS_NoErr;
}

QTSS_Error	QTSSCallbacks::QTSS_RequestLockedCallback()
{
	QTSS_ModuleState* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != NULL)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();
		
	// This may happen if this callback is occurring on module-created thread
	if (theState == NULL)
		return QTSS_RequestFailed;
		
	if (theState->curTask == NULL)
		return QTSS_OutOfState;
		
	theState->globalLockRequested = true; //x

	return QTSS_NoErr;
}

Bool16		QTSSCallbacks::QTSS_IsGlobalLocked()
{
	QTSS_ModuleState* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != NULL)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();
		
	// This may happen if this callback is occurring on module-created thread
	if (theState == NULL)
		return false;
		
	if (theState->curTask == NULL)
		return false;

	return theState->isGlobalLocked;
}

QTSS_Error	QTSSCallbacks::QTSS_UnlockGlobalLock()
{
	QTSS_ModuleState* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != NULL)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();
		
	// This may happen if this callback is occurring on module-created thread
	if (theState == NULL)
		return QTSS_RequestFailed;
		
	if (theState->curTask == NULL)
		return QTSS_OutOfState;
		
	((Task *)OSThread::GetCurrent())->GlobalUnlock();
	
	theState->globalLockRequested = false; 
	theState->isGlobalLocked = false; 


	return QTSS_NoErr;
}

#pragma mark __AUTH_ROUTINES__

QTSS_Error	QTSSCallbacks::QTSS_Authenticate(const char* inAuthUserName, const char* inAuthResourceLocalPath, const char* inAuthMoviesDir, QTSS_ActionFlags inAuthRequestAction, QTSS_AuthScheme inAuthScheme, QTSS_RTSPRequestObject ioAuthRequestObject)
{
	if((inAuthUserName == NULL) || (inAuthResourceLocalPath == NULL) || (inAuthMoviesDir == NULL) || (ioAuthRequestObject == NULL)) 
		return QTSS_BadArgument;
	if(inAuthRequestAction == qtssActionFlagsNoFlags)
		return QTSS_BadArgument;
	if(inAuthScheme == qtssAuthNone)
		return QTSS_BadArgument;

	// First create a RTSPRequestInterface object 
	// There is no session attached to it, so just pass in NULL for the RTSPSession
	RTSPRequestInterface *request =  (RTSPRequestInterface *) ioAuthRequestObject;
	// Set all the attributes required by the authentication module, using the input values
	(void) request->SetValue(qtssRTSPReqUserName, 0,  inAuthUserName , ::strlen(inAuthUserName), QTSSDictionary::kDontObeyReadOnly);
	(void) request->SetValue(qtssRTSPReqLocalPath, 0,  inAuthResourceLocalPath , ::strlen(inAuthResourceLocalPath), QTSSDictionary::kDontObeyReadOnly);
	(void) request->SetValue(qtssRTSPReqRootDir, 0,  inAuthMoviesDir , ::strlen(inAuthMoviesDir), QTSSDictionary::kNoFlags);
	(void) request->SetValue(qtssRTSPReqAction, 0,  (const void *)&inAuthRequestAction , sizeof(QTSS_ActionFlags), QTSSDictionary::kNoFlags);
	(void) request->SetValue(qtssRTSPReqAuthScheme, 0,  (const void *)&inAuthScheme , sizeof(QTSS_AuthScheme), QTSSDictionary::kDontObeyReadOnly);
	QTSSUserProfile *profile = request->GetUserProfile();
	(void) profile->SetValue(qtssUserName, 0, inAuthUserName, ::strlen(inAuthUserName), QTSSDictionary::kDontObeyReadOnly);
	
	
	// Because this is a role being executed from inside a callback, we need to
	// make sure that QTSS_RequestEvent will not work.
	Task* curTask = NULL;
	QTSS_ModuleState* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != NULL)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();
		
	if (theState != NULL)
		curTask = theState->curTask;
	
	// Setup the authentication param block
	QTSS_RoleParams theAuthenticationParams;
	theAuthenticationParams.rtspAthnParams.inRTSPRequest = request;
			
	QTSS_Error theErr = QTSS_RequestFailed;
	
	// There is only one module that is registered for the RTSP Authentication role
	if (QTSServerInterface::GetNumModulesInRole(QTSSModule::kRTSPAthnRole) > 0)
	{
		theErr = QTSServerInterface::GetModule(QTSSModule::kRTSPAthnRole, 0)->CallDispatch(QTSS_RTSPAuthenticate_Role, &theAuthenticationParams);
	}
	
	// Reset the curTask to what it was before this role started
	if (theState != NULL)
		theState->curTask = curTask;

	return theErr;
}

QTSS_Error	QTSSCallbacks::QTSS_Authorize(QTSS_RTSPRequestObject inAuthRequestObject, char* outAuthRealm, Bool16* outAuthUserAllowed)
{
	RTSPRequestInterface* request = (RTSPRequestInterface *) inAuthRequestObject;
	if (request == NULL)
		return QTSS_BadArgument;
			
	// Because this is a role being executed from inside a callback, we need to
	// make sure that QTSS_RequestEvent will not work.
	Task* curTask = NULL;
	QTSS_ModuleState* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != NULL)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();
		
	if (theState != NULL)
		curTask = theState->curTask;
		
	QTSS_RoleParams theParams;
	theParams.rtspRequestParams.inRTSPSession = NULL;
	theParams.rtspRequestParams.inRTSPRequest = request;
	theParams.rtspRequestParams.inClientSession = NULL;

	QTSS_Error theErr = QTSS_RequestFailed;
	UInt32 x = 0;
	*outAuthUserAllowed = true;
	
	// Call all the modules that are registered for the RTSP Authorize Role	
	for ( ; x < QTSServerInterface::GetNumModulesInRole(QTSSModule::kRTSPAuthRole); x++)
	{
		theErr = QTSServerInterface::GetModule(QTSSModule::kRTSPAuthRole, x)->CallDispatch(QTSS_RTSPAuthorize_Role, &theParams);
	
		// If any module sets allowed to false, exit the loop as authentication has been denied
		*outAuthUserAllowed = request->GetAllowed();	
		if(false == *outAuthUserAllowed)
			break;
	}
	
	// outAuthRealm is set to the realm that is given by the module that has denied authentication
	StrPtrLen* realm = request->GetValue(qtssRTSPReqURLRealm);
	outAuthRealm = realm->GetAsCString();
	
	return theErr;
}

