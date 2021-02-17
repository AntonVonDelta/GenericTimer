
#ifdef VCC
// VC++ Adaptations and Patches
#include <Windows.h>
#define uint8_t unsigned char
#define uint16_t unsigned short
#define HIGH 1
#define LOW 2
#define OUTPUT 1
#define INPUT 2


unsigned long _millis = 0;
unsigned long millis() { return _millis; }

void delay(int time) { Sleep(time); };
void pinMode(int pin, int type) { return; }
int digitalRead(int pin) { return 0; }
void digitalWrite(int pin, int value) { return; }
#endif


//#include <avr/EEPROM.h>
#include "EEPROM_custom.h"
#include "CTimer.h"

#define MEM_ADDRESS 0
#define MS_IN_SEC	1000	//50		// How many milliseconds are there in a second
#define CLK  0
#define DAT  1
#define SHOW 2
#define BTN_PIN 3
#define ACTION 4		//relay pin

#define BTN_PRESSED 1
#define BTN_UNPRESSED 0
#define BTN1 0
#define BTN2 1

#define BTN1_CLICK 1
#define BTN1_DBL_CLICK 3  //implies BTN1_CLICK
#define BTN2_CLICK 4
#define BTN1_BTN2_CLICK 13   //implies BTN1_CLICK and BTN2_CLICK but NOT BTN1_DBL_CLICK

#define BTN1_DOWN  16
#define BTN1_UP 32
#define BTN2_DOWN 64
#define BTN2_UP 128

#define VIEW_TIMER_WORK 1
#define VIEW_TIMER_DELAY 2
#define VIEW_TIME_ELAPSING 3		// view the actual elapsing of time
#define VIEW_TIME_OVERWRITE 4		// the view used when the controls are overriden by user

#define TIMER_WORKING 1
#define TIMER_DELAYING 2
#define TIMER_OVERWRITE 4 			// Control taken by user...waiting to be reseted also by the user

/*
   TIMER WORK:  how much time spent on doing work
   TIMER DELAY: how much time spent between delays
*/
/*      CLICK BUTTON BEHAVIOUR:
	BTN1 click      ->  increments TIMER WORK
	BTN1 dbl click  ->  decrement TIMER WORK

	BTN1+BTN2 clicks: switch to other state(TIMER WORK or TIMER DELAY)
	BTN2 click: does the work until pressed again. Resets TIMER DELAY

	On diagram: BTN2=S2 -> Qh
				BTN1=S1 -> Qg

	OutputPinsState:
	  Display1  Display2 S1 S2
	|0000 000|0 0000 00|00
*/
struct BUTTON_EVENT {
	bool Button;
	unsigned long Start, Stop;
} BTN1_EVENT, BTN2_EVENT;
uint16_t diff(uint16_t a, uint16_t b) {
	return (max(a, b) - min(a, b));
}

// Help for debugging
CTimer debugTime;
bool debugging = false;

#ifdef VCC
const uint8_t digit[14] = {};//{ B1011111, B0001001, B0111110, B0111011, B1101001, B1110011, B1110111, B0011001, B1111111, B1111011,   B1111101,B1010110,B1110110,B1110100 };
#else
const uint8_t digit[14] = { B1011111, B0001001, B0111110, B0111011, B1101001, B1110011, B1110111, B0011001, B1111111, B1111011,   B1111101,B1010110,B1110110,B1110100 };

#endif
uint16_t OutputPinsState = 0xFFFE; // A mapping betwen virtual states of pins and serial device's outputs. Default: 88(on screen), S2 is grounded


bool display_flicker = false;
CTimer last_flicker(200);
bool show_flicker_stage = true;	// if number should be shown at this stage when flicker is enabled


uint8_t TIMER_WORK = 5;
uint8_t TIMER_DELAY = 2;
uint8_t ACTIVITY_TIMEOUT = 4;

// Debounce timers and variables
CTimer S1_LastBounce(500);
CTimer S2_LastBounce(100);
CTimer S1_HoldTimer;
int S1_lastButtonState = HIGH;
int S1_buttonState = HIGH;

int S2_lastButtonState = HIGH;
int S2_buttonState = HIGH;

// Buttons state
CTimer BTN1KeyHold(60);
BUTTON_EVENT Events[2];
unsigned char Event_Count = 0;

uint8_t KeyboardState = 0xA0;   // BTN1 and BTN2 are up by default

