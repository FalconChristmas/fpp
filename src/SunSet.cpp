/*
The MIT License (MIT)

Copyright (c) 2015 Peter Buelow (goballstate at gmail)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "SunSet.h"

SunSet::SunSet()
{
  latitude = 0.0;
  longitude = 0.0;
  julianDate = 0.0;
  tzOffset = 0;
}

SunSet::SunSet(double lat, double lon, int tz)
{
  latitude = lat;
  longitude = lon;
  julianDate = 0.0;
  tzOffset = tz;
}

SunSet::~SunSet()
{

}

void SunSet::setPosition(double lat, double lon, int tz)
{
    latitude = lat;
    longitude = lon;
    tzOffset = tz;
}

double SunSet::degToRad(double angleDeg)
{
  return (M_PI * angleDeg / 180.0);
}

double SunSet::radToDeg(double angleRad)
{
  return (180.0 * angleRad / M_PI);
}

double SunSet::calcMeanObliquityOfEcliptic(double t)
{
  double seconds = 21.448 - t*(46.8150 + t*(0.00059 - t*(0.001813)));
  double e0 = 23.0 + (26.0 + (seconds/60.0))/60.0;

  return e0;              // in degrees
}

double SunSet::calcGeomMeanLongSun(double t)
{
  double L = 280.46646 + t * (36000.76983 + 0.0003032 * t);

  while ((int) L > 360) {
    L -= 360.0;
  }

  while (L <  0) {
    L += 360.0;
  }

  return L;              // in degrees
}

double SunSet::calcObliquityCorrection(double t)
{
  double e0 = calcMeanObliquityOfEcliptic(t);
  double omega = 125.04 - 1934.136 * t;
  double e = e0 + 0.00256 * cos(degToRad(omega));

  return e;               // in degrees
}

double SunSet::calcEccentricityEarthOrbit(double t)
{
  double e = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
  return e;               // unitless
}

double SunSet::calcGeomMeanAnomalySun(double t)
{
  double M = 357.52911 + t * (35999.05029 - 0.0001537 * t);
  return M;               // in degrees
}

double SunSet::calcEquationOfTime(double t)
{
  double epsilon = calcObliquityCorrection(t);
  double l0 = calcGeomMeanLongSun(t);
  double e = calcEccentricityEarthOrbit(t);
  double m = calcGeomMeanAnomalySun(t);
  double y = tan(degToRad(epsilon)/2.0);

  y *= y;

  double sin2l0 = sin(2.0 * degToRad(l0));
  double sinm   = sin(degToRad(m));
  double cos2l0 = cos(2.0 * degToRad(l0));
  double sin4l0 = sin(4.0 * degToRad(l0));
  double sin2m  = sin(2.0 * degToRad(m));
  double Etime = y * sin2l0 - 2.0 * e * sinm + 4.0 * e * y * sinm * cos2l0 - 0.5 * y * y * sin4l0 - 1.25 * e * e * sin2m;
  return radToDeg(Etime)*4.0;	// in minutes of time
}

double SunSet::calcTimeJulianCent(double jd)
{
  double T = ( jd - 2451545.0)/36525.0;
  return T;
}

double SunSet::calcSunTrueLong(double t)
{
  double l0 = calcGeomMeanLongSun(t);
  double c = calcSunEqOfCenter(t);

  double O = l0 + c;
  return O;               // in degrees
}

double SunSet::calcSunApparentLong(double t)
{
  double o = calcSunTrueLong(t);

  double  omega = 125.04 - 1934.136 * t;
  double  lambda = o - 0.00569 - 0.00478 * sin(degToRad(omega));
  return lambda;          // in degrees
}

double SunSet::calcSunDeclination(double t)
{
  double e = calcObliquityCorrection(t);
  double lambda = calcSunApparentLong(t);

  double sint = sin(degToRad(e)) * sin(degToRad(lambda));
  double theta = radToDeg(asin(sint));
  return theta;           // in degrees
}

double SunSet::calcHourAngleSunrise(double lat, double solarDec)
{
  double latRad = degToRad(lat);
  double sdRad  = degToRad(solarDec);
  double HA = (acos(cos(degToRad(90.833))/(cos(latRad)*cos(sdRad))-tan(latRad) * tan(sdRad)));

  return HA;              // in radians
}

double SunSet::calcHourAngleSunset(double lat, double solarDec)
{
  double latRad = degToRad(lat);
  double sdRad  = degToRad(solarDec);
  double HA = (acos(cos(degToRad(90.833))/(cos(latRad)*cos(sdRad))-tan(latRad) * tan(sdRad)));

  return -HA;              // in radians
}

double SunSet::calcJD(int y, int m, int d)
{
  if (m <= 2) {
    y -= 1;
    m += 12;
  }
  int A = floor(y/100);
  int B = 2 - A + floor(A/4);

  double JD = floor(365.25*(y + 4716)) + floor(30.6001*(m+1)) + d + B - 1524.5;
  return JD;
}

double SunSet::calcJDFromJulianCent(double t)
{
  double JD = t * 36525.0 + 2451545.0;
  return JD;
}

double SunSet::calcSunEqOfCenter(double t)
{
  double m = calcGeomMeanAnomalySun(t);
  double mrad = degToRad(m);
  double sinm = sin(mrad);
  double sin2m = sin(mrad+mrad);
  double sin3m = sin(mrad+mrad+mrad);
  double C = sinm * (1.914602 - t * (0.004817 + 0.000014 * t)) + sin2m * (0.019993 - 0.000101 * t) + sin3m * 0.000289;

  return C;		// in degrees
}

double SunSet::calcSunriseUTC()
{
  double t = calcTimeJulianCent(julianDate);
  // *** First pass to approximate sunrise
  double  eqTime = calcEquationOfTime(t);
  double  solarDec = calcSunDeclination(t);
  double  hourAngle = calcHourAngleSunrise(latitude, solarDec);
  double  delta = longitude + radToDeg(hourAngle);
  double  timeDiff = 4 * delta;	// in minutes of time
  double  timeUTC = 720 - timeDiff - eqTime;	// in minutes
  double  newt = calcTimeJulianCent(calcJDFromJulianCent(t) + timeUTC/1440.0);

  eqTime = calcEquationOfTime(newt);
  solarDec = calcSunDeclination(newt);

  hourAngle = calcHourAngleSunrise(latitude, solarDec);
  delta = longitude + radToDeg(hourAngle);
  timeDiff = 4 * delta;
  timeUTC = 720 - timeDiff - eqTime; // in minutes

  return timeUTC;
}

double SunSet::calcSunrise()
{
  double t = calcTimeJulianCent(julianDate);
  // *** First pass to approximate sunrise
  double  eqTime = calcEquationOfTime(t);
  double  solarDec = calcSunDeclination(t);
  double  hourAngle = calcHourAngleSunrise(latitude, solarDec);
  double  delta = longitude + radToDeg(hourAngle);
  double  timeDiff = 4 * delta;	// in minutes of time
  double  timeUTC = 720 - timeDiff - eqTime;	// in minutes
  double  newt = calcTimeJulianCent(calcJDFromJulianCent(t) + timeUTC/1440.0);

  eqTime = calcEquationOfTime(newt);
  solarDec = calcSunDeclination(newt);

  hourAngle = calcHourAngleSunrise(latitude, solarDec);
  delta = longitude + radToDeg(hourAngle);
  timeDiff = 4 * delta;
  timeUTC = 720 - timeDiff - eqTime; // in minutes

  double localTime = timeUTC + (60 * tzOffset);

  return localTime;	// return time in minutes from midnight
}

double SunSet::calcSunsetUTC()
{
  double t = calcTimeJulianCent(julianDate);
  // *** First pass to approximate sunset
  double  eqTime = calcEquationOfTime(t);
  double  solarDec = calcSunDeclination(t);
  double  hourAngle = calcHourAngleSunset(latitude, solarDec);
  double  delta = longitude + radToDeg(hourAngle);
  double  timeDiff = 4 * delta;	// in minutes of time
  double  timeUTC = 720 - timeDiff - eqTime;	// in minutes
  double  newt = calcTimeJulianCent(calcJDFromJulianCent(t) + timeUTC/1440.0);

  eqTime = calcEquationOfTime(newt);
  solarDec = calcSunDeclination(newt);

  hourAngle = calcHourAngleSunset(latitude, solarDec);
  delta = longitude + radToDeg(hourAngle);
  timeDiff = 4 * delta;
  timeUTC = 720 - timeDiff - eqTime; // in minutes

  return timeUTC;	// return time in minutes from midnight
}

double SunSet::calcSunset()
{
  double t = calcTimeJulianCent(julianDate);
  // *** First pass to approximate sunset
  double  eqTime = calcEquationOfTime(t);
  double  solarDec = calcSunDeclination(t);
  double  hourAngle = calcHourAngleSunset(latitude, solarDec);
  double  delta = longitude + radToDeg(hourAngle);
  double  timeDiff = 4 * delta;	// in minutes of time
  double  timeUTC = 720 - timeDiff - eqTime;	// in minutes
  double  newt = calcTimeJulianCent(calcJDFromJulianCent(t) + timeUTC/1440.0);

  eqTime = calcEquationOfTime(newt);
  solarDec = calcSunDeclination(newt);

  hourAngle = calcHourAngleSunset(latitude, solarDec);
  delta = longitude + radToDeg(hourAngle);
  timeDiff = 4 * delta;
  timeUTC = 720 - timeDiff - eqTime; // in minutes
  double localTime = timeUTC + (60 * tzOffset);

  return localTime;	// return time in minutes from midnight
}

double SunSet::setCurrentDate(int y, int m, int d)
{
	m_year = y;
	m_month = m;
	m_day = d;
	julianDate = calcJD(y, m, d);
	return julianDate;
}

void SunSet::setTZOffset(int tz)
{
	tzOffset = tz;
}

int SunSet::moonPhase(int fromepoch)
{
	int moonepoch = 614100;

        int phase = (fromepoch - moonepoch) % 2551443;
        int res = floor(phase / (24 * 3600)) + 1;
	if (res == 30)
		res = 0;

        return res;
}

