/****************************************************************************
 *
 *   Copyright (c) 2012-2019 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/getopt.h>

#include "BMM150.hpp"

enum class BMM150_BUS {
	ALL = 0,
	I2C_INTERNAL,
	I2C_EXTERNAL
};

namespace bmm150
{

// list of supported bus configurations
struct bmm150_bus_option {
	BMM150_BUS busid;
	uint8_t busnum;
	uint32_t address;
	BMM150	*dev;
} bus_options[] = {
#if defined(USE_I2C)
#  if defined(PX4_I2C_BUS_ONBOARD) && defined(PX4_I2C_OBDEV_BMM150)
	{ BMM150_BUS::I2C_INTERNAL, &BMM150_I2C_interface, false, PX4_I2C_BUS_ONBOARD, PX4_I2C_OBDEV_BMM150, nullptr },
#  endif
#  if defined(PX4_I2C_BUS_EXPANSION) && defined(PX4_I2C_OBDEV_BMM150)
	{ BMM150_BUS::I2C_EXTERNAL, &BMM150_I2C_interface, false, PX4_I2C_BUS_EXPANSION, PX4_I2C_OBDEV_BMM150, nullptr },
#  endif
#  if defined(PX4_I2C_BUS_EXPANSION1) && defined(PX4_I2C_OBDEV_BMM150)
	{ BMM150_BUS::I2C_EXTERNAL, &BMM150_I2C_interface, false, PX4_I2C_BUS_EXPANSION1, PX4_I2C_OBDEV_BMM150, nullptr },
#  endif
#  if defined(PX4_I2C_BUS_EXPANSION2) && defined(PX4_I2C_OBDEV_BMM150)
	{ BMM150_BUS::I2C_EXTERNAL, &BMM150_I2C_interface, false, PX4_I2C_BUS_EXPANSION2, PX4_I2C_OBDEV_BMM150, nullptr },
#  endif
#endif
};

// find a bus structure for a busid
static struct bmm150_bus_option *find_bus(BMM150_BUS busid)
{
	for (bmm150_bus_option &bus_option : bus_options) {
		if ((busid == BMM150_BUS::ALL ||
		     busid == bus_option.busid) && bus_option.dev != nullptr) {

			return &bus_option;
		}
	}

	return nullptr;
}

static bool start_bus(bmm150_bus_option &bus, enum Rotation rotation)
{
	BMM150 *dev = new BMM150(bus.busnum, rotation);

	if (dev == nullptr || (dev->init() != PX4_OK)) {
		PX4_ERR("driver start failed");
		delete dev;
		return false;
	}

	bus.dev = dev;

	return true;
}

static int start(BMM150_BUS busid, enum Rotation rotation)
{
	for (bmm150_bus_option &bus_option : bus_options) {
		if (bus_option.dev != nullptr) {
			// this device is already started
			PX4_WARN("already started");
			continue;
		}

		if (busid != BMM150_BUS::ALL && bus_option.busid != busid) {
			// not the one that is asked for
			continue;
		}

		if (start_bus(bus_option, rotation)) {
			return PX4_OK;
		}
	}

	return PX4_ERROR;
}

static int stop(BMM150_BUS busid)
{
	bmm150_bus_option *bus = find_bus(busid);

	if (bus != nullptr && bus->dev != nullptr) {
		delete bus->dev;
		bus->dev = nullptr;

	} else {
		PX4_WARN("driver not running");
		return PX4_ERROR;
	}

	return PX4_OK;
}

static int status(BMM150_BUS busid)
{
	bmm150_bus_option *bus = find_bus(busid);

	if (bus != nullptr && bus->dev != nullptr) {
		bus->dev->print_info();
		return PX4_OK;
	}

	PX4_WARN("driver not running");
	return PX4_ERROR;
}

static int usage()
{
	PX4_INFO("missing command: try 'start', 'stop', 'status'");
	PX4_INFO("options:");
	PX4_INFO("    -X    (i2c external bus)");
	PX4_INFO("    -I    (i2c internal bus)");
	PX4_INFO("    -R rotation");

	return 0;
}

} // namespace

extern "C" int bmm150_main(int argc, char *argv[])
{
	int myoptind = 1;
	int ch;
	const char *myoptarg = nullptr;

	BMM150_BUS busid = BMM150_BUS::ALL;
	enum Rotation rotation = ROTATION_NONE;

	while ((ch = px4_getopt(argc, argv, "XIR:", &myoptind, &myoptarg)) != EOF) {
		switch (ch) {
		case 'X':
			busid = BMM150_BUS::I2C_EXTERNAL;
			break;

		case 'I':
			busid = BMM150_BUS::I2C_INTERNAL;
			break;

		case 'R':
			rotation = (enum Rotation)atoi(myoptarg);
			break;

		default:
			return bmm150::usage();
		}
	}

	if (myoptind >= argc) {
		return bmm150::usage();
	}

	const char *verb = argv[myoptind];

	if (!strcmp(verb, "start")) {
		return bmm150::start(busid, rotation);

	} else if (!strcmp(verb, "stop")) {
		return bmm150::stop(busid);

	} else if (!strcmp(verb, "status")) {
		return bmm150::status(busid);
	}

	return bmm150::usage();
}
