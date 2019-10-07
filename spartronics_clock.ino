/**
 * Spartronics sample code for Teensy LC based display via Adafruit NeoMatrix
 *
 * Parts/Libraries:
 *  - Teensy LC
 *  - Adafruit NeoMatrix
 *  - DS3231 RTC
 *  - AT24C32 for storage -- currently not used
 *
 * Libraries:
 *  - DebounceEvent button manager by Xose Perez
 *    https://github.com/xoseperez/debounceevent
 *  - IntervalTimer library for Teensy
 *  - DS1307RTC library (compatible w/ DS3231)
 *    https://www.pjrc.com/teensy/td_libs_DS1307RTC.html
 *
 * Usage:
 *  3 buttons control the behavior of the display
 *  - button 1: specifies mode for reprogramming the display:
 *    prints scrolling message or coundown clock
 *  - button 2 & 3: unused -- button push will generate corresponding events
 *
 * Note: holding the DEC button down during boot will force setting
 *       the datetime via user input
 *
 * Note: Spartronics clock also hosts an AT24C32 which is a 32K EEPROM.
 *       It is currently not used.
 *       In the future, this can hold long messages or short novels.
 */

#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <DebounceEvent.h>
#include <DS1307RTC.h>
#include <Time.h>
#include <TimeLib.h>
#include <Wire.h>
#include <stdbool.h>

#define DEBUG_ON    0

#define NUM_ELEMENTS(x) (sizeof(x) / sizeof(x[0]))

// Spartronics colors!
#define SPARTRONICS_YELLOW Adafruit_NeoMatrix::Color(90, 90, 0)
#define SPARTRONICS_BLUE Adafruit_NeoMatrix::Color(0, 50, 170)

/**
 *  Structure to hold time values in their human-readable components
 */
typedef struct {
    unsigned year;
    unsigned month;
    unsigned day;
    unsigned hour;
    unsigned minute;
    unsigned second;
    const char *description;
} CalendarTime_t;

static const CalendarTime_t _now_date = { 2019, 10, 5, 00, 00, 00, "now" };
static const CalendarTime_t _kickoff_date = { 2020, 1, 4, 7, 0, 0, "kickoff" };

/**
 *  List of events to count-down to. They must be sorted in ascending order.
 */
static const CalendarTime_t _important_times[] = {
    { 2019, 10, 11, 15, 15,  0, "GirlsGen" },           // GirlsGen pack time
    { 2020,  1,  4,  7,  0,  0, "Kickoff" },            // Kickoff
    { 2020,  2, 18, 21,  0,  0, "Bag day" },            // Pseudo-bag-day
    { 2020,  2, 28, 12,  0,  0, "Glacier Peak" },       // Glacier Peak
 // { 2020,  3, 13, 12,  0,  0, "EVENT 2" },            // (EVENT #2 - TBD)
    { 2020,  4,  1,  9,  0,  0, "Districts" },          // District Champs
    { 2020,  4, 14, 12,  0,  0, "Worlds" },             // Worlds
};

static time_t _target_time;
static unsigned _target_index;


/**
 *  Color names for various display modes
 */
typedef enum {
    COLOR_COLON,
    COLOR_DIGITS,
    COLOR_RED,
    COLOR_ORANGE,
    COLOR_YELLOW,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_PURPLE,
    // Insert new color names above this line
    COLOR_MAX
} ColorName_t;

// colors used for the matrix display
static const uint16_t _colors[] = {
    [COLOR_COLON] = SPARTRONICS_BLUE,
    [COLOR_DIGITS] = SPARTRONICS_YELLOW,
    [COLOR_RED] = Adafruit_NeoMatrix::Color(128, 0, 0),
    [COLOR_ORANGE] = Adafruit_NeoMatrix::Color(128, 128, 0),
    [COLOR_YELLOW] = SPARTRONICS_YELLOW,
    [COLOR_GREEN] = Adafruit_NeoMatrix::Color(0, 128, 0),
    [COLOR_BLUE] = SPARTRONICS_BLUE,
    [COLOR_PURPLE] = Adafruit_NeoMatrix::Color(0, 128, 128),
};

/**
 * Setup the display state
 *      - countdown: displays the countdown clock -- updated by timer_ticks
 *      - days/hrs/mins/secs/: changes the value using inc/dec button presses
 *      - message: prints text messages
 */
typedef enum   // at each button mode press, display mode changes accordingly
{
    STATE_INIT = 0,
    STATE_MESSAGE,
    STATE_COUNTDOWN,
    STATE_DATE,
    STATE_TIME,
    // Add new states above this line
    STATE_MAX
} State_t;

/**
 * Setup the events for triggering state changes
 * These are driven by the buttons
 */
