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
	File:		SourceInfo.cpp

	Contains:	Implements object defined in .h file.
					

*/

#include "SourceInfo.h"
#include "SocketUtils.h"
#include "SDPSourceInfo.h"

Bool16	SourceInfo::IsReflectable()
{
	if (fStreamArray == NULL)
		return false;
	if (fNumStreams == 0)
		return false;
		
	//each stream's info must meet certain criteria
	for (UInt32 x = 0; x < fNumStreams; x++)
	{
		if (fStreamArray[x].fIsTCP)
			continue;
			
		if ((!this->IsReflectableIPAddr(fStreamArray[x].fDestIPAddr)) ||
			(fStreamArray[x].fTimeToLive == 0))
			return false;
	}
	return true;
}

Bool16	SourceInfo::IsReflectableIPAddr(UInt32 inIPAddr)
{
	if (SocketUtils::IsMulticastIPAddr(inIPAddr) || SocketUtils::IsLocalIPAddr(inIPAddr))
		return true;
	return false;
}

Bool16	SourceInfo::HasTCPStreams()
{	
	//each stream's info must meet certain criteria
	for (UInt32 x = 0; x < fNumStreams; x++)
	{
		if (fStreamArray[x].fIsTCP)
			return true;
	}
	return false;
}

Bool16	SourceInfo::HasIncomingBroacast()
{	
	//each stream's info must meet certain criteria
	for (UInt32 x = 0; x < fNumStreams; x++)
	{
		if (fStreamArray[x].fSetupToReceive)
			return true;
	}
	return false;
}
SourceInfo::StreamInfo*	SourceInfo::GetStreamInfo(UInt32 inIndex)
{
	Assert(inIndex < fNumStreams);
	if (fStreamArray == NULL)
		return NULL;
	if (inIndex < fNumStreams)
		return &fStreamArray[inIndex];
	else
		return NULL;
}

SourceInfo::StreamInfo*	SourceInfo::GetStreamInfoByTrackID(UInt32 inTrackID)
{
	if (fStreamArray == NULL)
		return NULL;
	for (UInt32 x = 0; x < fNumStreams; x++)
	{
		if (fStreamArray[x].fTrackID == inTrackID)
			return &fStreamArray[x];
	}
	return NULL;
}

SourceInfo::OutputInfo*	SourceInfo::GetOutputInfo(UInt32 inIndex)
{
	Assert(inIndex < fNumOutputs);
	if (fOutputArray == NULL)
		return NULL;
	if (inIndex < fNumOutputs)
		return &fOutputArray[inIndex];
	else
		return NULL;
}

UInt32 SourceInfo::GetNumNewOutputs()
{
	UInt32 theNumNewOutputs = 0;
	for (UInt32 x = 0; x < fNumOutputs; x++)
	{
		if (!fOutputArray[x].fAlreadySetup)
			theNumNewOutputs++;
	}
	return theNumNewOutputs;
}

