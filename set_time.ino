

time_t get_time_from_rtc(void)
{
    time_t t;

    t = RTC.get();
    setTime(t);

    return t;
}

bool save_time_to_rtc(void)
{
    return RTC.set(now());
}

void print2digits(int number)
{
    if (number >= 0 && number < 10)
    {
        Serial.write('0');
    }
    Serial.print(number);
}

/**
 * Set time: 1st get time from computer, 2nd write to RTC
 */
time_t set_time()
{
    tmElements_t tm;
    char time_string[32];

    // Check if the serial port is connected and available
    if (!Serial)
    {
        return 0;
    }

    // Put a message on the clock to indicate we are in time set mode
    message_print("SET TIME", COLOR_RED);

    // Drain all of the old bytes out of the serial input buffer
    while (Serial.available() > 0)
    {
        Serial.read();
    }

    // Print out a header to prompt the user for time data
    Serial.println("\r\nMake sure Serial Terminal is set to Newline");
    // Print out the current time
    if (RTC.read(tm))
    {
        Serial.print("Current time: ");
        Serial.print(tmYearToCalendar(tm.Year));
        Serial.write(':');
        print2digits(tm.Month);
        Serial.write(':');
        print2digits(tm.Day);
        Serial.write(':');
        print2digits(tm.Hour);
        Serial.write(':');
        print2digits(tm.Minute);
        Serial.write(':');
        print2digits(tm.Second);
        Serial.println();
    }
    else
    {
        Serial.println("Error reading RTC!");
    }
    Serial.println("Enter today's date and time: YYYY:MM:DD:hh:mm:ss");

    // Read the time string from the user
    size_t num_chars = get_string_from_serial(time_string, sizeof(time_string));
    Serial.print("Received ");
    Serial.print(num_chars);
    Serial.print(" characters: ");
    Serial.println(time_string);

    // Convert the string representation to the time value
    time_t t;
    CalendarTime_t the_time;
    if (sscanf(time_string, "%04u:%02u:%02u:%02u:%02u:%02u",
                &the_time.year, &the_time.month, &the_time.day,
                &the_time.hour, &the_time.minute, &the_time.second) != 6)
    {
        Serial.println("Error: problem parsing YYYY:MM:DD:hh:mm:ss");
        return 0;
    }
    t = convert_time(the_time);

    // Set the RTC (hardware clock) time
    setTime(t);                 // Set the internal clock
    if (save_time_to_rtc() != true)
    {
        Serial.println("Error: RTC.set failed.");
    }

    return t;
}

static const char * const _valid_chars = "0123456789:";

// code from: http: //forum.arduino.cc/index.php?topic=396450
size_t get_string_from_serial(char *the_string, size_t string_size)
{
    bool done = false;
    size_t idx = 0;
    char ch;

    if (string_size == 0)
    {
        return 0;
    }

    // Save one byte for the string terminator
    string_size--;

    time_t timeout = now() + 30;

    while (!done)
    {
        if (Serial.available() > 0)
        {
            ch = Serial.read();
            if ((idx < string_size) &&
                    (strchr(_valid_chars, ch) != NULL))
            {
                the_string[idx++] = ch;
            }

            if ((ch == '\n') || (ch == '\r'))
            {
                // We found the end-of-line
                done = true;
            }
        }

        if (now() > timeout)
        {
            break;
        }
    }

    // Terminate the string
    the_string[idx] = '\0';

    return idx;
}
