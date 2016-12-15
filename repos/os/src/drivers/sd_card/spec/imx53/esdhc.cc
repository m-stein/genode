/*
 * \brief  Freescale Enhanced Secured Digital Host Controller
 * \author Martin Stein
 * \date   2016-12-13
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* local includes */
#include <esdhcv2.h>

using namespace Sd_card;
using namespace Genode;


void
Esdhcv2_controller::_stop_transmission_finish_xfertyp(Xfertyp::access_t &xfertyp)
{
	Xfertyp::Msbsel::set(xfertyp, 1);
	Xfertyp::Bcen::set(xfertyp, 1);
	Xfertyp::Dmaen::set(xfertyp, 1);
}


int Esdhcv2_controller::_wait_for_cmd_complete_mb_finish(bool const reading)
{
	if (reading) { return 0; }

	/*
	 * The "Auto Command 12" feature of the ESDHC seems to be
	 * broken for multi-block writes as it causes command-
	 * timeout errors sometimes. Thus, we stop such transfers
	 * manually.
	 */
	if (_stop_transmission())  { return -1; }

	/*
	 * The manual termination of multi-block writes seems to leave
	 * the card in a busy state sometimes. This causes
	 * errors on subsequent commands. Thus, we have to synchronize
	 * manually with the card-internal state.
	 */
	return _wait_for_card_ready_mbw() ? -1 : 0;
}


bool Esdhcv2_controller::_issue_cmd_finish_xfertyp(Xfertyp::access_t &xfertyp,
                                                   bool const         transfer,
                                                   bool const         multiblock,
                                                   bool const         reading)
{
	if (transfer) {
		Xfertyp::Bcen::set(xfertyp, 1);
		Xfertyp::Msbsel::set(xfertyp, 1);
		if (multiblock) {
			/*
			 * The "Auto Command 12" feature of the ESDHC seems to be
			 * broken for multi-block writes as it causes command-
			 * timeout errors sometimes.
			 */
			if (reading) {
				Xfertyp::Ac12en::set(xfertyp, 1); }

			if (_use_dma) {
				Xfertyp::Dmaen::set(xfertyp, 1); }
		}
		Xfertyp::Dtdsel::set(xfertyp,
			reading ? Xfertyp::Dtdsel::READ : Xfertyp::Dtdsel::WRITE);
	}
	return _wait_for_cmd_allowed() ? false : true;
}


bool Esdhcv2_controller::_supported_host_version(Hostver::access_t hostver)
{
	return Hostver::Vvn::get(hostver) == 18 &&
	       Hostver::Svn::get(hostver) == 1;
}


void Esdhcv2_controller::_watermark_level(Wml::access_t &wml)
{
	Wml::Wr_wml::set(wml, 16);
	Wml::Wr_brst_len::set(wml, 8);
}


void Esdhcv2_controller::_reset_amendments()
{
	/*
	 * The SDHC specification says that a software reset shouldn't
	 * have an effect on the the card detection circuit. The ESDHC
	 * clears Sysctl::Ipgen, Sysctl::Hcken, and Sysctl::Peren
	 * nonetheless which disables clocks that card detection relies
	 * on.
	 */
	Sysctl::access_t sysctl = read<Sysctl>();
	Sysctl::Ipgen::set(sysctl, 1);
	Sysctl::Hcken::set(sysctl, 1);
	Sysctl::Peren::set(sysctl, 1);
	write<Sysctl>(sysctl);
}


void Esdhcv2_controller::_clock_finish(Clock clock)
{
	write<Sysctl::Dtocv>(Sysctl::Dtocv::SDCLK_TIMES_2_POW_27);
	switch (clock) {
	case CLOCK_INITIAL:     _enable_clock(CLOCK_DIV_512); break;
	case CLOCK_OPERATIONAL: _enable_clock(CLOCK_DIV_8);   break; }
	write<Sysctl::Dtocv>(Sysctl::Dtocv::SDCLK_TIMES_2_POW_27);
}


void Esdhcv2_controller::_disable_clock_preparation() { }

void Esdhcv2_controller::_enable_clock_finish() { }
