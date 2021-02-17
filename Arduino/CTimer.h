#define CTIMER_h

class CTimer {
	bool started = false;
	unsigned long start_time = 0;
	unsigned long timer_delay = 0;

public:
	CTimer() {
		//do nothing
	}
	CTimer(unsigned long time_delay, bool auto_start = false) {
		timer_delay = time_delay;
		if (auto_start) Start();
	}

	// Start timer
	void Start() {
		if (started) return;		// do not reset the timer
		started = true;
		start_time = millis();
	}
	void Start(unsigned long time_delay) {
		if (started) return;
		started = true;
		start_time = millis();
		timer_delay = time_delay;
	}
	
	// Stop timer
	void Stop() {
		started = false;
	}

	// Tries to reset the timer only if the specified time has passed
	bool TryReset(){ 
		if (!started) return false;
		if (millis() - start_time >= timer_delay) {
			started = false;
			return true;
		}
		return false;
	}
	bool TryReset(unsigned long time_delay) {
		if (!started) return false;
		if (millis() - start_time >= time_delay) {
			started = false;
			return true;
		}
		return false;
	}

	// Resets the current time. Does not stop the timer OR START it unless specified
	void Reset(bool auto_start = false) { 
		start_time = millis(); 
		if (auto_start) Start();
	}
	
	// Check if the timer is started or not
	bool Runs() {
		return started;
	}

	void SetDelay(unsigned long time_delay) {
		timer_delay = time_delay;
	}

	unsigned long GetElapsedTime() {
		if (!started) return 0;
		return millis() - start_time;
	}
};