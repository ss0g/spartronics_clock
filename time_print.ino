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

static const byte _slash_width = 8;
static const byte _slash[] =
{
  // / (8 pix wide)
  B00000010,
  B00000100,
  B00001000,
  B00010000,
  B00100000,
  B01000000,
  B10000000,
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
 *  Print a slash on the display, at the cursor location, in the given color.
 *  Does not call matrix.show().
 */
void print_slash(uint16_t color)
{
  matrix.fillRect(_cursor, 0, _cursor + _slash_width, CHAR_HEIGHT, 0);
  matrix.drawBitmap(_cursor, 0, _slash, _slash_width, CHAR_HEIGHT, color);
  _cursor += _slash_width;
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
 *  Print the countdown time on the display
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

/**
 *  Print the time on the display
 */
void print_time(CalendarTime_t &calendar_time)
{
    // Display the time
    clear_screen();

    // Offset the time to center it
    _cursor = 6;

    print_num(calendar_time.hour, _colors[COLOR_DIGITS]);
    print_colon(_colors[COLOR_COLON]);
    print_num(calendar_time.minute, _colors[COLOR_DIGITS]);
    print_colon(_colors[COLOR_COLON]);
    print_num(calendar_time.second, _colors[COLOR_DIGITS]);

    matrix.show();
}

/**
 *  Print the date on the display
 */
void print_date(CalendarTime_t &calendar_time)
{
    // Display the date
    clear_screen();

    print_num(calendar_time.year, _colors[COLOR_DIGITS]);
    print_slash(_colors[COLOR_COLON]);
    print_num(calendar_time.month, _colors[COLOR_DIGITS]);
    print_slash(_colors[COLOR_COLON]);
    print_num(calendar_time.day, _colors[COLOR_DIGITS]);

    matrix.show();
}
