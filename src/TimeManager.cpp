
// // ==================================================
// // File: src/TimeManager.cpp
// // ==================================================
// Helper function to calculate the last Sunday of a given month and year

#include "TimeManager.h"
#include "Globals.h"

int lastSundayOfMonth(int year, int month)
{
    tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = 31;
    mktime(&t);
    // t.tm_wday: 0=Sunday, 6=Saturday
    return 31 - t.tm_wday;
}
// Add these at the top of TimeManager.cpp (outside any function)
int currentDay = 1;
int currentMonth = 1;
int Hours = 0;
int Minutes = 0;
int nextDay = 2;
bool asBeenSaved = false;

/**************************************************
 *   Function to check if it is daylight saving   *
 *                 time or not                    *
 *                   start                        *
 **************************************************/
WiFiUDP ntpUDP;
const int UTC_OFFSET_STANDARD_VALUE = 0 * 3600;
const int UTC_OFFSET_DST_VALUE = 1 * 3600;
NTPClient timeClient(ntpUDP, "pool.ntp.org", UTC_OFFSET_STANDARD_VALUE, 60000);

bool isDST(int day, int month, int hour)
{
  int yearNow = year();
    if (month < 3 || month > 10)
        return false;
    if (month > 3 && month < 10)
        return true;
    int lastSundayMarch = lastSundayOfMonth(yearNow, 3);
    int lastSundayOctober = lastSundayOfMonth(yearNow, 10);
    if (month == 3)
        return (day > lastSundayMarch || (day == lastSundayMarch && hour >= 1));
    if (month == 10)
        return (day < lastSundayOctober || (day == lastSundayOctober && hour < 2));
    return false;
}
/**************************************************
 *   Function to check if it is daylight saving   *
 *                 time or not                    *
 *                  end                           *
 **************************************************/

/***************************************
    int yearNow = year();
 *   function to get the time          *
 *           start                     *
 ****************************************/
void getTime()
{
    timeClient.update();
    time_t epochTime = timeClient.getEpochTime();
    int tempDay = day(epochTime);
    int tempMonth = month(epochTime);
    int tempHour = hour(epochTime);

    bool dstActive = isDST(tempDay, tempMonth, tempHour);
    int offset = dstActive ? UTC_OFFSET_DST_VALUE : UTC_OFFSET_STANDARD_VALUE;
    timeClient.setTimeOffset(offset);
    timeClient.update();
    epochTime = timeClient.getEpochTime();

    Hours = hour(epochTime);
    Minutes = minute(epochTime);
    int currentSecond = second(epochTime);
    currentDay = day(epochTime);
    currentMonth = month(epochTime);
    int currentYear = year(epochTime);
    if (currentDay == nextDay)
    {
        if (currentMonth == 2) // February
        {
            // Check for leap year
            if ((year() % 4 == 0 && year() % 100 != 0) || (year() % 400 == 0)) // Leap year
            {
                if (currentDay > 28)
                {
                    asBeenSaved = true;
                    nextDay = 1; // Reset nextDay to 1 for the next month
                }
            }
            else // Non-leap year
            {
                if (currentDay > 27)
                {
                    asBeenSaved = true;
                    nextDay = 1; // Reset nextDay to 1 for the next month
                }
            }
        }
        else if (currentMonth == 4 || currentMonth == 6 || currentMonth == 9 || currentMonth == 11) // Months with 30 days
        {
            if (currentDay > 29)
            {
                asBeenSaved = true;
                nextDay = 1; // Reset nextDay to 1 for the next month
            }
        }
        else // Months with 31 days
        {
            if (currentDay > 30)
            {
                asBeenSaved = true;
                nextDay = 1; // Reset nextDay to 1 for the next month
            }
        }

        // Reset to January if the month exceeds December
        if (currentMonth > 12)
        {
            currentMonth = 1;
        }
        if (asBeenSaved == false)
        {
            asBeenSaved = true;
            nextDay++; // Increment nextDay for the next day
        }
    }
}

/***************************************
 *   function to get the time          *
 *           end                       *
 ***************************************/

String getFormattedTime()
{
    char timeBuffer[10];
    sprintf(timeBuffer, "%02d:%02d", Hours, Minutes); // Always 2 digits for hours
    return String(timeBuffer);
}

String getFormattedDate()
{
    time_t epochTime = timeClient.getEpochTime();
    char dateBuffer[20];
    sprintf(dateBuffer, "%d/%d/%d", currentDay, currentMonth, year(epochTime));
    return String(dateBuffer);
}
