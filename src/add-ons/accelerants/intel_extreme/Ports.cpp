/*
 * Copyright 2006-2013, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 *		Michael Lotz, mmlr@mlotz.ch
 */


#include "Ports.h"

#include "accelerant.h"
#include "intel_extreme.h"

#include <ddc.h>

#include <stdlib.h>
#include <string.h>


#define TRACE_PORTS
#ifdef TRACE_PORTS
extern "C" void _sPrintf(const char* format, ...);
#	define TRACE(x) _sPrintf x
#else
#	define TRACE(x) ;
#endif


Port::Port(port_index index, const char* baseName)
	:
	fPortIndex(index),
	fPortName(NULL),
	fEDIDState(B_NO_INIT)
{
	char portID[2];
	portID[0] = 'A' + index - INTEL_PORT_A;
	portID[1] = 0;

	char buffer[32];
	buffer[0] = 0;

	strlcat(buffer, baseName, sizeof(buffer));
	strlcat(buffer, " ", sizeof(buffer));
	strlcat(buffer, portID, sizeof(buffer));
	fPortName = strdup(buffer);
}


Port::~Port()
{
	free(fPortName);
}


bool
Port::HasEDID()
{
	if (fEDIDState == B_NO_INIT)
		GetEDID(NULL);

	return fEDIDState == B_OK;
}


status_t
Port::GetEDID(edid1_info* edid, bool forceRead)
{
	if (fEDIDState == B_NO_INIT || forceRead) {
		TRACE(("trying to read EDID on %s\n", PortName()));

		uint32 ddcRegister = _DDCRegister();
		if (ddcRegister == 0) {
			fEDIDState = B_ERROR;
			return fEDIDState;
		}

		TRACE(("using register %" B_PRIx32 "\n", ddcRegister));

		i2c_bus bus;
		bus.cookie = (void*)ddcRegister;
		bus.set_signals = &_SetI2CSignals;
		bus.get_signals = &_GetI2CSignals;
		ddc2_init_timing(&bus);

		fEDIDState = ddc2_read_edid1(&bus, &fEDIDInfo, NULL, NULL);

		if (fEDIDState == B_OK) {
			TRACE(("found EDID on port %s:\n", PortName()));
			edid_dump(&fEDIDInfo);
		}
	}

	if (fEDIDState != B_OK)
		return fEDIDState;

	if (edid != NULL)
		memcpy(edid, &fEDIDInfo, sizeof(edid1_info));

	return B_OK;
}


status_t
Port::GetPLLLimits(pll_limits& limits)
{
	return B_ERROR;
}


status_t
Port::_GetI2CSignals(void* cookie, int* _clock, int* _data)
{
	uint32 ioRegister = (uint32)cookie;
	uint32 value = read32(ioRegister);

	*_clock = (value & I2C_CLOCK_VALUE_IN) != 0;
	*_data = (value & I2C_DATA_VALUE_IN) != 0;

	return B_OK;
}


status_t
Port::_SetI2CSignals(void* cookie, int clock, int data)
{
	uint32 ioRegister = (uint32)cookie;
	uint32 value;

	if (gInfo->shared_info->device_type.InGroup(INTEL_TYPE_83x)) {
		// on these chips, the reserved values are fixed
		value = 0;
	} else {
		// on all others, we have to preserve them manually
		value = read32(ioRegister) & I2C_RESERVED;
	}

	if (data != 0)
		value |= I2C_DATA_DIRECTION_MASK;
	else {
		value |= I2C_DATA_DIRECTION_MASK | I2C_DATA_DIRECTION_OUT
			| I2C_DATA_VALUE_MASK;
	}

	if (clock != 0)
		value |= I2C_CLOCK_DIRECTION_MASK;
	else {
		value |= I2C_CLOCK_DIRECTION_MASK | I2C_CLOCK_DIRECTION_OUT
			| I2C_CLOCK_VALUE_MASK;
	}

	write32(ioRegister, value);
	read32(ioRegister);
		// make sure the PCI bus has flushed the write

	return B_OK;
}


// #pragma mark - Analog Port


AnalogPort::AnalogPort()
	:
	Port(INTEL_PORT_A, "Analog")
{
}


bool
AnalogPort::IsConnected()
{
	return HasEDID();
}


