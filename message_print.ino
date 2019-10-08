// standard pixel size for Adafruit_GFX is 6px per character
static const int _pixel_size = 6;
static int _x = matrix.width();
static ColorName_t _message_color;
static bool _message_finished;
static const char *_message;
static int _message_width;

/**
 *  Setup a message to be scrolled
 */
void message_start(const char *str, ColorName_t color)
{
    // Save the message string
    if (str != NULL)
    {
        _message = str;
        _message_width = (strlen(str) * _pixel_size);
        _message_color = color;
    }
    _message_finished = false;
    _x = matrix.width();
}

/**
 * Display a scrolling message
 */
void message_scroll(void)
{
    if (_message_finished == true)
    {
        return;
    }

    matrix.clear();             // Clear the screen
    matrix.setCursor(_x, 0);    // Set the starting point of the message
    matrix.setTextColor(_colors[_message_color]);
    matrix.print(_message);     // Store the message at the cursor position
    /**
     * setCursor() sets the pixel position from where to render the text
     * offset is calculated based on font width, screen width, and text length
     * when offset is reached, pass is complete & color is updated
    **/
    // int offset = _message_width - _x;
    int offset = _message_width;

    Serial.println(matrix.width());

    _x = _x - 1;        // Set the x-axis one pixel to the left

    // if (_x < (0 - _message_width))
    if (_x < -offset)
    {
        // After one time through scrolling the message, set the "done" flag
        _message_finished = true;
    }

    matrix.show();              // Display the message as saved
}

/**
 *  Indicate that a message has scrolled off the screen
 */
bool message_done(void)
{
    return _message_finished;
}

/**
 *  Print a static message on the display (not scrolled)
 */
void message_print(const char *str)
{
    matrix.clear();
    matrix.setCursor(0, 0);     /* Is this correct? or matrix.width()? */
    matrix.print(str);
    matrix.show();
}

