

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
 * Note: this code could be simplified by switching use of Serial.parseInt()
 */
time_t set_time()
{
    tmElements_t tm;
    char time_string[32];

    // prompt user for time and set it on the board
    message_print("SET TIME", COLOR_RED);

    // read & parse time from the user
    Serial.println("Make sure Serial Terminal is set to Newline");
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

    // NOTE: sscanf can be replaced w/ Serial.parseInt()
    size_t num_chars = get_string_from_serial(time_string, sizeof(time_string));
    Serial.print("Received ");
    Serial.print(num_chars);
    Serial.print(" characters: ");
    Serial.println(time_string);

    time_t t;

    CalendarTime_t set_time;
    if (sscanf(time_string, "%04u:%02u:%02u:%02u:%02u:%02u",
                &set_time.year, &set_time.month, &set_time.day,
                &set_time.hour, &set_time.minute, &set_time.second) != 6)
    {
        Serial.println("Error: problem parsing YYYY:MM:DD:hh:mm:ss");
        return 0;
    }
    t = convert_time(set_time);

    // set RTC time
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
