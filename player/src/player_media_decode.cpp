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
 *              Bill May        wmay@cisco.com
 */
/*
 * player_media_decode.cpp
 * decode task thread for a CPlayerMedia
 */
#include "systems.h"
#include "player_session.h"
#include "player_media.h"
#include "player_sdp.h"
#include "player_util.h"
#include "media_utils.h"
#include "audio.h"
#include "video.h"
#include "rtp_bytestream.h"
#include "codec_plugin_private.h"
//#define DEBUG_DECODE 1
//#define DEBUG_DECODE_MSGS 1
//#define TIME_DECODE 1
/*
 * parse_decode_message - handle messages to the decode task
 */
void CPlayerMedia::parse_decode_message (int &thread_stop, int &decoding)
{
  CMsg *newmsg;

  if ((newmsg = m_decode_msg_queue.get_message()) != NULL) {
#ifdef DEBUG_DECODE_MSGS
    media_message(LOG_DEBUG, "decode thread message %d",newmsg->get_value());
#endif
    switch (newmsg->get_value()) {
    case MSG_STOP_THREAD:
      thread_stop = 1;
      break;
    case MSG_PAUSE_SESSION:
      decoding = 0;
      if (m_video_sync != NULL) {
	m_video_sync->flush_decode_buffers();
      }
      if (m_audio_sync != NULL) {
	m_audio_sync->flush_decode_buffers();
      }
      break;
    case MSG_START_DECODING:
      if (m_video_sync != NULL) {
	m_video_sync->flush_decode_buffers();
      }
      if (m_audio_sync != NULL) {
	m_audio_sync->flush_decode_buffers();
      }
      decoding = 1;
      break;
    }
    delete newmsg;
  }
}

/*
 * Main decode thread.
 */