// Used for debugging the internal code by showing data on the screen
void Debug(uint8_t nr) {
	debugging = true;
	debugTime.Start();

	if (display_flicker) {
		last_flicker.Start();

		if (show_flicker_stage) {
			if (last_flicker.TryReset()) {
				show_flicker_stage = false;
				last_flicker.Reset(true);
				OutputPinsState = OutputPinsState & 0x3;
				return;
			}
		} else {
			if (last_flicker.TryReset()) {
				show_flicker_stage = true;
				last_flicker.Reset(true);
			} else return;
		}
	}

	if (nr > 99) {
		// Deactivate screen
		OutputPinsState = OutputPinsState & 0x3;
		return;
	}

	uint8_t nr1 = digit[nr % 10];
	nr /= 10;
	uint8_t nr2 = digit[nr];
	// Nr1 is shown on screen 2(on the irght)
	// Nr2 is shown on screen 1(on the left)

	OutputPinsState = (OutputPinsState & 0x1FF) | (nr2 << 9);
	OutputPinsState = (OutputPinsState & 0xFE03) | (nr1 << 2);
}
void Debug(const char* str) {
	debugging = true;
	debugTime.Start();

	if (display_flicker) {
		last_flicker.Start();

		if (show_flicker_stage) {
			if (last_flicker.TryReset()) {
				show_flicker_stage = false;
				last_flicker.Reset(true);
				OutputPinsState = OutputPinsState & 0x3;
				return;
			}
		} else {
			if (last_flicker.TryReset()) {
				show_flicker_stage = true;
				last_flicker.Reset(true);
			} else return;
		}
	}

	unsigned char index = 10;

	index = (str[0] == 'A' ? 10 : index);
	index = (str[0] == 'C' ? 11 : index);
	index = (str[0] == 'E' ? 12 : index);
	index = (str[0] == 'F' ? 13 : index);

	// Remove content from screens
	OutputPinsState = OutputPinsState & 0x3;

	// Nr1 is shown on screen 2(on the irght)
	// Nr2 is shown on screen 1(on the left)
	uint8_t nr2 = digit[index];

	OutputPinsState = (OutputPinsState & 0x1FF) | (nr2 << 9);
}
void print_serial() {
	uint16_t temp = OutputPinsState;

	for (int i = 0; i < 16; i++) {
		bool bitt = temp % 2;
		temp /= 2;
		digitalWrite(DAT, bitt ? HIGH : LOW);

		digitalWrite(CLK, HIGH);
		delay(1);
		digitalWrite(CLK, LOW);
		delay(1);
	}

	digitalWrite(SHOW, HIGH);
	digitalWrite(SHOW, LOW);
}
void erase_serial() {
	return;
	digitalWrite(DAT, LOW);
	for (int i = 0; i < 16; i++) {
		digitalWrite(CLK, HIGH);
		delay(1);
		digitalWrite(CLK, LOW);
		delay(1);
	}
}

// Sets the number on the segments
// Number 100 deactivates the screen
void SetNumber(uint8_t nr) {
	if (debugging) {
		if (debugTime.TryReset(2000)) debugging = false;
		else return;
	}

	if (display_flicker) {
		last_flicker.Start();

		if (show_flicker_stage) {
			if (last_flicker.TryReset()) {
				show_flicker_stage = false;
				last_flicker.Reset(true);
				OutputPinsState = OutputPinsState & 0x3;
				return;
			}
		} else {
			if (last_flicker.TryReset()) {
				show_flicker_stage = true;
				last_flicker.Reset(true);
			} else return;
		}
	}

	if (nr > 99) {
		// Deactivate screen
		OutputPinsState = OutputPinsState & 0x3;
		return;
	}

	uint8_t nr1 = digit[nr % 10];
	nr /= 10;
	uint8_t nr2 = digit[nr];
	// Nr1 is shown on screen 2(on the irght)
	// Nr2 is shown on screen 1(on the left)

	OutputPinsState = (OutputPinsState & 0x1FF) | (nr2 << 9);
	OutputPinsState = (OutputPinsState & 0xFE03) | (nr1 << 2);
}
void SetText(const char* str) {
	unsigned char index = 10;

	index = (str[0] == 'A' ? 10 : index);
	index = (str[0] == 'C' ? 11 : index);
	index = (str[0] == 'E' ? 12 : index);
	index = (str[0] == 'F' ? 13 : index);

	// Remove content from screens
	OutputPinsState = OutputPinsState & 0x3;

	// Nr1 is shown on screen 2(on the irght)
	// Nr2 is shown on screen 1(on the left)
	uint8_t nr2 = digit[index];

	OutputPinsState = (OutputPinsState & 0x1FF) | (nr2 << 9);
}

