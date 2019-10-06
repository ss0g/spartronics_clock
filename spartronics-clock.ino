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
} CalendarTime_t;

static const CalendarTime_t _now_date = { 2019, 10, 5, 00, 00, 00 };
static const CalendarTime_t _kickoff_date = { 2020, 1, 4, 7, 0, 0 };

/**
 *  List of events to count-down to. They must be sorted in ascending order.
 */
static const CalendarTime_t _important_times[] = {
    { 2019, 10, 11, 15, 15,  0 },       // GirlsGen pack time
    { 2020,  1,  4,  7,  0,  0 },       // Kickoff
    { 2020,  2, 18, 21,  0,  0 },       // Pseudo-bag-day
    { 2020,  2, 28, 12,  0,  0 },       // Glacier Peak
 // { 2020,  3, 13, 12,  0,  0 },       // (EVENT #2 - TBD)
    { 2020,  4,  1,  9,  0,  0 },       // District Champs
    { 2020,  4, 14, 12,  0,  0 },       // Worlds
};


/**
 *  Color names for various display modes
 */
typedef enum {
    COLOR_COLON,
    COLOR_DIGITS,
    COLOR_MESSAGE,
    // Insert new color names above this line
    COLOR_MAX
} ColorName_t;

// colors used for the matrix display
static const uint16_t _colors[] = {
    [COLOR_COLON] = Adafruit_NeoMatrix::Color(0, 0, 255),    // Blue
    [COLOR_DIGITS] = Adafruit_NeoMatrix::Color(25, 150, 0),  // Yellow
    [COLOR_MESSAGE] = Adafruit_NeoMatrix::Color(255, 0, 0)   // Red
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

State_t state;  // tracks the state of the display
static const char * const _message = { "Spartronics: 4915. Woot!" };

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
#define CONTROL_PIN 17      // pin for the NeoMatrix

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(45, 7,   // matrix width & height
  CONTROL_PIN,                                  // pin number
  NEO_MATRIX_TOP     + NEO_MATRIX_LEFT +        // matrix layout, flags added as needed
  NEO_MATRIX_ROWS    + NEO_MATRIX_ZIGZAG,
  NEO_GRB            + NEO_KHZ800);             // pixel type flags, added as needed


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
    state = state_machine(state, _event);
}


/**
 * Set time: 1st get time from computer, 2nd write to RTC
 * Note: this code could be simplified by switching use of Serial.parseInt()
 */
time_t set_time()
{
    // prompt user for time and set it on the board
    state = STATE_MESSAGE;
    print_scroll("Enter time using Serial Port");

    // read & parse time from the user
    Serial.println("Make sure Serial Terminal is set to Newline");
    Serial.println("Enter today's date and time: YYYY:MM:DD:hh:mm:ss");

    // NOTE: sscanf can be replaced w/ Serial.parseInt()
    char *datetime = recvWithEndMarker();
    Serial.print("Received: "); Serial.println(datetime);
    CalendarTime_t set_time;
    if (sscanf(datetime, "%04u:%02u:%02u:%02u:%02u:%02u",
                &set_time.year, &set_time.month, &set_time.day,
                &set_time.hour, &set_time.minute, &set_time.second) != 6)
    {
        Serial.println("Error: problem parsing YYYY:MM:DD:hh:mm:ss");
        return 0;
    }

    // set RTC time
    time_t t = convert_time(set_time);
    if (RTC.set(t) == false) // error occured while setting clock
    {
        state = STATE_MESSAGE;
        print_scroll("Error: RTC.set() failed!");
#if DEBUG_ON
        Serial.println("Error: RTC.set failed.");
#endif
    }

    return t;
}

