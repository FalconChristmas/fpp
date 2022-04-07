// Compute times of sunise and sunset at a specified latitude and longitude.
//
// This software minimizes computational work by performing the full calculation
// of the solar position three times, at the beginning, middle, and end of the
// period of interest.  Three point interpolation is used to predict the position
// for each hour, and the arithmetic mean is used to predict the half-hour positions.
//
// The full computational burden is negligible on modern computers, but the
// algorithm is effective and still useful for small embedded systems.
//
// This software was originally adapted to javascript by Stephen R. Schmitt
// from a BASIC program from the 'Astronomical Computing' column of Sky & Telescope,
// August 1994, page 84, written by Roger W. Sinnott.
//
// Subsequently adapted from Stephen R. Schmitt's javascript to c++ for the Arduino
// by Cyrus Rahman, this work is subject to Stephen Schmitt's copyright:
//
// Copyright 2007 Stephen R. Schmitt  
// Subsequent work Copyright 2020 Cyrus Rahman
// You may use or modify this source code in any way you find useful, provided
// that you agree that the author(s) have no warranty, obligations or liability.  You
// must determine the suitability of this source code for your use.
//
// Redistributions of this source code must retain this copyright notice.

#include <math.h>
#include "SunRise.h"

#define K1 15*(M_PI/180)*1.0027379

struct skyCoordinates {
  double RA;		    // Right ascension
  double declination;	    // Declination
};

// Determine the nearest sun rise or set event previous, and the nearest
// sun rise or set event subsequent, to the specified time in seconds since the
// Unix epoch (January 1, 1970) and at the specified latitude and longitude in
// degrees.
//
// We look for events from SR_WINDOW/2 hours in the past to SR_WINDOW/2 hours
// in the future.
void
SunRise::calculate(double latitude, double longitude, time_t t, TwilightType type) {
  skyCoordinates sunPosition[3];
  double offsetDays;

  initClass();
  queryTime = t;
  offsetDays = julianDate(t) - 2451545L;     // Days since Jan 1, 2000, 1200UTC.
  // Begin testing (SR_WINDOW / 2) hours before requested time.
  offsetDays -= (double)SR_WINDOW / (2 * 24) ;	

  // Calculate coordinates at start, middle, and end of search period.
  for (int i = 0; i < 3; i ++) {
    sunPosition[i] = sun(offsetDays + i * (double)SR_WINDOW / (2 * 24));
  }

  // If the RA wraps around during this period, unwrap it to keep the
  // sequence smooth for interpolation.
  if (sunPosition[1].RA <= sunPosition[0].RA)
    sunPosition[1].RA += 2 * M_PI;
  if (sunPosition[2].RA <= sunPosition[1].RA)
    sunPosition[2].RA += 2 * M_PI;

  // Initialize interpolation array.
  skyCoordinates spWindow[3];
  spWindow[0].RA  = sunPosition[0].RA;
  spWindow[0].declination = sunPosition[0].declination;

  for (int k = 0; k < SR_WINDOW; k++) {	    // Check each interval of search period
    float ph = (float)(k + 1)/SR_WINDOW;
        
    spWindow[2].RA = interpolate(sunPosition[0].RA,
				 sunPosition[1].RA,
				 sunPosition[2].RA, ph);
    spWindow[2].declination = interpolate(sunPosition[0].declination,
					  sunPosition[1].declination,
					  sunPosition[2].declination, ph);

    // Look for sunrise/set events during this interval.
    testSunRiseSet(k, offsetDays, latitude, longitude, spWindow, type);

    spWindow[0] = spWindow[2];		    // Advance to next interval.
  }
}