// This function takes care of states based on timers
void UpdateKeyboardState() {
	if (Event_Count != 0) {
		BUTTON_EVENT first = Events[0];

		if (millis() - first.Stop > 202) { //102 is bigger than 100(the distance between double clicks)
		  //Reset keyboard because we create a new state here( and overwrite the last one)
			KeyboardState = KeyboardState & 0xF0;

			if (Event_Count == 1) {
				if (first.Button == BTN1) KeyboardState |= BTN1_CLICK;
				else KeyboardState |= BTN2_CLICK;
				Event_Count = 0;   // We analyzed all events and reached a conclusion
				return;
			}

			BUTTON_EVENT other = Events[1];

			if (first.Button == BTN1 && other.Button == BTN1) {
				KeyboardState |= BTN1_DBL_CLICK;
				//if (diff(other.Start, first.Stop) < 200) KeyboardState |= BTN1_DBL_CLICK;
				//else KeyboardState |= BTN1_CLICK;
			}
			if (first.Button == BTN1 && other.Button == BTN2) {
				if (diff(other.Start, first.Start) < 100) KeyboardState |= BTN1_BTN2_CLICK;
				else KeyboardState |= BTN1_CLICK;
			}
			if (first.Button == BTN2 && other.Button == BTN1) {
				if (diff(other.Start, first.Start) < 100) KeyboardState |= BTN1_BTN2_CLICK;
				else KeyboardState |= BTN2_CLICK;
			}
			if (first.Button == BTN2 && other.Button == BTN2) {
				KeyboardState |= BTN2_CLICK;
			}

			Event_Count = 0;  // We analyzed all events and reached a conclusion
		}
	}
}

// Button management system
// btn: 0->S1, 1->S2
// state: 1->pressed; 0->unpressed
void UpdateButtonState(bool btn, bool state) {
	if (btn == BTN1) {
		if (state == BTN_PRESSED) {
			BTN1_EVENT.Button = BTN1;
			BTN1_EVENT.Start = millis();
		} else {
			BTN1_EVENT.Stop = millis();

			S1_HoldTimer.Stop();

			// If button was held up then don't register this button event and reset
			if (BTN1KeyHold.Runs()) {
				BTN1KeyHold.Stop();
				return;
			}

			
			if (Event_Count != 2) {
				Events[Event_Count] = BTN1_EVENT;
				Event_Count++;
			}
		}
	} else {
		if (state == BTN_PRESSED) {
			BTN2_EVENT.Button = BTN2;
			BTN2_EVENT.Start = millis();
		} else {
			BTN2_EVENT.Stop = millis();
			if (Event_Count != 2) {
				Events[Event_Count] = BTN2_EVENT;
				Event_Count++;
			}
		}
	}
}

// Used for printing the number, reading buttons states and togling between them
void TranslateStates() {
	int current_state = digitalRead(BTN_PIN);

	if ((~OutputPinsState) & 0x1) {
		// S2 is grounded

		// If signal flips reset the timer
		if (current_state != S2_lastButtonState) {
			S2_LastBounce.Reset(true);
		}

		//Here only the signal that takes more than 60ms (meaning it's stable) will pass by this filter
		// If the signal oscillates quickly the timer will be reset above
		if (S2_LastBounce.TryReset(40)) {
			if (current_state != S2_buttonState) {
				UpdateButtonState(BTN2, current_state == LOW ? BTN_PRESSED : BTN_UNPRESSED);
				S2_buttonState = current_state;
			}
		}

		S2_lastButtonState = current_state;
	} else {
		// S1 is grounded

		// If signal flips reset the timer
		if (current_state != S1_lastButtonState) {
			S1_LastBounce.Reset(true);
		}

		//Here only the signal that takes more than 70ms (meaning it's stable) will pass by this filter
		// If the signal oscillates quickly the timer will be reset above
		// I don't need debounce this button too much. All of my buttons seem to not need debounce. Perhaps it's because of the pullup or some capacitance on the way
		//if (S1_LastBounce.TryReset(2)) {
		if (current_state != S1_buttonState) {
			UpdateButtonState(BTN1, current_state == LOW ? BTN_PRESSED : BTN_UNPRESSED);
			S1_buttonState = current_state;

			if (S1_buttonState == LOW) S1_HoldTimer.Start();
		}
		//}
		if (S1_HoldTimer.TryReset(500)) {
			BTN1KeyHold.Start();
		}

		S1_lastButtonState = current_state;
	}

	//Also run the keyboard event generator
	UpdateKeyboardState();

	// Toggle button pins
	OutputPinsState = OutputPinsState ^ 0x3;

	erase_serial();
	print_serial();
}

uint8_t GetButtonEvent() {
	uint8_t temp = KeyboardState;
	KeyboardState = KeyboardState & 0xF0;
	return temp;
}


