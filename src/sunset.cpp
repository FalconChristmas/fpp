/*
 * Provides the ability to calculate the local time for sunrise,
 * sunset, and moonrise at any point in time at any location in the world
 *
 * Original work used with permission maintaining license
 * Copyright (GPL) 2004 Mike Chirico mchirico@comcast.net
 * Modifications copyright
 * Copyright (GPL) 2015 Peter Buelow
 *
 * This file is part of the Sunset library
 *
 * Sunset is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Sunset is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "sunset.h"

/**
 * \fn SunSet::SunSet()
 * 
 * Default constructor taking no arguments. It will default all values
 * to zero. It is possible to call calcSunrise() on this type of initialization
 * and it will not fail, but it is unlikely you are at 0,0, TZ=0. This also
 * will not include an initialized date to work from.
 */
SunSet::SunSet() : m_latitude(0.0), m_longitude(0.0), m_julianDate(0.0), m_tzOffset(0.0)
{
}

/**
 * \fn SunSet::SunSet(double lat, double lon, int tz)
 * \param lat Double Latitude for this object
 * \param lon Double Longitude for this object
 * \param tz Integer based timezone for this object
 * 
 * This will create an object for a location with an integer based
 * timezone value. This constructor is a relic of the original design. 
 * It is not deprecated, as this is a valid construction, but the double is
 * preferred for correctness.
 */
SunSet::SunSet(double lat, double lon, int tz) : m_latitude(lat), m_longitude(lon), m_julianDate(0.0), m_tzOffset(tz)
{
}

/**
 * \fn SunSet::SunSet(double lat, double lon, double tz)
 * \param lat Double Latitude for this object
 * \param lon Double Longitude for this object
 * \param tz Double based timezone for this object
 * 
 * This will create an object for a location with a double based
 * timezone value.
 */
SunSet::SunSet(double lat, double lon, double tz) : m_latitude(lat), m_longitude(lon), m_julianDate(0.0), m_tzOffset(tz)
{
}

/**
 * \fn SunSet::~SunSet()
 * 
 * The constructor has no value and does nothing.
 */
SunSet::~SunSet()
{
}

/**
 * \fn void SunSet::setPosition(double lat, double lon, int tz)
 * \param lat Double Latitude value
 * \param lon Double Longitude value
 * \param tz Integer Timezone offset
 * 
 * This will set the location the library uses for it's math. The
 * timezone is included in this as it's not valid to call
 * any of the calc functions until you have set a timezone.
 * It is possible to simply call setPosition one time, with a timezone
 * and not use the setTZOffset() function ever, if you never
 * change timezone values.
 * 
 * This is the old version of the setPosition using an integer
 * timezone, and will not be deprecated. However, it is preferred to
 * use the double version going forward.
 */
void SunSet::setPosition(double lat, double lon, int tz)
{
    m_latitude = lat;
    m_longitude = lon;
    if (tz >= -12 && tz <= 14)
        m_tzOffset = tz;
    else
        m_tzOffset = 0.0;
}

/**
 * \fn void SunSet::setPosition(double lat, double lon, double tz)
 * \param lat Double Latitude value
 * \param lon Double Longitude value
 * \param tz Double Timezone offset
 * 
 * This will set the location the library uses for it's math. The
 * timezone is included in this as it's not valid to call
 * any of the calc functions until you have set a timezone.
 * It is possible to simply call setPosition one time, with a timezone
 * and not use the setTZOffset() function ever, if you never
 * change timezone values.
 */
void SunSet::setPosition(double lat, double lon, double tz)
{
    m_latitude = lat;
    m_longitude = lon;
    if (tz >= -12 && tz <= 14)
        m_tzOffset = tz;
    else
        m_tzOffset = 0.0;
}

double SunSet::degToRad(double angleDeg) const
{
    return (M_PI * angleDeg / 180.0);
}

double SunSet::radToDeg(double angleRad) const
{
    return (180.0 * angleRad / M_PI);
}

double SunSet::calcMeanObliquityOfEcliptic(double t) const
{
    double seconds = 21.448 - t*(46.8150 + t*(0.00059 - t*(0.001813)));
    double e0 = 23.0 + (26.0 + (seconds/60.0))/60.0;

    return e0;              // in degrees
}

double SunSet::calcGeomMeanLongSun(double t) const
{
    if (std::isnan(t)) {
        return nan("");
    }
    double L = 280.46646 + t * (36000.76983 + 0.0003032 * t);

    return std::fmod(L, 360.0);
}

double SunSet::calcObliquityCorrection(double t) const
{
    double e0 = calcMeanObliquityOfEcliptic(t);
    double omega = 125.04 - 1934.136 * t;
    double e = e0 + 0.00256 * cos(degToRad(omega));

    return e;               // in degrees
}