// Look for sun rise or set events during an hour.
void
SunRise::testSunRiseSet(int k, double offsetDays, double latitude, double longitude,
			skyCoordinates *sp, TwilightType type) {
  double ha[3], VHz[3];
  double lSideTime;

  // Get (local_sidereal_time - SR_WINDOW / 2) hours in radians.
  lSideTime = localSiderealTime(offsetDays, longitude) * 2* M_PI / 360;

  // Calculate Hour Angle.
  ha[0] = lSideTime - sp[0].RA + k*K1;
  ha[2] = lSideTime - sp[2].RA + k*K1 + K1;

  // Hour Angle and declination at half hour.
  ha[1]  = (ha[2] + ha[0])/2;
  sp[1].declination = (sp[2].declination + sp[0].declination)/2;

  double s = sin(M_PI / 180 * latitude);
  double c = cos(M_PI / 180 * latitude);

  // refraction + sun semidiameter at horizon
  double z = 0;

  if (type == TT_ASTRONOMICAL)
    z = cos(M_PI / 180 * 108.0);
  else if (type == TT_NAUTICAL)
    z = cos(M_PI / 180 * 102.0);
  else if (type == TT_CIVIL)
    z = cos(M_PI / 180 * 96.0);
  else
    z = cos(M_PI / 180 * 90.833);

  VHz[0] = s * sin(sp[0].declination) + c * cos(sp[0].declination) * cos(ha[0]) - z;
  VHz[2] = s * sin(sp[2].declination) + c * cos(sp[2].declination) * cos(ha[2]) - z;

  if (signbit(VHz[0]) == signbit(VHz[2]))
    goto noevent;			    // No event this hour.
    
  VHz[1] = s * sin(sp[1].declination) + c * cos(sp[1].declination) * cos(ha[1]) - z;

  double a, b, d, e, time;
  a = 2 * VHz[2] - 4 * VHz[1] + 2 * VHz[0];
  b = 4 * VHz[1] - 3 * VHz[0] - VHz[2];
  d = b * b - 4 * a * VHz[0];

  if (d < 0)
    goto noevent;			    // No event this hour.
    
  d = sqrt(d);
  e = (-b + d) / (2 * a);
  if ((e < 0) || (e > 1))
    e = (-b - d) / (2 * a);
  time = k + e + 1 / 120;	    // Time since k=0 of event (in hours).

  // The time we started searching + the time from the start of the search to the
  // event is the time of the event.  Add (time since k=0) - window/2 hours.
  time_t eventTime;
  eventTime = queryTime + (time - SR_WINDOW / 2) *60 *60;

  double hz, nz, dz, az;
  hz = ha[0] + e * (ha[2] - ha[0]);	    // Azimuth of the sun at the event.
  nz = -cos(sp[1].declination) * sin(hz);
  dz = c * sin(sp[1].declination) - s * cos(sp[1].declination) * cos(hz);
  az = atan2(nz, dz) / (M_PI / 180);
  if (az < 0)
    az += 360;

  // If there is no previously recorded event of this type, save this event.
  //
  // If this event is previous to queryTime, and is the nearest event to queryTime
  // of events of its type previous to queryType, save this event, replacing the
  // previously recorded event of its type.  Events subsequent to queryTime are
  // treated similarly, although since events are tested in chronological order
  // no replacements will occur as successive events will be further from
  // queryTime.
  //
  // If this event is subsequent to queryTime and there is an event of its type
  // previous to queryTime, then there is an event of the other type between the
  // two events of this event's type.  If the event of the other type is
  // previous to queryTime, then it is the nearest event to queryTime that is
  // previous to queryTime.  In this case save the current event, replacing
  // the previously recorded event of its type.  Otherwise discard the current
  // event.
  //
  if ((VHz[0] < 0) && (VHz[2] > 0)) {
    if (!hasRise ||
	(signbit(riseTime - queryTime) == signbit(eventTime - queryTime) &&
	 fabs(riseTime - queryTime) > fabs(eventTime - queryTime)) ||
	(signbit(riseTime - queryTime) != signbit(eventTime - queryTime) &&
	 (hasSet && 
	  signbit(riseTime - queryTime) == signbit(setTime - queryTime)))) {
      riseTime = eventTime;
      riseAz = az;
      hasRise = true;
    }
  }
  if ((VHz[0] > 0) && (VHz[2] < 0)) {
    if (!hasSet ||
	(signbit(setTime - queryTime) == signbit(eventTime - queryTime) &&
	 fabs(setTime - queryTime) > fabs(eventTime - queryTime)) ||
	(signbit(setTime - queryTime) != signbit(eventTime - queryTime) &&
	 (hasRise && 
	  signbit(setTime - queryTime) == signbit(riseTime - queryTime)))) {
      setTime = eventTime;
      setAz = az;
      hasSet = true;
    }
  }

noevent:
  // There are obscure cases in the polar regions that require extra logic.
  if (!hasRise && !hasSet)
    isVisible = !signbit(VHz[2]);
  else if (hasRise && !hasSet)
    isVisible = (queryTime > riseTime);
  else if (!hasRise && hasSet)
    isVisible = (queryTime < setTime);
  else
    isVisible = ((riseTime < setTime && riseTime < queryTime && setTime > queryTime) ||
		 (riseTime > setTime && (riseTime < queryTime || setTime > queryTime)));

  return;
}

