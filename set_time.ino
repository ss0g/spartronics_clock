/**
 * Set time: 1st get time from computer, 2nd write to RTC
 * Note: this code could be simplified by switching use of Serial.parseInt()
 */
time_t set_time()
{
    // prompt user for time and set it on the board
    _state = STATE_MESSAGE;
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
        _state = STATE_MESSAGE;
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
