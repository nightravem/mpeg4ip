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
 * Copyright (C) Cisco Systems Inc. 2000-2005.  All Rights Reserved.
 * 
 * Contributor(s): 
 *		Dave Mackie		dmackie@cisco.com
 *              Bill May  wmay@cisco.com
 */

#include "mp4live.h"
#include "video_encoder.h"
#include "video_encoder_base.h"
#include "video_util_filter.h"

CVideoEncoder* VideoEncoderCreate(CVideoProfile *vp, 
				  CVideoEncoder *next, 
				  bool realTime)
{
  return VideoEncoderCreateBase(vp, next, realTime);
}

MediaType get_video_mp4_fileinfo (CVideoProfile *pConfig,
				  bool *createIod,
				  bool *isma_compliant,
				  uint8_t *videoProfile,
				  uint8_t **videoConfig,
				  uint32_t *videoConfigLen,
				  uint8_t *mp4_video_type)
{
  return get_video_mp4_fileinfo_base(pConfig, 
				     createIod, 
				     isma_compliant, 
				     videoProfile, 
				     videoConfig, 
				     videoConfigLen, 
				     mp4_video_type);
}

media_desc_t *create_video_sdp (CVideoProfile *pConfig,
				bool *createIod,
				bool *isma_compliant,
				uint8_t *videoProfile,
				uint8_t **videoConfig,
				uint32_t *videoConfigLen)
{
 return create_video_sdp_base(pConfig,
			      createIod,
			      isma_compliant,
			      videoProfile,
			      videoConfig,
			      videoConfigLen);
}


void create_mp4_video_hint_track (CVideoProfile *pConfig,
				  MP4FileHandle mp4file,
				  MP4TrackId trackId,
				  uint16_t mtu)
{
  return create_mp4_video_hint_track_base(pConfig, mp4file, trackId, mtu);
}

rtp_transmitter_f GetVideoRtpTransmitRoutine (CVideoProfile *pConfig,
					      MediaType *pType,
					      uint8_t *pPayload)
{
  return GetVideoRtpTransmitRoutineBase(pConfig, pType, pPayload);
}

// Video encoder initialization

CVideoEncoder::CVideoEncoder(CVideoProfile *vp,
			     CVideoEncoder *next,
			     bool realTime) : 
  CMediaCodec(vp, next, realTime)
{
  m_videoSrcYImage = NULL;
  m_videoDstYImage = NULL;
  m_videoYResizer = NULL;
  m_videoSrcUVImage = NULL;
  m_videoDstUVImage = NULL;
  m_videoUVResizer = NULL;
  m_videoDstPrevImage = NULL;
  m_videoDstPrevReconstructImage = NULL;
  m_videoSrcWidth = 0;
  m_videoSrcHeight = 0;
  m_videoSrcYStride = 0;
  m_preview = false;
};

