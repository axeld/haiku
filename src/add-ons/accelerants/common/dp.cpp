/*
 * Copyright 2012, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *      Alexander von Gluck, kallisti5@unixzen.com
 */


#include "dp.h"


#define TRACE_DISPLAY
#ifdef TRACE_DISPLAY
extern "C" void _sPrintf(const char* format, ...);
#   define TRACE(x...) _sPrintf("radeon_hd: " x)
#else
#   define TRACE(x...) ;
#endif

#define ERROR(x...) _sPrintf("radeon_hd: " x)


uint32
dp_encode_link_rate(uint32 linkRate)
{
	switch (linkRate) {
		case 162000:
			// 1.62 Ghz
			return DP_LINK_RATE_162;
		case 270000:
			// 2.7 Ghz
			return DP_LINK_RATE_270;
		case 540000:
			// 5.4 Ghz
			return DP_LINK_RATE_540;
	}

	ERROR("%s: Unknown DisplayPort Link Rate!\n",
		__func__);
	return DP_LINK_RATE_162;
}


uint32
dp_decode_link_rate(uint32 rawLinkRate)
{
	switch (rawLinkRate) {
		case DP_LINK_RATE_162:
			return 162000;
		case DP_LINK_RATE_270:
			return 270000;
		case DP_LINK_RATE_540:
			return 540000;
	}
	ERROR("%s: Unknown DisplayPort Link Rate!\n",
		__func__);
	return 162000;
}


uint32
dp_get_lane_count(dp_info* dpInfo, display_mode* mode)
{
	size_t pixelChunk;
	size_t pixelsPerChunk;
	status_t result = get_pixel_size_for((color_space)mode->space, &pixelChunk,
		NULL, &pixelsPerChunk);

	if (result != B_OK) {
		TRACE("%s: Invalid color space!\n", __func__);
		return 0;
	}

	uint32 bitsPerPixel = (pixelChunk / pixelsPerChunk) * 8;

	uint32 maxLaneCount = dpInfo->config[DP_MAX_LANE_COUNT]
		& DP_MAX_LANE_COUNT_MASK;
	uint32 maxLinkRate = dp_decode_link_rate(dpInfo->config[DP_MAX_LINK_RATE]);

	uint32 lane;
	for (lane = 1; lane < maxLaneCount; lane <<= 1) {
		uint32 maxDPPixelClock = (maxLinkRate * lane * 8) / bitsPerPixel;
		if (mode->timing.pixel_clock <= maxDPPixelClock)
			break;
	}

	TRACE("%s: Lanes: %" B_PRIu32 "\n", __func__, lane);

	return lane;
}