// Sun position using fundamental arguments
// (Van Flandern & Pulkkinen, 1979)
skyCoordinates
SunRise::sun(double dayOffset) {
  double centuryOffset = dayOffset / 36525 + 1;	      // Centuries from 1900.0

  double l = 0.779072 + 0.00273790931 * dayOffset;
  double g = 0.993126 + 0.00273777850 * dayOffset;

  l = 2 * M_PI * (l - floor(l));
  g = 2 * M_PI * (g - floor(g));
    
  double v, u, w;
  v = 0.39785 * sin(l)
    - 0.01000 * sin(l - g)
    + 0.00333 * sin(l + g)
    - 0.00021 * centuryOffset * sin(l);
    
  u = 1
    - 0.03349 * cos(g)
    - 0.00014 * cos(2*l)
    + 0.00008 * cos(l);
    
  w = -0.00010
     - 0.04129 * sin(2*l)
     + 0.03211 * sin(g)
     + 0.00104 * sin(2*l - g)
     - 0.00035 * sin(2*l + g)
     - 0.00008 * centuryOffset * sin(g);
    
  double s;
  skyCoordinates sc;
  s = w / sqrt(u - v*v);			    // Right ascension  
  sc.RA = l + atan(s / sqrt(1 - s*s));

  s = v / sqrt(u);				    // Declination
  sc.declination = atan(s / sqrt(1 - s*s));
  return(sc);
}

// 3-point interpolation
double
SunRise::interpolate(double f0, double f1, double f2, double p) {
    double a = f1 - f0;
    double b = f2 - f1 - a;
    return(f0 + p * (2*a + b * (2*p - 1)));
}

// Determine Julian date from Unix time.
// Provides marginally accurate results with Arduino 4-byte double.
double
SunRise::julianDate(time_t t) {
  return (t / 86400.0L + 2440587.5);
}

#if __ISO_C_VISIBLE < 1999
// Arduino compiler is missing this function as of 6/2020.
//
// The Arduino ATmega platforms (including the Uno) are also missing rint().
// This can be worked around by inserting "#define rint(x) (double)lrint(x)" here,
// but since these platforms use only four bytes for double precision - which is
// insufficient for the correct performance of the required calculations - you
// should instead upgrade to an Arduino Due or better.
//
#define remainder(x, y) ((double)((double)x - (double)y * rint((double)x / (double)y)))

// double remainder(double x, double y) {
//   return(x - (y * rint(x / y)));
// }
#endif

// Local Sidereal Time
// Provides local sidereal time in degrees, requires longitude in degrees
// and time in fractional Julian days since Jan 1, 2000, 1200UTC (e.g. the
// Julian date - 2451545).
// cf. USNO Astronomical Almanac and
// https://astronomy.stackexchange.com/questions/24859/local-sidereal-time
double
SunRise::localSiderealTime(double offsetDays, double longitude) {
  double lSideTime = (15.0L * (6.697374558L + 0.06570982441908L * offsetDays +
			       remainder(offsetDays, 1) * 24 + 12 +
			       0.000026 * (offsetDays / 36525) * (offsetDays / 36525))
		      + longitude) / 360;
  lSideTime -= floor(lSideTime);
  lSideTime *= 360;			  // Convert to degrees.
  return(lSideTime);
}

// Class initialization.
void
SunRise::initClass() {
  queryTime = 0;
  riseTime = 0;
  setTime = 0;
  riseAz = 0;
  setAz = 0;
  hasRise = false;
  hasSet = false;
  isVisible = false;
}