bool action_state = 0;	// 0 ==low, 1 high
bool timerChanged = false;

// Time workings for keeping work time and deciding what to display depending on user activity
uint8_t current_waiting_state;
uint8_t current_view_mode;

unsigned long time_elapsed = 0;
unsigned long millis_offset = 0;

CTimer entered_settings;
CTimer time_last_activity;
CTimer switchModeTimer;
void SwitchViewMode() { current_view_mode = (current_view_mode == VIEW_TIMER_WORK ? VIEW_TIMER_DELAY : VIEW_TIMER_WORK); }
bool IsInSettings() { return (current_view_mode == VIEW_TIMER_WORK) || (current_view_mode == VIEW_TIMER_DELAY); }

void setup() {
	// put your setup code here, to run once:
	pinMode(DAT, OUTPUT);
	pinMode(CLK, OUTPUT);
	pinMode(SHOW, OUTPUT);
	pinMode(BTN_PIN, INPUT);
	pinMode(ACTION, OUTPUT);

	digitalWrite(CLK, LOW);
	digitalWrite(DAT, LOW);
	digitalWrite(SHOW, LOW);
	digitalWrite(ACTION, LOW);

	current_waiting_state = TIMER_DELAYING;
	current_view_mode = VIEW_TIME_ELAPSING;

	time_elapsed = millis();
	TIMER_DELAY = EEPROM.read(MEM_ADDRESS) % 100;
	TIMER_WORK = EEPROM.read(MEM_ADDRESS + 1) % 100;

	TIMER_DELAY = (TIMER_DELAY == 0 ? 1 : TIMER_DELAY);
}