int CVideoEncoder::ThreadMain(void) 
{
  CMsg* pMsg;
  bool stop = false;

  debug_message("video encoder %s start", Profile()->GetName());
  m_videoSrcFrameNumber = 0;
  //  debug_message("audio source frame is %d", m_audioSrcFrameNumber);
  //  m_audioSrcFrameNumber = 0;	// ensure audio is also at zero

  const char *videoFilter;
  videoFilter = Profile()->GetStringValue(CFG_VIDEO_FILTER);
  m_videoFilterInterlace = 
    (strncasecmp(videoFilter, 
		 VIDEO_FILTER_DEINTERLACE, 
		 strlen(VIDEO_FILTER_DEINTERLACE)) == 0);
  m_videoDstFrameRate = Profile()->GetFloatValue(CFG_VIDEO_FRAME_RATE);
  m_videoDstFrameDuration = 
    (Duration)(((float)TimestampTicks / m_videoDstFrameRate) + 0.5);
  m_videoDstFrameNumber = 0;
  m_videoDstWidth =
    Profile()->m_videoWidth;
  m_videoDstHeight =
    Profile()->m_videoHeight;
  m_videoDstAspectRatio = 
    (float)Profile()->m_videoWidth / (float)Profile()->m_videoHeight;
  m_videoDstYSize = m_videoDstWidth * m_videoDstHeight;
  m_videoDstUVSize = m_videoDstYSize / 4;
  m_videoDstYUVSize = (m_videoDstYSize * 3) / 2;

  Init();
  m_videoDstType = GetFrameType();

  m_videoWantKeyFrame = true;
  //m_videoEncodingDrift = 0;
  //m_videoEncodingMaxDrift = m_videoDstFrameDuration;
  m_videoSrcElapsedDuration = 0;
  m_videoDstElapsedDuration = 0;

  m_videoDstPrevImage = NULL;
  m_videoDstPrevReconstructImage = NULL;

  while (stop == false && SDL_SemWait(m_myMsgQueueSemaphore) == 0) {
    pMsg = m_myMsgQueue.get_message();
    if (pMsg != NULL) {
      switch (pMsg->get_value()) {
      case MSG_NODE_STOP_THREAD:
	debug_message("video %s stop received", 
		      Profile()->GetName());
	DoStopVideo();
	stop = true;
	break;
      case MSG_NODE_START:
	// DoStartTransmit();  Anything ?
	break;
      case MSG_NODE_STOP:
	DoStopVideo();
	break;
      case MSG_SINK_FRAME: {
	uint32_t dontcare;
	CMediaFrame *mf = (CMediaFrame*)pMsg->get_message(dontcare);
	if (m_stop_thread == false)
	  ProcessVideoYUVFrame(mf);
	if (mf->RemoveReference()) {
	  delete mf;
	}
	break;
      }
      }
      
      delete pMsg;
    } 
  }
  while ((pMsg = m_myMsgQueue.get_message()) != NULL) {
    if (pMsg->get_value() == MSG_SINK_FRAME) {
      uint32_t dontcare;
      CMediaFrame *mf = (CMediaFrame*)pMsg->get_message(dontcare);
      if (mf->RemoveReference()) {
	delete mf;
      }
    }
    delete pMsg;
  }
  debug_message("video encoder %s exit", Profile()->GetName());
  return 0;
}

static void c_ReleaseReconstruct(void *f)
{
  yuv_media_frame_t *yuv = (yuv_media_frame_t *)f;
  if (yuv->free_y) {
    CHECK_AND_FREE(yuv->y);
  } 
  free(yuv);
}