typedef enum
{
    EVENT_NULL = 0,     // no event
    EVENT_MODE,
    EVENT_INCREMENT,
    EVENT_DECREMENT,
    EVENT_TIMER,
    EVENT_SCROLL,
    // Add new events above this line
    EVENT_MAX
} Event_t;

State_t _state;  // tracks the state of the display

static const char *_messages[] = {
    "Spartronics: 4915. Woot!",
    "Robots don't quit!",
    // Add new messages above this line
    NULL
};

/**
 *  Variable to track the time interval remaining until the event being tracked
 */
static uint32_t _countdown_time;

/**
 *  Structure to hold the time interval value to be displayed on the clock
 */
typedef struct              // parses the countdown_time to its components
{
    uint8_t  seconds;
    uint8_t  minutes;
    uint8_t  hours;
    uint16_t days;
} TimeInterval_t;


/**
 *  Setup interrupts for scroll and timer events, callbacks,
 *  and variables to update controls
 */
IntervalTimer scroll_interval;
IntervalTimer timer_interval;

bool update_timer_event = false;
bool update_scroll_event = false;;

void timer_callback()
{
    update_timer_event = true;
    // Compute new time value here, so it's done regardless of state
    if (_countdown_time != 0)
    {
        _countdown_time = _countdown_time - 1;
    }
}

void scroll_callback()
{
    update_scroll_event = true;
}

/** Good reference for display configuration:
 *  https://learn.adafruit.com/adafruit-neopixel-uberguide/neomatrix-library
 *
 *  Spartronics display is setup as a 45x7 matrix.The first pixel is on top left,
 *  lines are arranged in rows as zig-zag order. The shield uses 800Khz (v2)
 *  pixels that expect GRB color data
 */
#define CONTROL_PIN     17
#define MATRIX_WIDTH    45
#define MATRIX_HEIGHT   7

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(MATRIX_WIDTH, MATRIX_HEIGHT,
  CONTROL_PIN,                                  // pin number
  NEO_MATRIX_TOP     + NEO_MATRIX_LEFT +        // matrix layout
  NEO_MATRIX_ROWS    + NEO_MATRIX_ZIGZAG,       // flags describe our panel
  NEO_GRB            + NEO_KHZ800);             // pixel type flags


/**
 * For botton management using the DebounceEvent library
 * https://github.com/xoseperez/debounceevent
 */
#define MODE_BUTTON_PIN 14      // D14 (D == digital port) -- sets display mode
#define INC_BUTTON_PIN  15      // D15 -- increments value
#define DEC_BUTTON_PIN  16      // D16 -- decrements value

#define CUSTOM_DEBOUNCE_DELAY   50
#define CUSTOM_REPEAT_DELAY     0       // disabled double clicks

DebounceEvent *mode_button;
DebounceEvent *inc_button;
DebounceEvent *dec_button;

void event_callback(uint8_t pin, uint8_t event, uint8_t count, uint16_t length) {
    Event_t _event = EVENT_NULL;

    if (event != EVENT_PRESSED)
    {
        return;
    }

    switch (pin)
    {
        case MODE_BUTTON_PIN:
            _event = EVENT_MODE;
            break;
        case INC_BUTTON_PIN:
            _event = EVENT_INCREMENT;
            break;
        case DEC_BUTTON_PIN:
            _event = EVENT_DECREMENT;
            break;
        default:
            return;
    }
    // update the state machine
    _state = state_machine(_state, _event);
}




/**
 * setup the display, matrix, and button callbacks
 **/