int CPlayerMedia::decode_thread (void) 
{
  //  uint32_t msec_per_frame = 0;
  int ret = 0;
#ifdef TIME_DECODE
  int64_t avg = 0, diff;
  int64_t max = 0;
  int avg_cnt = 0;
#endif
  int thread_stop = 0, decoding = 0;
  uint64_t decode_skipped_frames = 0;
  while (thread_stop == 0) {
    // waiting here for decoding or thread stop
    ret = SDL_SemWait(m_decode_thread_sem);
#ifdef DEBUG_DECODE
    media_message(LOG_DEBUG, "%s Decode thread awake",
		  is_video() ? "video" : "audio");
#endif
    parse_decode_message(thread_stop, decoding);

    if (decoding == 1) {
      // We've been told to start decoding - if we don't have a codec, 
      // create one
      if (is_video())
	m_video_sync->set_wait_sem(m_decode_thread_sem);
      else
	m_audio_sync->set_wait_sem(m_decode_thread_sem);
      if (m_plugin == NULL) {
	if (is_video()) {
	  m_plugin = check_for_video_codec(NULL,
					   m_media_fmt,
					   -1,
					   -1,
					   m_user_data,
					   m_user_data_size);
	  if (m_plugin != NULL) {
	    m_plugin_data = (m_plugin->vc_create)(m_media_fmt,
						  m_video_info,
						  m_user_data,
						  m_user_data_size,
						  get_video_vft(),
						  m_video_sync);
	    if (m_plugin_data == NULL) {
	      m_plugin = NULL;
	    } else {
	      media_message(LOG_DEBUG, "Starting %s codec from decode thread",
			    m_plugin->c_name);
	    }
	  }
	} else {
	  m_plugin = check_for_audio_codec(NULL,
					   m_media_fmt,
					   -1, 
					   -1, 
					   m_user_data,
					   m_user_data_size);
	  if (m_plugin != NULL) {
	    m_plugin_data = (m_plugin->ac_create)(m_media_fmt,
						  m_audio_info,
						  m_user_data,
						  m_user_data_size,
						  get_audio_vft(),
						  m_audio_sync);
	    if (m_plugin_data == NULL) {
	      m_plugin = NULL;
	    } else {
	      media_message(LOG_DEBUG, "Starting %s codec from decode thread",
			    m_plugin->c_name);
	    }
	  }
	}
      }
      if (m_plugin != NULL) {
	m_plugin->c_do_pause(m_plugin_data);
      } else {
	while (thread_stop == 0 && decoding) {
	  SDL_Delay(100);
	  if (m_rtp_byte_stream) {
	    m_rtp_byte_stream->flush_rtp_packets();
	  }
	  parse_decode_message(thread_stop, decoding);
	}
      }
    }
    /*
     * this is our main decode loop
     */
#ifdef DEBUG_DECODE
    media_message(LOG_DEBUG, "%s Into decode loop",
		  is_video() ? "video" : "audio");
#endif
    while ((thread_stop == 0) && decoding) {
      parse_decode_message(thread_stop, decoding);
      if (thread_stop != 0)
	continue;
      if (decoding == 0) {
	m_plugin->c_do_pause(m_plugin_data);
	continue;
      }
      if (m_byte_stream->eof()) {
	media_message(LOG_INFO, "%s hit eof", m_is_video ? "video" : "audio");
	if (m_audio_sync) m_audio_sync->set_eof();
	if (m_video_sync) m_video_sync->set_eof();
	decoding = 0;
	continue;
      }
      if (m_byte_stream->have_no_data()) {
	// Indicate that we're waiting, and wait for a message from RTP
	// task.
	m_decode_thread_waiting = 1;
#ifdef DEBUG_DECODE
	media_message(LOG_INFO, "decode thread %s waiting", m_media_info->media);
#endif
	ret = SDL_SemWait(m_decode_thread_sem);
	m_decode_thread_waiting = 0;
	continue;
      }

      uint64_t ourtime;
      // Tell bytestream we're starting the next frame - they'll give us
      // the time.
      unsigned char *frame_buffer;
      uint32_t frame_len;
      void *ud = NULL;
      frame_buffer = NULL;
      ourtime = m_byte_stream->start_next_frame(&frame_buffer, 
						&frame_len,
						&ud);
      /*
       * If we're decoding video, see if we're playing - if so, check
       * if we've fallen significantly behind the audio
       */
      if (is_video() &&
	  (m_parent->get_session_state() == SESSION_PLAYING)) {
	uint64_t current_time = m_parent->get_playing_time();
	if (current_time >= ourtime) {
#ifdef DEBUG_DECODE
	  media_message(LOG_INFO, "Candidate for skip decode %llu our %llu", 
			       current_time, ourtime);
#endif
	  // If the bytestream can skip ahead, let's do so
	  if (m_byte_stream->can_skip_frame() != 0) {
	    int ret;
	    int hassync;
	    int count;
	    current_time += 200; 
	    count = 0;
	    // Skip up to the current time + 200 msec
	    do {
	      ret = m_byte_stream->skip_next_frame(&ourtime, &hassync,
						   &frame_buffer, &frame_len);
	      decode_skipped_frames++;
	    } while (ret != 0 &&
		     !m_byte_stream->eof() && 
		     current_time > ourtime);
	    if (m_byte_stream->eof() || ret == 0) continue;
#if 1
	    media_message(LOG_INFO, "Skipped ahead %llu  to %llu", 
			  current_time - 200, ourtime);
#endif
	    /*
	     * Ooh - fun - try to match to the next sync value - if not, 
	     * 15 frames
	     */
	    do {
	      ret = m_byte_stream->skip_next_frame(&ourtime, &hassync,
						   &frame_buffer, &frame_len);
	      if (hassync < 0) {
		uint64_t diff = ourtime - current_time;
		if (diff > (2 * C_LLU)) {
		  hassync = 1;
		}
	      }
	      decode_skipped_frames++;
	      count++;
	    } while (ret != 0 &&
		     hassync <= 0 &&
		     count < 30 &&
		     !m_byte_stream->eof());
	    if (m_byte_stream->eof() || ret == 0) continue;
#ifdef DEBUG_DECODE
	    media_message(LOG_INFO, "Matched ahead - count %d, sync %d time %llu",
				 count, hassync, ourtime);
#endif
	  }
	}
      }
#ifdef DEBUG_DECODE
      media_message(LOG_DEBUG, "Decoding %c frame " LLU, 
		    m_is_video ? 'v' : 'a', ourtime);
#endif
#ifdef TIME_DECODE
      clock_t start, end;
      start = clock();
#endif
      if (frame_buffer != NULL && frame_len != 0) {
	int sync_frame;
	ret = m_plugin->c_decode_frame(m_plugin_data,
				       ourtime,
				       m_streaming != 0,
				       &sync_frame,
				       frame_buffer, 
				       frame_len,
				       ud);
#ifdef DEBUG_DECODE
	media_message(LOG_DEBUG, "Decoding %c frame return %d", 
		      m_is_video ? 'v' : 'a', ret);
#endif
	if (ret > 0) {
	  m_byte_stream->used_bytes_for_frame(ret);
	} else {
	  m_byte_stream->used_bytes_for_frame(frame_len);
	}

#ifdef TIME_DECODE
      end = clock();
      if (ret > 0) {
	diff = end - start;
	media_message(LOG_DEBUG, "%c " LLD, m_is_video ? 'v' : 'a',diff);
	if (diff > max) max = diff;
	avg += diff;
	avg_cnt++;
      }
#endif
      }
    }
  }
#ifdef TIME_DECODE
  if (avg_cnt != 0) {
    media_message(LOG_INFO, "%s Decode avg time is " LLD " max " LLD, 
		  m_is_video ? "video" : "audio",
		  avg / avg_cnt,
		  max);
    media_message(LOG_INFO, "%s total %lld count %d", 
		  m_is_video ? "video" : "audio",
		  avg, avg_cnt);
  }
#endif
  if (m_is_video)
    media_message(LOG_NOTICE, "Video decoder skipped "LLU" frames", 
		  decode_skipped_frames);
  if (m_plugin) {
    m_plugin->c_close(m_plugin_data);
    m_plugin_data = NULL;
  }
  return (0);
}

/* end file player_media_decode.cpp */