// Called from ProcessYUVVideoFrame when we get the first frame - 
// it will have the source information.  
void CVideoEncoder::SetVideoSrcSize(
				    u_int16_t srcWidth,
				    u_int16_t srcHeight,
				    u_int16_t srcStride,
				    bool matchAspectRatios)
{
  // N.B. InitVideo() must be called first

  m_videoSrcWidth = srcWidth;
  m_videoSrcHeight = srcHeight;
  m_videoSrcAspectRatio = (float)srcWidth / (float)srcHeight;
  m_videoMatchAspectRatios = matchAspectRatios;

  // N.B. SetVideoSrcSize() should be called once before 

  m_videoSrcYStride = srcStride;
  m_videoSrcUVStride = srcStride / 2;

  // these next three may change below
  m_videoSrcAdjustedHeight = m_videoSrcHeight;
  m_videoSrcYCrop = 0;
  m_videoSrcUVCrop = 0;

  // match aspect ratios
  if (m_videoMatchAspectRatios 
      && fabs(m_videoSrcAspectRatio - m_videoDstAspectRatio) > 0.01) {

    m_videoSrcAdjustedHeight =
      (u_int16_t)(m_videoSrcWidth / m_videoDstAspectRatio);
    if ((m_videoSrcAdjustedHeight % 16) != 0) {
      m_videoSrcAdjustedHeight += 16 - (m_videoSrcAdjustedHeight % 16);
    }

    if (m_videoSrcAspectRatio < m_videoDstAspectRatio) {
      // crop src
      m_videoSrcYCrop = m_videoSrcYStride * 
	((m_videoSrcHeight - m_videoSrcAdjustedHeight) / 2);
      m_videoSrcUVCrop = m_videoSrcYCrop / 4;
    }
  }

  m_videoSrcYSize = m_videoSrcYStride 
    * MAX(m_videoSrcHeight, m_videoSrcAdjustedHeight);
  m_videoSrcUVSize = m_videoSrcYSize / 4;
  m_videoSrcYUVSize = (m_videoSrcYSize * 3) / 2;

  // resizing

  DestroyVideoResizer();

  if (m_videoSrcWidth != m_videoDstWidth 
      || m_videoSrcAdjustedHeight != m_videoDstHeight) {

    m_videoSrcYImage = 
      scale_new_image(m_videoSrcWidth, 
		      m_videoSrcAdjustedHeight, 1);
    m_videoSrcYImage->span = m_videoSrcYStride;
    m_videoDstYImage = 
      scale_new_image(m_videoDstWidth, 
		      m_videoDstHeight, 1);
    m_videoYResizer = 
      scale_image_init(m_videoDstYImage, m_videoSrcYImage, 
		       Bell_filter, Bell_support);

    m_videoSrcUVImage = 
      scale_new_image(m_videoSrcWidth / 2, 
		      m_videoSrcAdjustedHeight / 2, 1);
    m_videoSrcUVImage->span = m_videoSrcUVStride;
    m_videoDstUVImage = 
      scale_new_image(m_videoDstWidth / 2, 
		      m_videoDstHeight / 2, 1);
    m_videoUVResizer = 
      scale_image_init(m_videoDstUVImage, m_videoSrcUVImage, 
		       Bell_filter, Bell_support);
  }
}