Bool16	SourceInfo::SetActiveNTPTimes(UInt32 startTimeNTP,UInt32 endTimeNTP)
{ 	// right now only handles earliest start and latest end time.

	//printf("SourceInfo::SetActiveNTPTimes start=%lu end=%lu\n",startTimeNTP,endTimeNTP);
	Bool16 accepted = false;
	if (startTimeNTP <= endTimeNTP || endTimeNTP == 0) do 
	{
		
		if (startTimeNTP != 0 && !IsValidNTPSecs(startTimeNTP)) break; // not valid NTP time
		if (endTimeNTP != 0 && !IsValidNTPSecs(endTimeNTP)) break; // not valid NTP time
		
		time_t startTimeUnixSecs = 0; 
		time_t endTimeUnixSecs  = 0; 
		if (startTimeNTP != 0) 
			startTimeUnixSecs = NTPSecs_to_UnixSecs(startTimeNTP);
		if (endTimeNTP != 0) 
			endTimeUnixSecs =NTPSecs_to_UnixSecs(endTimeNTP);
		
		if (!fTimeSet)
		{	fTimeSet = true;
			fStartTimeUnixSecs = startTimeUnixSecs;
			fEndTimeUnixSecs = endTimeUnixSecs;
		}
		else
		{	
			if (startTimeUnixSecs < fStartTimeUnixSecs || 0 == startTimeUnixSecs )
				fStartTimeUnixSecs = startTimeUnixSecs;
				
			if (endTimeUnixSecs > fEndTimeUnixSecs || 0 == endTimeUnixSecs)
				fEndTimeUnixSecs = endTimeUnixSecs;
		}			
		
		//printf("SourceInfo::SetActiveNTPTimes fStartTimeUnixSecs=%lu fEndTimeUnixSecs=%lu\n",fStartTimeUnixSecs,fEndTimeUnixSecs);
		//printf("SourceInfo::SetActiveNTPTimes\n");
		//printf("SourceInfo::SetActiveNTPTimes start time = %s",ctime(&fStartTimeUnixSecs) );
		//printf("SourceInfo::SetActiveNTPTimes end time = %s",ctime(&fEndTimeUnixSecs) );

		accepted = true;
	}  while(0);
		
	return accepted;
}

Bool16	SourceInfo::IsActiveTime(time_t unixTimeSecs)
{ 
	// order of tests are important here
	// we do it this way because of the special case time value of 0 for end time
	// start - 0 = unbounded 
	// 0 - 0 = permanent
	
	if (unixTimeSecs < 0) //check valid value
		return false;
		
	if (IsPermanentSource()) //check for all times accepted
		return true;
	
	if (unixTimeSecs < fStartTimeUnixSecs)
		return false; //too early

	if (fEndTimeUnixSecs == 0)	
		return true;// accept any time after start

	if (unixTimeSecs > fEndTimeUnixSecs)
		return false; // too late

	return true; // ok start <= time <= end

}


time_t	SourceInfo::GetDurationSecs() 
{ 	 
	
	if (fEndTimeUnixSecs == 0) // unbounded time
		return 0;
	
	time_t timeNow = OS::UnixTime_Secs();
	if (fEndTimeUnixSecs <= timeNow) // the active time has past or duration is 0 so return the minimum duration
		return 1; 
			
	if (fStartTimeUnixSecs == 0) // relative duration = from "now" to end time
		return fEndTimeUnixSecs - timeNow;
	
	return fEndTimeUnixSecs - fStartTimeUnixSecs; // this must be an duration because of test for endtime above

}

SourceInfo::StreamInfo::StreamInfo(const StreamInfo& copy)
: 	fSrcIPAddr(copy.fSrcIPAddr),
	fDestIPAddr(copy.fDestIPAddr),
	fPort(copy.fPort),
	fTimeToLive(copy.fTimeToLive),
	fPayloadType(copy.fPayloadType),
	fPayloadName(copy.fPayloadName),// Note that we don't copy data here
	fTrackID(copy.fTrackID),
	fBufferDelay(copy.fBufferDelay),
	fIsTCP(copy.fIsTCP),
	fSetupToReceive(copy.fSetupToReceive)
{}

SourceInfo::OutputInfo::OutputInfo(const OutputInfo& copy)
: 	fDestAddr(copy.fDestAddr),
	fLocalAddr(copy.fLocalAddr),
	fTimeToLive(copy.fTimeToLive),
	fPortArray(copy.fPortArray),// Note that we don't copy data here
	fAlreadySetup(copy.fAlreadySetup)
{}

Bool16 SourceInfo::OutputInfo::Equal(const OutputInfo& info)
{
	if ((fDestAddr == info.fDestAddr) && (fLocalAddr == info.fLocalAddr) &&
		(fTimeToLive == info.fTimeToLive) && (fPortArray[0] == info.fPortArray[0]))
		return true;
	return false;
}


