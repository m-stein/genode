
		void _handle_tic()
		{
			unsigned volatile remote_ms = _tic_session.elapsed_ms();
			unsigned volatile ts        = Kernel::time();
			if (remote_ms > ~(unsigned)0 / 1000) {
				error("elapsed ms value too high"); }

			unsigned remote_us         = remote_ms * 1000UL;
			unsigned remote_us_diff    = remote_us - _remote_us;
			unsigned remote_us_ts_diff = ts - _remote_us_ts;
			unsigned us_to_ts_factor   = (remote_us_ts_diff << FACTOR_SHIFT) /
			                             (remote_us_diff ? remote_us_diff : 1);

			if (remote_us < _local_us) {
				_local_us = _ts_to_local_us(ts); }
			else {
				_local_us = remote_us; }

			_us_to_ts_factor = (_us_to_ts_factor + us_to_ts_factor) >> 1;
			_remote_us       = remote_us;
			_remote_us_ts    = ts;
			_local_us_ts     = ts;
		}

		unsigned _ts_to_local_us(unsigned ts) const
		{
			unsigned local_us_ts_diff = ts - _local_us_ts;
			unsigned local_us = _local_us +
			                    ((local_us_ts_diff << FACTOR_SHIFT) /
			                     _us_to_ts_factor);

			if (local_us < _local_us) {
				return _local_us + ((_local_us - local_us) >> 1); }
			else {
				return local_us; }
		}

		Timer_hd_time_source(::Timer::Session &session,
		                     ::Timer::Session &tic_session,
		                     Entrypoint       &ep)
		: Timer_time_source(session, ep), _tic_session(tic_session),
		  _tic(ep, *this, &Timer_hd_time_source::_handle_tic),
		  _test(ep, *this, &Timer_hd_time_source::_handle_test)

		{
			_tic_session.sigh(_tic);
			_tic_session.trigger_periodic(_tic_us);
		}

		Microseconds curr_time_local(unsigned i)
		{
unsigned old_local_us = _local_us;

			unsigned ts = Kernel::time();
			_local_us   = _ts_to_local_us(ts);

if (i > DMAX) { throw -1; }
_dtd[i] = ts - _local_us_ts;
_dus[i] = _local_us;
_dmd[i] = _local_us - old_local_us;
_dfac[i] = _us_to_ts_factor;

			_local_us_ts = ts;

			return Microseconds(_local_us);
		}

		void schedule_timeout(Microseconds     duration,
		                      Timeout_handler &handler) override
		{
			if (duration.value < MIN_TIMEOUT_US)
				duration.value = MIN_TIMEOUT_US;

			if (duration.value > max_timeout().value)
				duration.value = max_timeout().value;

			_handler = &handler;
			_session.trigger_once(duration.value);
		}
