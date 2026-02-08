#ifndef UFT_ALGORITHMS_FLUX_FLUXSTREAMANALYZER_H
#define UFT_ALGORITHMS_FLUX_FLUXSTREAMANALYZER_H

typedef struct stathisto_
{
	uint32_t val;
	uint32_t occurence;
	float pourcent;
}stathisto;

typedef struct pulses_link_
{
	int32_t * forward_link;
	int32_t * backward_link;
	int32_t number_of_pulses;
}pulses_link;

typedef struct pulsesblock_
{
	int32_t timeoffset;
	int64_t tickoffset;

	int32_t timelength;
	int32_t ticklength;
	int32_t start_index;
	int32_t end_index;
	int32_t number_of_pulses;

	int32_t state;
	int32_t overlap_offset;
	int32_t overlap_size;

	int32_t locked;
}pulsesblock;

typedef struct track_blocks_
{

	pulsesblock * blocks;

	uint32_t number_of_blocks;

}track_blocks;


typedef struct s_match_
{
	int32_t yes;
	int32_t no;
	int32_t offset;
}s_match;

LIBFLUX_SIDE* ScanAndDecodeStream(LIBFLUX_CTX* flux_ctx,LIBFLUX_FXSA * fxs, int initialvalue,LIBFLUX_TRKSTREAM * track,pulses_link * pl,uint32_t start_index, short rpm,int phasecorrection, int flags);
int cleanupTrack(LIBFLUX_SIDE *curside);
LIBFLUX_FLOPPY * makefloppyfromtrack(LIBFLUX_SIDE * side);
void freefloppy(LIBFLUX_FLOPPY * fp);
void computehistogram(uint32_t *indata,int size,uint32_t *outdata);
int detectpeaks(LIBFLUX_CTX* flux_ctx, pll_stat *pll, uint32_t *histogram);
void libflux_FxStream_JitterFilter(LIBFLUX_FXSA * fxs,LIBFLUX_TRKSTREAM * stream);

typedef struct streamconv_
{
	LIBFLUX_CTX* libflux_ctx;

	uint32_t stream_in_mode;
	uint32_t bitstream_pos;
	uint32_t start_bitstream_pos;
	uint32_t end_bitstream_pos;

	int      start_revolution;
	int      end_revolution;

	LIBFLUX_SIDE * track;
	LIBFLUX_FXSA * fxs;

	float    stream_period_ps;
	uint64_t stream_time_offset_ps;
	uint64_t stream_prev_time_offset_ps;
	uint64_t stream_total_time_ps;

	float    overflow_value;
	double   conv_error;
	int      rollover;

	int      current_revolution;

	uint8_t  index_state;
	uint8_t  old_index_state;
	uint8_t  index_event;
	uint8_t  stream_end_event;

	int      stream_source;
}streamconv;

streamconv * initStreamConvert(LIBFLUX_CTX* libflux_ctx, LIBFLUX_SIDE * track, float stream_period_ps, float overflowvalue,int start_revolution,float start_offset,int end_revolution,float end_offset);
uint32_t StreamConvert_getNextPulse(streamconv * sc);
uint32_t StreamConvert_search_index(streamconv * sc, int index);
uint32_t StreamConvert_setPosition(streamconv * sc, int revolution, float offset);
void deinitStreamConvert(streamconv * sc);

#endif /* UFT_ALGORITHMS_FLUX_FLUXSTREAMANALYZER_H */