void setup()
{
    Serial.begin(115200);

    // setup matrix display
    matrix.begin();
    matrix.setTextWrap(false);
    matrix.setBrightness(100);
    matrix.setTextColor(_colors[COLOR_RED]);

    // setup the button event loops
    mode_button = new DebounceEvent(MODE_BUTTON_PIN, event_callback,
            BUTTON_PUSHBUTTON | BUTTON_DEFAULT_HIGH | BUTTON_SET_PULLUP);
    inc_button = new DebounceEvent(INC_BUTTON_PIN, event_callback,
            BUTTON_PUSHBUTTON | BUTTON_DEFAULT_HIGH | BUTTON_SET_PULLUP);
    dec_button = new DebounceEvent(DEC_BUTTON_PIN, event_callback,
            BUTTON_PUSHBUTTON | BUTTON_DEFAULT_HIGH | BUTTON_SET_PULLUP);

    /* Check if the DEC_BUTTON_PIN is held low (pressed) while we boot up.
     * If so, force time setting.
     */
    bool force_set = (digitalRead(DEC_BUTTON_PIN) == LOW);

    /*
     * Initialize the countdown_time using bag date and current date
     * for countdown_time
     *   - via DS3231 RTC, read current time if available. If not,
     *     set current time using compiler time
     *   - bag date is set in the code
     * Use str format for creating date
     *     https://forum.arduino.cc/index.php?topic=465881.0
     *   - YYYY, MM, DD, HH, MM, SS
     */
    time_t now_date = 0;
    tmElements_t tm;

    if (force_set)
    {
        now_date = set_time();
    }
    else
    {
        now_date = get_time_from_rtc();
    }

    if (!RTC.read(tm))    // no prior date is set
    {
        if (RTC.chipPresent())
        {
            if ((now_date = set_time()) == 0)   // error: failed to set RTC time!
            {
                _state = STATE_MESSAGE;
                message_print("ERR: RTC");
#if DEBUG_ON
                Serial.println("RTC is stopped. Set time failed!");
#endif
            }
        }
        else
        {
            _state = STATE_MESSAGE;
            message_print("ERR: HW");
#if DEBUG_ON
            Serial.println("RTC read error!  Please check the circuitry.");
#endif
        }
    }
    else
    {
        // Successfully got the time from the RTC!
        now_date = makeTime(tm);
    }

    if (now_date == 0)
    {
        // date not set due to error, manually set the date
        now_date = convert_time(_now_date);
    }
    // FIXME: Figure out the best time to use...
    time_t kickoff_date = convert_time(_kickoff_date);
    _countdown_time = kickoff_date - now_date;

#if DEBUG_ON
    Serial.print("kickoff_date: "); Serial.println(kickoff_date);
    Serial.print("now_date: "); Serial.println(now_date);
    Serial.print("_countdown_time: "); Serial.println(_countdown_time);
#endif

    // initialize state machine
    _state = STATE_COUNTDOWN;

    // IntervalTimer takes a callback function, and a number of microseconds
    // timer_interval is set to run the callback once per second
    timer_interval.begin(timer_callback,
            1000 /* ms per second */ * 1000 /* us per ms */);
    // scroll_interval is set to run the callback 10 times per second
    scroll_interval.begin(scroll_callback, 100 /* ms */ * 1000 /* us per ms */);

    if (find_next_target(now()) == 0)
    {
        // No target time found!
        _state = STATE_MESSAGE;
        message_print("ERR: Target");
    }
}

/**
 * continously loop through button callbacks to detect display state changes
 * if no button presses, update the display accordingly
 *      - if in countdown mode, wait for timer_ticks to adjust seconds, etc.
 *      - if in sayings mode, print the 'sayings' on the matrix
 *      - in any other mode, wait for inc/dec button presses
 */
void loop()
{
    Event_t event;

    // listen for button callbacks and trigger state_machine change accordingly
    mode_button->loop();
    inc_button->loop();
    dec_button->loop();

    // listen for timer events & update state machine
    if (update_timer_event)
    {
        update_timer_event = false;
        event = EVENT_TIMER;
        _state = state_machine(_state, event);
    }
    else if (update_scroll_event)
    {
        update_scroll_event = false;
        event = EVENT_SCROLL;
        _state = state_machine(_state, event);
    }

    // Check if a new countdown time is needed?
    if (_countdown_time == 0)
    {
        // Go through list and find next time that is ahead of now
    }
}

/**
 *  given a date info, makes time -- returns time in seconds since epoch
 */
time_t convert_time(const CalendarTime_t &time)
{
    tmElements_t tmSet;

    tmSet.Year = time.year - 1970;
    tmSet.Month = time.month;
    tmSet.Day = time.day;
    tmSet.Hour = time.hour;
    tmSet.Minute = time.minute;
    tmSet.Second = time.second;

    return makeTime(tmSet); //convert to time_t
}

time_t find_next_target(time_t now)
{
    time_t target = 0;

    for (unsigned idx=0; idx<NUM_ELEMENTS(_important_times); idx++)
    {
        time_t temp;
        // Convert the time to time_t so we can compare to now
        temp = convert_time(_important_times[idx]);

        // Compare to 'now'. If temp is in the future, then it is our target
        if (temp > now)
        {
            target = temp;
            _target_time = target;
            _target_index = idx;
            break;
        }
    }

    return target;
}

/**
 *  Break down the provided time interval (in seconds) into clock-style
 *  components of days, hours, minutes and seconds.
 *    see: https://pastebin.com/sfEjA94n
 */
void compute_elapsedTime(TimeInterval_t &time_interval, uint32_t countdown_time)
{
    time_interval.seconds = countdown_time % 60;
    countdown_time /= 60; // now it is minutes

    time_interval.minutes = countdown_time % 60;
    countdown_time /= 60; // now it is hours

    time_interval.hours = countdown_time % 24;
    countdown_time /= 24; // now it is days

    time_interval.days = countdown_time;
}

static State_t _handle_state_init(Event_t event, bool first_time)
{
    State_t next_state = STATE_COUNTDOWN;

    // We can do some initialization code here, if we need to
    matrix.clear();

    return next_state;
}