void loop() {
	uint8_t key_event = GetButtonEvent();

	if (current_waiting_state == TIMER_DELAYING) {
		if (BTN1KeyHold.Runs()) {
			entered_settings.Start();
			time_last_activity.Reset(true);

			if (BTN1KeyHold.TryReset()) {
				timerChanged = true;
				BTN1KeyHold.Reset(true);

				// If view is out-of-focus first change view BUT do not modify values
				if (current_view_mode == VIEW_TIME_ELAPSING) current_view_mode = VIEW_TIMER_WORK;
				else {
					if (current_view_mode == VIEW_TIMER_DELAY) TIMER_DELAY++;
					else TIMER_WORK++;
				}
			}
		} else {
			// Here the user can modify the timeouts
			if ((key_event & BTN1_BTN2_CLICK) == BTN1_BTN2_CLICK) {
				time_last_activity.Reset(true);
				switchModeTimer.Start();
				entered_settings.Start();

				SwitchViewMode();
			} else if ((key_event & BTN1_DBL_CLICK) == BTN1_DBL_CLICK) {
				time_last_activity.Reset(true);
				entered_settings.Start();
				timerChanged = true;

				// If view is out-of-focus first change view BUT do not modify value
				if (current_view_mode == VIEW_TIME_ELAPSING) {
					current_view_mode = VIEW_TIMER_WORK;
					switchModeTimer.Start();
				} else {
					if (current_view_mode == VIEW_TIMER_DELAY) TIMER_DELAY--;
					else TIMER_WORK--;
				}
			} else if ((key_event & BTN1_CLICK) == BTN1_CLICK) {
				time_last_activity.Reset(true);
				entered_settings.Start();
				timerChanged = true;

				// If view is out-of-focus first change view BUT do not modify value
				if (current_view_mode == VIEW_TIME_ELAPSING) {
					current_view_mode = VIEW_TIMER_WORK;
					switchModeTimer.Start();
				} else {
					if (current_view_mode == VIEW_TIMER_DELAY) TIMER_DELAY++;
					else TIMER_WORK++;
				}
			}
		}
	}

	// If user underflows from 0 to maximum (100)
	if (TIMER_DELAY == 255) TIMER_DELAY = 99;
	if (TIMER_WORK == 255) TIMER_WORK = 99;

	// In case user overflows over 99
	TIMER_WORK %= 100;
	TIMER_DELAY = (TIMER_DELAY > 99 ? 1 : TIMER_DELAY);
	TIMER_DELAY = (TIMER_DELAY == 0 ? 99 : TIMER_DELAY);		// If TIMER_DELAY reaches 0(which is not allowed) we make it overflow


	// Change scrren if user inactive and Save permanent settings
	if (current_waiting_state == TIMER_DELAYING && time_last_activity.TryReset((unsigned long)ACTIVITY_TIMEOUT * 1000)) {
		current_view_mode = VIEW_TIME_ELAPSING;
		
		// Used for stopping time. We calculate how much the user spent in settings view and then offset millis.
		millis_offset+= entered_settings.GetElapsedTime();
		entered_settings.Stop();

		if (timerChanged) {
			timerChanged = false;
			EEPROM.update(MEM_ADDRESS, TIMER_DELAY);
			EEPROM.update(MEM_ADDRESS + 1, TIMER_WORK);
		}
	}

	// Handle special ACTION triggering button B2
	if (!IsInSettings() && (key_event & BTN1_BTN2_CLICK) != BTN1_BTN2_CLICK && (key_event & BTN2_CLICK) == BTN2_CLICK) {
		if ((current_waiting_state & TIMER_OVERWRITE) || current_waiting_state!=TIMER_WORKING) {
			action_state = !action_state;

			if (action_state) {
				// Start Action
				current_waiting_state |= TIMER_OVERWRITE;
				current_view_mode = VIEW_TIME_OVERWRITE;
			} else {
				current_view_mode = VIEW_TIME_ELAPSING;
				current_waiting_state &= ~TIMER_OVERWRITE;
			}
			digitalWrite(ACTION, action_state ? HIGH : LOW);
		} else {
			// Pump is working but user pressed this button
			// STOP IMMEDIATELY. PANIC CLICK
			current_waiting_state = TIMER_DELAYING;
			time_elapsed = millis();

			action_state = 0;
			digitalWrite(ACTION, LOW);
		}
	}

	// Apply protection if user forgets about the action
	if (current_waiting_state & TIMER_OVERWRITE) {
		if (millis() - time_elapsed > (unsigned long)20 * 60 * MS_IN_SEC) {
			current_view_mode = VIEW_TIME_ELAPSING;
			current_waiting_state &= ~TIMER_OVERWRITE;

			action_state = 0;
			digitalWrite(ACTION, LOW);
		}
	}


	// Action triggering only when not in settings mode
	if (!IsInSettings()) {
		if (current_waiting_state & TIMER_DELAYING) {
			if (millis() - time_elapsed >= (unsigned long)TIMER_DELAY * 60 * MS_IN_SEC + millis_offset) {
				millis_offset = 0;
				current_waiting_state &= ~TIMER_DELAYING;
				current_waiting_state |= TIMER_WORKING;

				time_elapsed = millis();

				if (!(current_waiting_state & TIMER_OVERWRITE) && TIMER_WORK>0) {
					action_state = 1;
					digitalWrite(ACTION, HIGH);
				}
			}
		} else if (current_waiting_state & TIMER_WORKING) {
			// millis_offset is only needed when in TIMER_DELAYING mode because the user can't be in TIMER_WORKING AND enter the settings!
			// For it to be this way it would mean the user *just* exited the settings and directly passed this check from above: current_waiting_state & TIMER_WORKING
			//    which is not possible because the user can't enter the settings on TIMER_WORKING
			if (millis() - time_elapsed >= (unsigned long)TIMER_WORK * 60 * MS_IN_SEC) {
				current_waiting_state &= ~TIMER_WORKING;
				current_waiting_state |= TIMER_DELAYING;

				time_elapsed = millis();

				if (!(current_waiting_state & TIMER_OVERWRITE)) {
					action_state = 0;
					digitalWrite(ACTION, LOW);
				}
			}
		}
	}

	switch (current_view_mode) {
	case VIEW_TIMER_WORK: {
		display_flicker = false;
		if (switchModeTimer.TryReset(1000) || !switchModeTimer.Runs()) {
			SetNumber(TIMER_WORK);
		} else SetText("A");
		break;
	}
	case VIEW_TIMER_DELAY: {
		display_flicker = false;
		if (switchModeTimer.TryReset(1000) || !switchModeTimer.Runs()) {
			SetNumber(TIMER_DELAY);
		} else SetText("C");
		break;
	}
	case VIEW_TIME_OVERWRITE: {
		display_flicker = true;
		last_flicker.SetDelay(70);
		SetNumber(0);
		break;
	}
	default: {
		last_flicker.SetDelay(500);		// If delaying set normal flicker
		display_flicker = true;
		if (current_waiting_state == TIMER_DELAYING) {
			SetNumber(TIMER_DELAY - ((millis() - time_elapsed) - millis_offset) / (60UL * MS_IN_SEC));
		} else {
			last_flicker.SetDelay(70);
			SetNumber(TIMER_WORK - (millis() - time_elapsed) / (60UL * MS_IN_SEC));
		}
	}
	}


	TranslateStates();
}

#ifdef VCC
int main() {
	setup();
	while (true) { loop(); Sleep(1000); _millis += 1; }
}
#endif
