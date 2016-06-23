/*
 * Copyright (c) 2015 John R. Bradley <jrb@turrettech.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "ff-callbacks.h"
#include "ff-circular-queue.h"
#include "ff-clock.h"
#include "ff-decoder.h"
#include "ff-frame.h"
#include "ff-packet-queue.h"
#include "ff-timer.h"

#include <libavutil/time.h>
#include <libswscale/swscale.h>

#include <assert.h>

//�����ļ�������֡���С������������ֻ�ѭ������ô�ͻỺ�������ļ��������ݣ������ظ�����
//��ʱ��״̬�Ƿ������޷�������������������ݶ�����
#define CACHE_WHOLE_FILE_SIZE_THREADHOLD 10485760

static bool queue_frame(struct ff_decoder *decoder, AVFrame *frame,
		double best_effort_pts)
{
	struct ff_frame *queue_frame;
	bool call_initialize;

	ff_circular_queue_wait_write(&decoder->frame_queue);

	if (decoder->abort) {
		return false;
	}

	queue_frame = ff_circular_queue_peek_write(&decoder->frame_queue);

	// Check if we need to communicate a different format has been received
	// to any callbacks
	AVCodecContext *codec = decoder->codec;
	call_initialize = (queue_frame->frame == NULL
			|| queue_frame->frame->width != codec->width
			|| queue_frame->frame->height != codec->height
			|| queue_frame->frame->format != codec->pix_fmt);

	if (queue_frame->frame != NULL)
		av_frame_free(&queue_frame->frame);

	queue_frame->frame = av_frame_clone(frame);
	queue_frame->clock = ff_clock_retain(decoder->clock);

	if (call_initialize)
		ff_callbacks_frame_initialize(queue_frame, decoder->callbacks);

	queue_frame->pts = best_effort_pts;

	ff_circular_queue_advance_write(&decoder->frame_queue, queue_frame);

	return true;
}

enum { SS_WAIT_BEGIN = 0, //�ȴ���һ֡
	SS_STATS, //ͳ�����н�������ݵĴ�С
	SS_BUILDCACHE, //�������棨�����������ݣ�
	SS_USECACHE, //ʹ�û��棨���ٽ��ܷ������������ݣ�
	SS_GIVEUP //��ʹ�û��棨���ݹ������ֹͣ�ˣ�
};

//ɾ���������棬�������
static void clear_cache(AVFrame*** frames, int* frame_count)
{
	int i;
	if (*frames == 0)
		return;

	for (i = 0; i < *frame_count; ++i)
		av_frame_unref((*frames)[i]);

	free(*frames);
	*frames = 0;
	*frame_count = 0;
}

void *ff_video_decoder_thread(void *opaque_video_decoder)
{
	struct ff_decoder *decoder = (struct ff_decoder*)opaque_video_decoder;

	struct ff_packet packet = {0};
	int complete;
	AVFrame *frame = av_frame_alloc();
	int ret;
	bool key_frame;

	uint64_t decoded_data_size = 0; //��������ݴ�С��ͳ��
	int decoded_frame_count = 0; //�ѽ���֡������
	int stats_status = SS_WAIT_BEGIN; //��ǰ״̬

	AVFrame** cache_frames = 0;
	int cache_frame_count = 0; //�ܻ����֡��
	int current_caching_frame = 0; //��ǰ�����֡��
	double total_length = 0; //��Ƶ��ʱ�䳤��
	double first_frame_ts = 0; //��һ֡��ʱ��
	double last_frame_end_ts = 0; //���һ֡�������ʱ��
	double ts_unit = 0; //������ʱ����õĵ�λ

	//��Ϊ�������Ƶִ�����ֻ�����������Աܿ�������Ƶ��ý��Դ
	//����ʵ��Ҫ�����gif�����������
	if (decoder->has_audio_stream)
		stats_status = SS_GIVEUP;

	while (!decoder->abort) {
		//������ֱ��ʹ��֮ǰ�����֡��״̬
		if (stats_status == SS_USECACHE)
		{
			//�������if����ֻ�е��յ�abort�ź�֮��Ż������
			int current_frame = 0;

			while (!decoder->abort)
			{
				//���ȵ���ĩβ����ת�ؿ�ͷ
				if (current_frame == cache_frame_count)
				{
					current_frame = 0;
				}

				double best_effort_pts = ff_decoder_get_best_effort_pts(decoder, cache_frames[current_frame]);
				queue_frame(decoder, cache_frames[current_frame], best_effort_pts);

				++current_frame;
			}

			stats_status = SS_GIVEUP;
			break;
		}
		else
		{
			ret = packet_queue_get(&decoder->packet_queue, &packet, 1);
			if (ret == FF_PACKET_FAIL) {
				// should we just use abort here?
				break;
			}

			//��ʼ�����ݻ�õ�packet�ϵ���Ϣ����״̬�任
			if (stats_status != SS_GIVEUP)
			{
				switch (stats_status)
				{
				case SS_WAIT_BEGIN:
					if (packet.is_first_frame)
					{
						stats_status = SS_STATS;
						ts_unit = packet.ts_unit;
					}
					break;

				case SS_STATS:
					//ͳ�ƹ�����seek�˵��ǲ��ǵ���һ֡������
					if (packet.is_seeked && packet.is_first_frame == false)
					{
						stats_status = SS_WAIT_BEGIN;
						cache_frames = 0;
						decoded_data_size = 0;
						decoded_frame_count = 0;
					}
					//ͳ�ƹ����������˵�һ֡��˵��ѭ���ˣ���������ͳ�Ƶ����ݽ�����һ������
					//��һ֡��ʼͳ�Ƶ�ʱ���ߵ���SS_WAIT_BEGIN��Ĵ��벻������
					else if (packet.is_first_frame)
					{
						if (decoded_data_size > CACHE_WHOLE_FILE_SIZE_THREADHOLD)
							stats_status = SS_GIVEUP;
						else
						{
							current_caching_frame = 0;
							stats_status = SS_BUILDCACHE;
							cache_frame_count = decoded_frame_count;
							cache_frames = (AVFrame**)calloc(sizeof(*cache_frames), cache_frame_count);
						}
					}
					break;

				case SS_BUILDCACHE:
					//�������������seek�˶��Ҳ��ǵ���һ֡������
					if (packet.is_seeked && packet.is_first_frame == false)
					{
						stats_status = SS_WAIT_BEGIN;
						clear_cache(&cache_frames, &cache_frame_count);

						decoded_data_size = 0;
						decoded_frame_count = 0;
					}
					//������������������˵�һ֡��˵��������ѭ�������Խ�����һ������
					else if (packet.is_first_frame)
					{
						total_length = last_frame_end_ts - first_frame_ts;
						stats_status = SS_USECACHE;
						//ֱ�ӻ�ȥѭ����ͷ��Ȼ��ͱ�ɴӻ����ȡ
						//��������������Ĵ���ѵ�ǰ��������������ȥ����Ȼ���������
						//֮�󻺴��еĵ�һ֡�����һ�Σ������������һ֡�����
						continue;
					}
					break;
				}
			}
			//���������ݻ�õ�packet�ϵ���Ϣ����״̬�任

			if (packet.base.data == decoder->packet_queue.flush_packet.base.data) {
				avcodec_flush_buffers(decoder->codec);
				continue;
			}

			// We received a reset packet with a new clock
			if (packet.clock != NULL) {
				if (decoder->clock != NULL)
					ff_clock_release(&decoder->clock);
				decoder->clock = ff_clock_move(&packet.clock);
				av_free_packet(&packet.base);
				continue;
			}

			int64_t start_time = ff_clock_start_time(decoder->clock);
			key_frame = packet.base.flags & AV_PKT_FLAG_KEY;

			// We can only make decisions on keyframes for
			// hw decoders (maybe just OSX?)
			// For now, always make drop decisions on keyframes
			bool frame_drop_check = key_frame;
			// Must have a proper packet pts to drop frames here
			frame_drop_check &= start_time != AV_NOPTS_VALUE;

			if (frame_drop_check)
				ff_decoder_set_frame_drop_state(decoder,
				start_time, packet.base.pts);

			avcodec_decode_video2(decoder->codec, frame,
				&complete, &packet.base);

			// Did we get an entire video frame?  This doesn't guarantee
			// there is a picture to show for some codecs, but we still want
			// to adjust our various internal clocks for the next frame
			if (complete) {
				// If we don't have a good PTS, try to guess based
				// on last received PTS provided plus prediction
				// This function returns a pts scaled to stream
				// time base
				double best_effort_pts =
					ff_decoder_get_best_effort_pts(decoder, frame);

				last_frame_end_ts = (best_effort_pts + packet.base.duration) * packet.ts_unit;
				if (packet.is_first_frame)
					first_frame_ts = last_frame_end_ts;

				queue_frame(decoder, frame, best_effort_pts);

				++decoded_frame_count;

				if (stats_status == SS_BUILDCACHE)
				{
					if (current_caching_frame < cache_frame_count)
					{
						cache_frames[current_caching_frame] = av_frame_clone(frame);
						++current_caching_frame;
					}
					else
					{
						//��Ӧ�û�С�ڵġ����������£�������е���죬������ڿƼ��ȽϿ���
						clear_cache(&cache_frames, &current_caching_frame);
						stats_status = SS_GIVEUP;
					}
				}

				av_frame_unref(frame);
			}

			av_free_packet(&packet.base);
		}
	}

	if (decoder->clock != NULL)
		ff_clock_release(&decoder->clock);

	av_frame_free(&frame);

	clear_cache(&cache_frames, &current_caching_frame);
	return NULL;
}