double SunSet::calcEccentricityEarthOrbit(double t) const
{
    double e = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
    return e;               // unitless
}

double SunSet::calcGeomMeanAnomalySun(double t) const
{
    double M = 357.52911 + t * (35999.05029 - 0.0001537 * t);
    return M;               // in degrees
}

double SunSet::calcEquationOfTime(double t) const
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

double SunSet::calcTimeJulianCent(double jd) const
{
    double T = ( jd - 2451545.0)/36525.0;
    return T;
}

double SunSet::calcSunTrueLong(double t) const
{
    double l0 = calcGeomMeanLongSun(t);
    double c = calcSunEqOfCenter(t);

    double O = l0 + c;
    return O;               // in degrees
}

double SunSet::calcSunApparentLong(double t) const
{
    double o = calcSunTrueLong(t);

    double  omega = 125.04 - 1934.136 * t;
    double  lambda = o - 0.00569 - 0.00478 * sin(degToRad(omega));
    return lambda;          // in degrees
}

double SunSet::calcSunDeclination(double t) const
{
    double e = calcObliquityCorrection(t);
    double lambda = calcSunApparentLong(t);

    double sint = sin(degToRad(e)) * sin(degToRad(lambda));
    double theta = radToDeg(asin(sint));
    return theta;           // in degrees
}

double SunSet::calcHourAngleSunrise(double lat, double solarDec, double offset) const
{
    double latRad = degToRad(lat);
    double sdRad  = degToRad(solarDec);
    double HA = (acos(cos(degToRad(offset))/(cos(latRad)*cos(sdRad))-tan(latRad) * tan(sdRad)));

    return HA;              // in radians
}

double SunSet::calcHourAngleSunset(double lat, double solarDec, double offset) const
{
    double latRad = degToRad(lat);
    double sdRad  = degToRad(solarDec);
    double HA = (acos(cos(degToRad(offset))/(cos(latRad)*cos(sdRad))-tan(latRad) * tan(sdRad)));

    return -HA;              // in radians
}

/**
 * \fn double SunSet::calcJD(int y, int m, int d) const
 * \param y Integer year as a 4 digit value
 * \param m Integer month, not 0 based
 * \param d Integer day, not 0 based
 * \return Returns the Julian date as a double for the calculations
 * 
 * A well known JD calculator
 */
double SunSet::calcJD(int y, int m, int d) const
{
    if (m <= 2) {
        y -= 1;
        m += 12;
    }
    double A = floor(y/100);
    double B = 2.0 - A + floor(A/4);

    double JD = floor(365.25*(y + 4716)) + floor(30.6001*(m+1)) + d + B - 1524.5;
    return JD;
}

double SunSet::calcJDFromJulianCent(double t) const
{
    double JD = t * 36525.0 + 2451545.0;
    return JD;
}

double SunSet::calcSunEqOfCenter(double t) const
{
    double m = calcGeomMeanAnomalySun(t);
    double mrad = degToRad(m);
    double sinm = sin(mrad);
    double sin2m = sin(mrad+mrad);
    double sin3m = sin(mrad+mrad+mrad);
    double C = sinm * (1.914602 - t * (0.004817 + 0.000014 * t)) + sin2m * (0.019993 - 0.000101 * t) + sin3m * 0.000289;

    return C;		// in degrees
}

/**
 * \fn double SunSet::calcAbsSunrise(double offset) const
 * \param offset Double The specific angle to use when calculating sunrise
 * \return Returns the time in minutes past midnight in UTC for sunrise at your location
 * 
 * This does a bunch of work to get to an accurate angle. Note that it does it 2x, once
 * to get a rough position, and then it doubles back and redoes the calculations to 
 * refine the value. The first time through, it will be off by as much as 2 minutes, but
 * the second time through, it will be nearly perfect.
 * 
 * Note that this is the base calculation for all sunrise calls. The others just modify
 * the offset angle to account for the different needs.
 */
double SunSet::calcAbsSunrise(double offset) const
{
    double t = calcTimeJulianCent(m_julianDate);
    // *** First pass to approximate sunrise
    double  eqTime = calcEquationOfTime(t);
    double  solarDec = calcSunDeclination(t);
    double  hourAngle = calcHourAngleSunrise(m_latitude, solarDec, offset);
    double  delta = m_longitude + radToDeg(hourAngle);
    double  timeDiff = 4 * delta;	// in minutes of time
    double  timeUTC = 720 - timeDiff - eqTime;	// in minutes
    double  newt = calcTimeJulianCent(calcJDFromJulianCent(t) + timeUTC/1440.0);

    eqTime = calcEquationOfTime(newt);
    solarDec = calcSunDeclination(newt);

    hourAngle = calcHourAngleSunrise(m_latitude, solarDec, offset);
    delta = m_longitude + radToDeg(hourAngle);
    timeDiff = 4 * delta;
    timeUTC = 720 - timeDiff - eqTime; // in minutes
    return timeUTC;	// return time in minutes from midnight
}

