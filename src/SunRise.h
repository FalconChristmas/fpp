#ifndef SunRise_h
#define SunRise_h

#include <string>
#include <time.h>

// Size of event search window in hours.
// Events further away from the search time than MR_WINDOW/2 will not be
// found.  At higher latitudes the sun rise/set intervals become larger, so if
// you want to find the nearest events this will need to increase.  Larger
// windows will increase interpolation error.  Useful values are probably from
// 12 - 48 but will depend upon your application.

#define SR_WINDOW   24	    // Even integer

typedef enum {
    TT_OFFICIAL,
    TT_CIVIL,
    TT_NAUTICAL,
    TT_ASTRONOMICAL
} TwilightType;

class SunRise {
  public:
    time_t queryTime;
    time_t riseTime;
    time_t setTime;
    float riseAz;
    float setAz;
    bool hasRise;
    bool hasSet;
    bool isVisible;

    void calculate(double latitude, double longitude, time_t t, TwilightType type = TT_OFFICIAL);

  private:
    void testSunRiseSet(int i, double offsetDays, double latitude, double longitude,
			struct skyCoordinates *mp, TwilightType type = TT_OFFICIAL);
    struct skyCoordinates sun(double dayOffset);
    double interpolate(double f0, double f1, double f2, double p);
    double julianDate(time_t t);
    double localSiderealTime(double offsetDays, double longitude);
    void initClass();
};
#endif