uint32
AnalogPort::_DDCRegister()
{
	// always fixed
	return INTEL_I2C_IO_A;
}


// #pragma mark - LVDS Panel


LVDSPort::LVDSPort()
	:
	Port(INTEL_PORT_C, "LVDS")
{
}


bool
LVDSPort::IsConnected()
{
	uint32 registerValue = read32(INTEL_DISPLAY_LVDS_PORT);
	if (gInfo->shared_info->device_type.HasPlatformControlHub()) {
		// there's a detection bit we can use
		if ((registerValue & PCH_LVDS_DETECTED) == 0)
			return false;
	}

	// Try getting EDID, as the LVDS port doesn't overlap with anything else,
	// we don't run the risk of getting someone else's data.
	return HasEDID();
}


uint32
LVDSPort::_DDCRegister()
{
	// always fixed
	return INTEL_I2C_IO_C;
}


// #pragma mark - DVI/SDVO/generic


DigitalPort::DigitalPort(port_index index, const char* baseName)
	:
	Port(index, baseName)
{
}


bool
DigitalPort::IsConnected()
{
	// As this port overlaps with pretty much everything, this must be called
	// after having ruled out all other port types.
	return HasEDID();
}


uint32
DigitalPort::_DDCRegister()
{
	switch (PortIndex()) {
		case INTEL_PORT_B:
			return INTEL_I2C_IO_E;
		case INTEL_PORT_C:
			return INTEL_I2C_IO_D;
		case INTEL_PORT_D:
			return INTEL_I2C_IO_F;
		default:
			return 0;
	}

	return 0;
}


// #pragma mark - HDMI


HDMIPort::HDMIPort(port_index index)
	:
	DigitalPort(index, "HDMI")
{
}


bool
HDMIPort::IsConnected()
{
	if (!gInfo->shared_info->device_type.SupportsHDMI())
		return false;

	uint32 portRegister = _PortRegister();
	if (portRegister == 0)
		return false;

	if (!gInfo->shared_info->device_type.HasPlatformControlHub()
		&& PortIndex() == INTEL_PORT_C) {
		// there's no detection bit on this port
	} else if ((read32(portRegister) & PORT_DETECTED) == 0)
		return false;

	return HasEDID();
}


uint32
HDMIPort::_PortRegister()
{
	// on PCH there's an additional port sandwiched in
	bool hasPCH = gInfo->shared_info->device_type.HasPlatformControlHub();

	switch (PortIndex()) {
		case INTEL_PORT_B:
			return hasPCH ? PCH_HDMI_PORT_B : INTEL_HDMI_PORT_B;
		case INTEL_PORT_C:
			return hasPCH ? PCH_HDMI_PORT_C : INTEL_HDMI_PORT_C;
		case INTEL_PORT_D:
			return hasPCH ? PCH_HDMI_PORT_D : 0;
		default:
			return 0;
	}

	return 0;
}


// #pragma mark - DisplayPort


DisplayPort::DisplayPort(port_index index, const char* baseName)
	:
	DigitalPort(index, baseName)
{
}


bool
DisplayPort::IsConnected()
{
	uint32 portRegister = _PortRegister();
	if (portRegister == 0)
		return false;

	if ((read32(portRegister) & PORT_DETECTED) == 0)
		return false;

	return HasEDID();
}


uint32
DisplayPort::_PortRegister()
{
	switch (PortIndex()) {
		case INTEL_PORT_A:
			return INTEL_DISPLAY_PORT_A;
		case INTEL_PORT_B:
			return INTEL_DISPLAY_PORT_B;
		case INTEL_PORT_C:
			return INTEL_DISPLAY_PORT_C;
		case INTEL_PORT_D:
			return INTEL_DISPLAY_PORT_D;
		default:
			return 0;
	}

	return 0;
}


// #pragma mark - Embedded DisplayPort


EmbeddedDisplayPort::EmbeddedDisplayPort()
	:
	DisplayPort(INTEL_PORT_A, "Embedded DisplayPort")
{
}


bool
EmbeddedDisplayPort::IsConnected()
{
	if (!gInfo->shared_info->device_type.IsMobile())
		return false;

	return DisplayPort::IsConnected();
}