// code from: http: //forum.arduino.cc/index.php?topic=396450
char *recvWithEndMarker()
{
    int numChars = 20;   // YYYY:MM:DD:hh:mm:ss
    static char receivedChars[20];
    boolean newData = false;
    static byte ndx = 0;
    char endMarker = '\n';
    char rc;

    while (newData == false)
    {
        while (Serial.available() > 0)
        {
            rc = Serial.read();

            if (rc != endMarker)
            {
                receivedChars[ndx] = rc;
                ndx++;
                if (ndx >= numChars)
                {
                    ndx = numChars - 1;
                }
            }
            else
            {
                receivedChars[ndx] = '\0'; // terminate the string
                ndx = 0;
                newData = true;
                break;
            }
        }
        delay(100);
    }
    return receivedChars;
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
    matrix.setBrightness(40);
    matrix.setTextColor(_colors[COLOR_MESSAGE]);

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
    Serial.print("force_set is: "); Serial.println(force_set);

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

    if (!RTC.read(tm))    // no prior date is set
    {
        if (RTC.chipPresent())
        {
            if ((now_date = set_time()) == 0)   // error: failed to set RTC time!
            {
                state = STATE_MESSAGE;
                print_scroll("Error: RTC set time!");
#if DEBUG_ON
                Serial.println("RTC is stopped. Set time failed!");
#endif
            }
        }
        else
        {
            state = STATE_MESSAGE;
            print_scroll("Error: RTC circuit!");
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
    state = STATE_COUNTDOWN;

    // IntervalTimer takes a callback function, and a number of microseconds
    // timer_interval is set to run the callback once per second
    timer_interval.begin(timer_callback,
            1000 /* ms per second */ * 1000 /* us per ms */);
    // scroll_interval is set to run the callback 10 times per second
    scroll_interval.begin(scroll_callback, 100 /* ms */ * 1000 /* us per ms */);
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
        state = state_machine(state, event);
    }
    else if (update_scroll_event)
    {
        update_scroll_event = false;
        event = EVENT_SCROLL;
        state = state_machine(state, event);
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

// standard pixel size for Adafruit_GFX is 6px per character
static const int _pixel_size = 6;
static int _x = matrix.width();
static int _message_color = 0;

/**
 * Display a scrolling message
 */
void print_scroll(const char *str)
{
    matrix.fillScreen(0);
    matrix.setCursor(_x, 0);
    matrix.print(str);
    /**
     * setCursor() sets the pixel position from where to render the text
     * offset is calculated based on font width, screen width, and text length
     * when offset is reached, pass is complete & color is updated
    **/
    int offset = (strlen(str) * _pixel_size) + _x;
    if (--_x < -offset)
    {
        // The message has scrolled off the screen, so reset variables
        _x = matrix.width();
        // Switch to the next color in our list of colors
        if (++_message_color >= COLOR_MAX)
        {
            _message_color = 0;
        }
        matrix.setTextColor(_colors[_message_color]);
    }
    matrix.show();
}

/**
 * Responsible for updating the state of the system
 * See State_t for possible states
 */
static State_t _get_next_state(State_t state)
{
    // we are using casts to ensure we can do enum math
    int next_state = (int)state + 1;
    if (next_state >= STATE_MAX)
    {
        next_state = 0;
    }

    return (State_t)next_state;
}

static State_t _handle_state_init(Event_t event)
{
    State_t next_state = STATE_COUNTDOWN;

    // We can do some initialization code here, if we need to

    return next_state;
}

static State_t _handle_state_countdown(Event_t event)
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
            // Advance to the next state
            next_state = _get_next_state(next_state);
            break;
        default:
            // Ignore all other events
            break;
    }

    return next_state;
}

static State_t _handle_state_message(Event_t event)
{
    State_t next_state = STATE_MESSAGE;

    switch (event)
    {
        case EVENT_SCROLL:
            // Scroll a message on the display
            print_scroll(_message);
            break;
        case EVENT_MODE:
            // Advance to the next state
            next_state = _get_next_state(next_state);
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

    switch (current_state)
    {
        case STATE_INIT:
            next_state = _handle_state_init(event);
            break;

        case STATE_COUNTDOWN:
            next_state = _handle_state_countdown(event);
            break;

        case STATE_MESSAGE:
            next_state = _handle_state_message(event);
            break;

        // Unhandled case. Don't know how we got here, just set the default
        default:
            next_state = STATE_INIT;
            break;
    }

    return next_state;
}


/****************************************************************************/
/************************** CLOCK DISPLAY ROUTINES **************************/
/****************************************************************************/

#define CHAR_HEIGHT 7

// 4x7 number digit font bitmaps -- used for displaying clockface
static const byte _number_width = 5;
static const byte _numbers[][CHAR_HEIGHT]
{
  // 0
  { B11110000,
    B10010000,
    B10010000,
    B10010000,
    B10010000,
    B10010000,
    B11110000 },
  //1
  { B00010000,
    B00010000,
    B00010000,
    B00010000,
    B00010000,
    B00010000,
    B00010000 },
  // 2
  { B11110000,
    B00010000,
    B00010000,
    B11110000,
    B10000000,
    B10000000,
    B11110000 },
  // 3
  { B11110000,
    B00010000,
    B00010000,
    B11110000,
    B00010000,
    B00010000,
    B11110000 },
  // 4
  { B10010000,
    B10010000,
    B10010000,
    B11110000,
    B00010000,
    B00010000,
    B00010000, },
  // 5
  {
    B11110000,
    B10000000,
    B10000000,
    B11110000,
    B00010000,
    B00010000,
    B11110000, },
  // 6
  { B11110000,
    B10000000,
    B10000000,
    B11110000,
    B10010000,
    B10010000,
    B11110000, },
  // 7
  { B11110000,
    B00010000,
    B00010000,
    B00010000,
    B00010000,
    B00010000,
    B00010000, },
  // 8
  { B11110000,
    B10010000,
    B10010000,
    B11110000,
    B10010000,
    B10010000,
    B11110000, },
  // 9
  { B11110000,
    B10010000,
    B10010000,
    B11110000,
    B00010000,
    B00010000,
    B11110000, },
};

static const byte _colon_width = 2;
static const byte _colon[] =
{
  // : (2 pix wide)
  B00000000,
  B10000000,
  B10000000,
  B00000000,
  B10000000,
  B10000000,
  B00000000,
};

/**
 *  Static variable to track the current display cursor position.
 */
static int _cursor = 0;

/**
 *  Clear the screen. Does not call matrix.show().
 */
void clear_screen(void)
{
  matrix.clear();
  _cursor = 0;
}

/**
 *  Print a single digit on the display, at the cursor location, in the
 *  given color. Does not call matrix.show().
 */
void print_digit(unsigned i, uint16_t color)
{
  if (i > 10)
  {
    // Invalid data, nothing to do...
    return;
  }

  matrix.fillRect(_cursor, 0, _cursor + _number_width, CHAR_HEIGHT, 0);
  matrix.drawBitmap(_cursor, 0, _numbers[i], _number_width, CHAR_HEIGHT, color);
  _cursor += _number_width;
}

/**
 *  Print a colon on the display, at the cursor location, in the given color.
 *  Does not call matrix.show().
 */
void print_colon(uint16_t color)
{
  matrix.fillRect(_cursor, 0, _cursor + _colon_width, CHAR_HEIGHT, 0);
  matrix.drawBitmap(_cursor, 0, _colon, _colon_width, CHAR_HEIGHT, color);
  _cursor += _colon_width;
}

/**
 *  Print a two-digit number on the display, at the cursor location,
 *  in the given color. Does not call matrix.show().
 */
void print_num(unsigned i, uint16_t color)
{
  uint8_t temp_digit;

  temp_digit = i % 10;
  i = i / 10;
  print_digit(i % 10, color);
  print_digit(temp_digit, color);
}

/**
 *  Print the given time on the display
 */
void print_time_interval(TimeInterval_t &time_interval)
{
    // Display the time interval
    clear_screen();
    print_num(time_interval.days, _colors[COLOR_DIGITS]);
    print_colon(_colors[COLOR_COLON]);
    print_num(time_interval.hours, _colors[COLOR_DIGITS]);
    print_colon(_colors[COLOR_COLON]);
    print_num(time_interval.minutes, _colors[COLOR_DIGITS]);
    print_colon(_colors[COLOR_COLON]);
    print_num(time_interval.seconds, _colors[COLOR_DIGITS]);
    matrix.show();
}