/**
 * \fn double SunSet::calcAbsSunset(double offset) const
 * \param offset Double The specific angle to use when calculating sunset
 * \return Returns the time in minutes past midnight in UTC for sunset at your location
 * 
 * This does a bunch of work to get to an accurate angle. Note that it does it 2x, once
 * to get a rough position, and then it doubles back and redoes the calculations to 
 * refine the value. The first time through, it will be off by as much as 2 minutes, but
 * the second time through, it will be nearly perfect.
 *
 * Note that this is the base calculation for all sunset calls. The others just modify
 * the offset angle to account for the different needs.
*/
double SunSet::calcAbsSunset(double offset) const
{
    double t = calcTimeJulianCent(m_julianDate);
    // *** First pass to approximate sunset
    double  eqTime = calcEquationOfTime(t);
    double  solarDec = calcSunDeclination(t);
    double  hourAngle = calcHourAngleSunset(m_latitude, solarDec, offset);
    double  delta = m_longitude + radToDeg(hourAngle);
    double  timeDiff = 4 * delta;	// in minutes of time
    double  timeUTC = 720 - timeDiff - eqTime;	// in minutes
    double  newt = calcTimeJulianCent(calcJDFromJulianCent(t) + timeUTC/1440.0);

    eqTime = calcEquationOfTime(newt);
    solarDec = calcSunDeclination(newt);

    hourAngle = calcHourAngleSunset(m_latitude, solarDec, offset);
    delta = m_longitude + radToDeg(hourAngle);
    timeDiff = 4 * delta;
    timeUTC = 720 - timeDiff - eqTime; // in minutes

    return timeUTC;	// return time in minutes from midnight
}

/**
 * \fn double SunSet::calcSunriseUTC()
 * \return Returns the UTC time when sunrise occurs in the location provided
 * 
 * This is a holdover from the original implementation and to me doesn't
 * seem to be very useful, it's just confusing. This function is deprecated
 * but won't be removed unless that becomes necessary.
 */
double SunSet::calcSunriseUTC()
{
    return calcAbsSunrise(SUNSET_OFFICIAL);
}

/**
 * \fn double SunSet::calcSunsetUTC() const
 * \return Returns the UTC time when sunset occurs in the location provided
 * 
 * This is a holdover from the original implementation and to me doesn't
 * seem to be very useful, it's just confusing. This function is deprecated
 * but won't be removed unless that becomes necessary.
 */
double SunSet::calcSunsetUTC()
{
    return calcAbsSunset(SUNSET_OFFICIAL);
}

/**
 * \fn double SunSet::calcAstronomicalSunrise()
 * \return Returns the Astronomical sunrise in fractional minutes past midnight
 * 
 * This function will return the Astronomical sunrise in local time for your location
 */
double SunSet::calcAstronomicalSunrise() const
{
    return calcCustomSunrise(SUNSET_ASTONOMICAL);
}

/**
 * \fn double SunSet::calcAstronomicalSunset() const
 * \return Returns the Astronomical sunset in fractional minutes past midnight
 * 
 * This function will return the Astronomical sunset in local time for your location
 */
double SunSet::calcAstronomicalSunset() const
{
    return calcCustomSunset(SUNSET_ASTONOMICAL);
}

/**
 * \fn double SunSet::calcCivilSunrise() const
 * \return Returns the Civil sunrise in fractional minutes past midnight
 * 
 * This function will return the Civil sunrise in local time for your location
 */
double SunSet::calcCivilSunrise() const
{
    return calcCustomSunrise(SUNSET_CIVIL);
}

/**
 * \fn double SunSet::calcCivilSunset() const
 * \return Returns the Civil sunset in fractional minutes past midnight
 * 
 * This function will return the Civil sunset in local time for your location
 */
double SunSet::calcCivilSunset() const
{
    return calcCustomSunset(SUNSET_CIVIL);
}

/**
 * \fn double SunSet::calcNauticalSunrise() const
 * \return Returns the Nautical sunrise in fractional minutes past midnight
 * 
 * This function will return the Nautical sunrise in local time for your location
 */
double SunSet::calcNauticalSunrise() const
{
    return calcCustomSunrise(SUNSET_NAUTICAL);
}

/**
 * \fn double SunSet::calcNauticalSunset() const
 * \return Returns the Nautical sunset in fractional minutes past midnight
 * 
 * This function will return the Nautical sunset in local time for your location
 */
