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