static State_t _handle_state_countdown(Event_t event, bool first_time)
{
    State_t next_state = STATE_COUNTDOWN;
    TimeInterval_t time_interval;

    switch (event)
    {
        case EVENT_TIMER:
            // Print the remaining time interval on the display
            compute_elapsedTime(time_interval, _countdown_time);

            // display the time value
            print_time_interval(time_interval);
#if DEBUG_ON
            Serial.print("---_countdown_time: ");
            Serial.println(_countdown_time);
            Serial.print("---display time: ");
            Serial.print(time_interval.days);
            Serial.print(":");
            Serial.print(time_interval.hours);
            Serial.print(":");
            Serial.print(time_interval.minutes);
            Serial.print(":");
            Serial.println(time_interval.seconds);
#endif
            break;
        case EVENT_MODE:
            // Go to the message display
            next_state = STATE_MESSAGE;
            break;
        case EVENT_INCREMENT:
            // Go to the date display
            next_state = STATE_DATE;
            break;
        default:
            // Ignore all other events
            break;
    }

    return next_state;
}

/**
 *  Print the current date on the display
 */
static State_t _handle_state_date(Event_t event, bool first_time)
{
    State_t next_state = STATE_DATE;
    static unsigned timeout;    // Static so it is saved between runs
    CalendarTime_t calendar_time;

    if (first_time)
    {
        // Set our timeout
        timeout = 10 /* seconds */;

        // Get the calendar time

        // Print the date
        print_date(calendar_time);
    }

    switch (event)
    {
        case EVENT_TIMER:
            timeout = timeout - 1;
            if (timeout == 0)
            {
                next_state = STATE_TIME;
            }
            break;
        case EVENT_MODE:
            // Go to the message display
            next_state = STATE_MESSAGE;
            break;
        case EVENT_DECREMENT:
            // Go back to the countdown display
            next_state = STATE_COUNTDOWN;
            break;
        case EVENT_INCREMENT:
            // Go to the date display
            next_state = STATE_TIME;
            break;
        default:
            // Ignore all other events
            break;
    }

    return next_state;
}

/**
 *  Print the current time on the display
 */
static State_t _handle_state_time(Event_t event, bool first_time)
{
    State_t next_state = STATE_TIME;
    static unsigned timeout;    // Static so it is saved between runs
    CalendarTime_t calendar_time;

    if (first_time)
    {
        // Set our timeout
        timeout = 10 /* seconds */;

        // Get the calendar time

        // Print the time
        print_time(calendar_time);
    }

    switch (event)
    {
        case EVENT_TIMER:
            timeout = timeout - 1;
            if (timeout == 0)
            {
                next_state = STATE_DATE;
            }
            break;
        case EVENT_MODE:
            // Go to the message display
            next_state = STATE_MESSAGE;
            break;
        case EVENT_DECREMENT:
            // Go back to the countdown display
            next_state = STATE_COUNTDOWN;
            break;
        case EVENT_INCREMENT:
            // Go to the date display
            next_state = STATE_DATE;
            break;
        default:
            // Ignore all other events
            break;
    }

    return next_state;
}

static State_t _handle_state_message(Event_t event, bool first_time)
{
    State_t next_state = STATE_MESSAGE;

    if (first_time)
    {
        // Set the message to scroll
        message_start(_messages[0]);
    }

    switch (event)
    {
        case EVENT_SCROLL:
            // Scroll a message on the display
            message_scroll();
            break;
        case EVENT_MODE:
            // Advance to the next state
            next_state = STATE_COUNTDOWN;
            break;
        default:
            // Ignore all other events
            break;
    }

    return next_state;
}

/**
 * State machine to control the display
 * Note: events triggered by the buttons and timer_ticks
 */
State_t state_machine(State_t current_state, Event_t event)
{
    State_t next_state = current_state;
    bool first_time = false;
    static State_t last_state = STATE_INIT;

    // Set a flag if this is the first time in this new state
    if (current_state != last_state)
    {
        first_time = true;
    }

    switch (current_state)
    {
        case STATE_INIT:
            next_state = _handle_state_init(event, first_time);
            break;

        case STATE_COUNTDOWN:
            next_state = _handle_state_countdown(event, first_time);
            break;

        case STATE_DATE:
            next_state = _handle_state_date(event, first_time);
            break;

        case STATE_TIME:
            next_state = _handle_state_time(event, first_time);
            break;

        case STATE_MESSAGE:
            next_state = _handle_state_message(event, first_time);
            break;

        // Unhandled case. Don't know how we got here, just set the default
        default:
            next_state = STATE_INIT;
            break;
    }

    last_state = current_state;

    return next_state;
}