double SunSet::calcNauticalSunset() const
{
    return calcCustomSunset(SUNSET_NAUTICAL);
}

/**
 * \fn double SunSet::calcSunrise() const
 * \return Returns local sunrise in minutes past midnight.
 * 
 * This function will return the Official sunrise in local time for your location
 */
double SunSet::calcSunrise() const
{
    return calcCustomSunrise(SUNSET_OFFICIAL);
}

/**
 * \fn double SunSet::calcSunset() const
 * \return Returns local sunset in minutes past midnight.
 * 
 * This function will return the Official sunset in local time for your location
 */
double SunSet::calcSunset() const
{
    return calcCustomSunset(SUNSET_OFFICIAL);
}

/**
 * \fn double SunSet::calcCustomSunrise(double angle) const
 * \param angle The angle in degrees over the horizon at which to calculate the sunset time
 * \return Returns sunrise at angle degrees in minutes past midnight.
 * 
 * This function will return the sunrise in local time for your location for any
 * angle over the horizon, where < 90 would be above the horizon, and > 90 would be at or below.
 */
double SunSet::calcCustomSunrise(double angle) const
{
    return calcAbsSunrise(angle) + (60 * m_tzOffset);
}

/**
 * \fn double SunSet::calcCustomSunset(double angle) const
 * \param angle The angle in degrees over the horizon at which to calculate the sunset time
 * \return Returns sunset at angle degrees in minutes past midnight.
 * 
 * This function will return the sunset in local time for your location for any
 * angle over the horizon, where < 90 would be above the horizon, and > 90 would be at or below.
 */
double SunSet::calcCustomSunset(double angle) const
{
    return calcAbsSunset(angle) + (60 * m_tzOffset);
}

/**
 * double SunSet::setCurrentDate(int y, int m, int d)
 * \param y Integer year, must be 4 digits
 * \param m Integer month, not zero based (Jan = 1)
 * \param d Integer day of month, not zero based (month starts on day 1)
 * \return Returns the result of the Julian Date conversion if you want to save it
 * 
 * Since these calculations are done based on the Julian Calendar, we must convert
 * our year month day into Julian before we use it. You get the Julian value for
 * free if you want it.
 */
double SunSet::setCurrentDate(int y, int m, int d)
{
	m_year = y;
	m_month = m;
	m_day = d;
	m_julianDate = calcJD(y, m, d);
	return m_julianDate;
}

/**
 * \fn void SunSet::setTZOffset(int tz)
 * \param tz Integer timezone, may be positive or negative
 * 
 * Critical to set your timezone so results are accurate for your time and date.
 * This function is critical to make sure the system works correctly. If you
 * do not set the timezone correctly, the return value will not be correct for
 * your location. Forgetting this will result in return values that may actually
 * be negative in some cases.
 * 
 * This function is a holdover from the previous design using an integer timezone
 * and will not be deprecated. It is preferred to use the setTZOffset(doubble).
 */
void SunSet::setTZOffset(int tz)
{
    if (tz >= -12 && tz <= 14)
        m_tzOffset = static_cast<double>(tz);
    else
        m_tzOffset = 0.0;
}

/**
 * \fn void SunSet::setTZOffset(double tz)
 * \param tz Double timezone, may be positive or negative
 * 
 * Critical to set your timezone so results are accurate for your time and date.
 * This function is critical to make sure the system works correctly. If you
 * do not set the timezone correctly, the return value will not be correct for
 * your location. Forgetting this will result in return values that may actually
 * be negative in some cases.
 */
void SunSet::setTZOffset(double tz)
{
    if (tz >= -12 && tz <= 14)
        m_tzOffset = tz;
    else
        m_tzOffset = 0.0;
}

/**
 * \fn int SunSet::moonPhase(int fromepoch) const
 * \param fromepoch time_t seconds from epoch to calculate the moonphase for
 * 
 * This is a simple calculation to tell us roughly what the moon phase is
 * locally. It does not give position. It's roughly accurate, but not great.
 * 
 * The return value is 0 to 29, with 0 and 29 being hidden and 14 being full.
 */
int SunSet::moonPhase(int fromepoch) const
{
	int moonepoch = 614100;
    int phase = (fromepoch - moonepoch) % 2551443;
    int res = static_cast<int>(floor(phase / (24 * 3600))) + 1;
	
    if (res == 30)
        res = 0;

    return res;
}

/**
 * \fn int SunSet::moonPhase() const
 * 
 * Overload to set the moonphase for right now
 */
int SunSet::moonPhase() const
{
    time_t t = std::time(0);
    return moonPhase(static_cast<int>(t));
}