void CVideoEncoder::ProcessVideoYUVFrame(CMediaFrame *pFrame)
{
  yuv_media_frame_t *pYUV = (yuv_media_frame_t *)pFrame->GetData();
  const u_int8_t* pY = pYUV->y;
  const u_int8_t* pU = pYUV->u;
  const u_int8_t* pV = pYUV->v;
  u_int16_t yStride = pYUV->y_stride;
  u_int16_t uvStride = pYUV->uv_stride;
  Timestamp srcFrameTimestamp = pFrame->GetTimestamp();

  if (m_videoSrcFrameNumber == 0) {
    m_videoStartTimestamp = srcFrameTimestamp;
    SetVideoSrcSize(pYUV->w, pYUV->h, yStride, false);
  }

  // if we want to be able to handle different input sizes, we
  // can check pYUV->w, pYUV->h, ystride against the stored values
  m_videoSrcFrameNumber++;
  m_videoSrcElapsedDuration = srcFrameTimestamp - m_videoStartTimestamp;

#ifdef DEBUG_VIDEO_SYNC
  debug_message("vsrc# %d srcDuration="U64" dst# %d dstDuration "U64,
                m_videoSrcFrameNumber, m_videoSrcElapsedDuration,
                m_videoDstFrameNumber, m_videoDstElapsedDuration);
#endif

  // destination gets ahead of source
  // drop src frames as needed to match target frame rate
  if (m_videoSrcElapsedDuration + m_videoDstFrameDuration < m_videoDstElapsedDuration) {
#ifdef DEBUG_VIDEO_SYNC
    debug_message("video: dropping frame, SrcElapsedDuration="U64" DstElapsedDuration="U64,
                  m_videoSrcElapsedDuration, m_videoDstElapsedDuration);
#endif
    return;
  }

  Duration lag = m_videoSrcElapsedDuration - m_videoDstElapsedDuration;

  // source gets ahead of destination
  if (lag > 3 * m_videoDstFrameDuration) {
    debug_message("lag "D64" src "U64" dst "U64,
		  lag, m_videoSrcElapsedDuration, m_videoDstElapsedDuration);
    int j = (lag - (2 * m_videoDstFrameDuration)) / m_videoDstFrameDuration;
    m_videoDstFrameNumber += j;
    m_videoDstElapsedDuration = VideoDstFramesToDuration();
    debug_message("video: advancing dst by %d frames", j);
  }

  // Disabled since we are not taking into account audio drift anymore
  // and the new algorithm automatically factors in any drift due
  // to video encoding
  /*
    // add any external drift (i.e. audio encoding drift)
    //to our drift measurement
    m_videoEncodingDrift += m_otherTotalDrift - m_otherLastTotalDrift;
    m_otherLastTotalDrift = m_otherTotalDrift;

    // check if the video encoding drift exceeds the max limit
    if (m_videoEncodingDrift >= m_videoEncodingMaxDrift) {
      // we skip multiple destination frames to give audio
      // a better chance to keep up
      // on subsequent calls, we will return immediately until
      // m_videoSrcElapsedDuration catches up with m_videoDstElapsedDuration
      int framesToSkip = m_videoEncodingDrift / m_videoDstFrameDuration;
      m_videoEncodingDrift -= framesToSkip * m_videoDstFrameDuration;
      m_videoDstFrameNumber += framesToSkip;
      m_videoDstElapsedDuration = VideoDstFramesToDuration();

      debug_message("video: will skip %d frames due to encoding drift", framesToSkip);

      return;
    }
  */

  m_videoDstFrameNumber++;
  m_videoDstElapsedDuration = VideoDstFramesToDuration();

  //Timestamp encodingStartTimestamp = GetTimestamp();


  u_int8_t* mallocedYuvImage = NULL;

  // crop to desired aspect ratio (may be a no-op)
  const u_int8_t* yImage = pY + m_videoSrcYCrop;
  const u_int8_t* uImage = pU + m_videoSrcUVCrop;
  const u_int8_t* vImage = pV + m_videoSrcUVCrop;

  // resize image if necessary
  if (m_videoYResizer) {
    u_int8_t* resizedYUV = (u_int8_t*)Malloc(m_videoDstYUVSize);
		
    u_int8_t* resizedY = resizedYUV;
    u_int8_t* resizedU = resizedYUV + m_videoDstYSize;
    u_int8_t* resizedV = resizedYUV + m_videoDstYSize + m_videoDstUVSize;

    m_videoSrcYImage->data = (pixel_t *)yImage;
    m_videoDstYImage->data = resizedY;
    scale_image_process(m_videoYResizer);

    m_videoSrcUVImage->data = (pixel_t *)uImage;
    m_videoDstUVImage->data = resizedU;
    scale_image_process(m_videoUVResizer);

    m_videoSrcUVImage->data = (pixel_t *)vImage;
    m_videoDstUVImage->data = resizedV;
    scale_image_process(m_videoUVResizer);

    // done with the original source image
    if (mallocedYuvImage) free(mallocedYuvImage);

    // switch over to resized version
    mallocedYuvImage = resizedYUV;
    yImage = resizedY;
    uImage = resizedU;
    vImage = resizedV;
    yStride = m_videoDstWidth;
    uvStride = yStride / 2;
  }
  // this has to be rewritten to not mess with the original YUV, 
  // since it has to be done after the resizer.
  if (m_videoFilterInterlace) {
    if (mallocedYuvImage == NULL) {
      u_int8_t* YUV = (u_int8_t*)Malloc(m_videoDstYUVSize);
		
      u_int8_t* pY = YUV;
      u_int8_t* pU = YUV + m_videoDstYSize;
      u_int8_t* pV = YUV + m_videoDstYSize + m_videoDstUVSize;
      CopyYuv(yImage, uImage, vImage, yStride, uvStride, uvStride,
	      pY, pU, pV, m_videoDstWidth, m_videoDstWidth/2, m_videoDstWidth / 2,
	      m_videoDstWidth, m_videoDstHeight);
      mallocedYuvImage = YUV;
      yImage = pY;
      uImage = pU;
      vImage = pV;
      yStride = m_videoDstWidth;
      uvStride = yStride / 2;
      // need to copy
    }
    video_filter_interlace((uint8_t *)yImage, 
			   (uint8_t *)yImage + m_videoDstYSize, yStride);
  }
  // if we want encoded video frames
  // this checkr really doesnt need to be here
  bool rc = EncodeImage(
			yImage, uImage, vImage, 
			yStride, uvStride,
			m_videoWantKeyFrame |
			pYUV->force_iframe,
			m_videoDstElapsedDuration,
			srcFrameTimestamp);

  if (!rc) {
    debug_message("Can't encode image!");
    if (mallocedYuvImage) free(mallocedYuvImage);
    return;
  }

  m_videoWantKeyFrame = false;

  uint8_t *frame;
  uint32_t frame_len;
  bool got_image;
  Timestamp pts, dts;
  got_image = GetEncodedImage(&frame,
			      &frame_len,
			      &dts,
			      &pts);
  if (got_image) {
    //error_message("frame len %d time %llu", frame_len, dts);
    CMediaFrame* pFrame = new CMediaFrame(
					  GetFrameType(),
					  frame,
					  frame_len,
					  dts,
					  m_videoDstFrameDuration,
					  TimestampTicks,
					  pts);
    pFrame->SetMediaFreeFunction(GetMediaFreeFunction());
    ForwardFrame(pFrame);
  }

  // forward reconstructed video to sinks
  if (m_preview) {
    yuv_media_frame_t *mf = MALLOC_STRUCTURE(yuv_media_frame_t);
    uint8_t *alloced;
    mf->y_stride = m_videoDstWidth;
    mf->uv_stride = m_videoDstWidth / 2;
    mf->y =  alloced = (u_int8_t*)Malloc(m_videoDstYUVSize);
    mf->u = mf->y + m_videoDstYSize;
    mf->v = mf->u + m_videoDstUVSize;
    mf->w = m_videoDstWidth;
    mf->h = m_videoDstHeight;
    mf->free_y = true;
    if (GetReconstructedImage(alloced,
			      alloced + m_videoDstYSize,
			      alloced + m_videoDstYSize + m_videoDstUVSize)) {

      //debug_message("forwarding encoded yuv");
      CMediaFrame* pFrame = new CMediaFrame(YUVVIDEOFRAME,
					    mf,
					    0,
					    srcFrameTimestamp,
					    m_videoDstFrameDuration);
      pFrame->SetMediaFreeFunction(c_ReleaseReconstruct);
      ForwardFrame(pFrame);
    } else {
      CHECK_AND_FREE(mf->y);
      free(mf);
    }
  }

  if (mallocedYuvImage) free(mallocedYuvImage);
}

void CVideoEncoder::DoStopVideo()
{
  DestroyVideoResizer();

  StopEncoder();
  debug_message("Video encoding profile %s stats", GetProfileName());
  debug_message("Encoded frames: %u", m_videoDstFrameNumber);
		
}

void CVideoEncoder::DestroyVideoResizer()
{
  if (m_videoSrcYImage) {
    scale_free_image(m_videoSrcYImage);
    m_videoSrcYImage = NULL;
  }
  if (m_videoDstYImage) {
    scale_free_image(m_videoDstYImage);
    m_videoDstYImage = NULL;
  }
  if (m_videoYResizer) {
    scale_image_done(m_videoYResizer);
    m_videoYResizer = NULL;
  }
  if (m_videoSrcUVImage) {
    scale_free_image(m_videoSrcUVImage);
    m_videoSrcUVImage = NULL;
  }
  if (m_videoDstUVImage) {
    scale_free_image(m_videoDstUVImage);
    m_videoDstUVImage = NULL;
  }
  if (m_videoUVResizer) {
    scale_image_done(m_videoUVResizer);
    m_videoUVResizer = NULL;
  }
}
