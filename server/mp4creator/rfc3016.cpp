/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is MPEG4IP.
 * 
 * The Initial Developer of the Original Code is Cisco Systems Inc.
 * Portions created by Cisco Systems Inc. are
 * Copyright (C) Cisco Systems Inc. 2000, 2001.  All Rights Reserved.
 * 
 * Contributor(s): 
 *		Dave Mackie		dmackie@cisco.com
 */

/* 
 * Notes:
 *  - file formatted with tabstops == 4 spaces 
 */

#include <mp4creator.h>

// over in mp4v.cpp
extern u_char Mp4vGetVopType(u_int8_t* pVopBuf, u_int32_t vopSize);

void Rfc3016Hinter(
	MP4FileHandle mp4File, MP4TrackId mediaTrackId, MP4TrackId hintTrackId)
{
	u_int8_t payloadNumber = 0;

	MP4SetHintTrackRtpPayload(mp4File, hintTrackId, 
		"MP4V-ES", &payloadNumber, 0);

	u_int32_t numSamples = MP4GetNumberOfTrackSamples(mp4File, mediaTrackId);
	u_int32_t maxSampleSize = MP4GetMaxSampleSize(mp4File, mediaTrackId);
	u_int8_t* pSampleBuffer = (u_int8_t*)malloc(maxSampleSize);
	ASSERT(pSampleBuffer);

	for (MP4SampleId sampleId = 1; sampleId <= numSamples; sampleId++) {
		u_int32_t sampleSize = maxSampleSize;
		MP4Timestamp startTime;
		MP4Duration duration;
		MP4Duration renderingOffset;
		bool isSyncSample;

		bool rc = MP4ReadSample(
			mp4File, mediaTrackId, sampleId, 
			&pSampleBuffer, &sampleSize, 
			&startTime, &duration, 
			&renderingOffset, &isSyncSample);

		if (!rc) {
			fprintf(stderr,
				"%s: failed to read sample %u from track %u\n",
				ProgName, sampleId, mediaTrackId);
			exit(EXIT_RFC3016_HINTER);
		}

		bool isBFrame = (Mp4vGetVopType(pSampleBuffer, sampleSize) == 'B');

		MP4AddRtpVideoHint(mp4File, hintTrackId, isBFrame, renderingOffset);

		u_int32_t offset = 0;
		u_int32_t remaining = sampleSize;

		while (remaining) {
			bool isLastPacket = false;
			u_int32_t length;

			if (remaining <= MaxPayloadSize) {
				length = remaining;
				isLastPacket = true;
			} else {
				length = MaxPayloadSize;
			}

			MP4AddRtpPacket(mp4File, hintTrackId, isLastPacket);
			
			MP4AddRtpSampleData(mp4File, hintTrackId, sampleId, 
				offset, length);

			offset += length;
			remaining -= length;
		}

		MP4WriteRtpHint(mp4File, hintTrackId, duration, isSyncSample);
	}
}

