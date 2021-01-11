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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Foobar.    If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __SUNPOSITION_H__
#define __SUNPOSITION_H__

#include <cmath>
#include <ctime>

#ifndef M_PI
  #define M_PI 3.14159265358979323846264338327950288
#endif

/**
 * \class SunSet
 * 
 * This class controls all aspects of the operations. The math is done in private
 * functions, and the public API's allow for returning a sunrise/sunset value for the
 * given coordinates and timezone.
 * 
 * The resulting calculations are relative to midnight of the day you set in the
 * setCurrentDate() function. It does not return a time_t value for delta from the
 * current epoch as that would not make sense as the sunrise/sunset can be calculated
 * thousands of years in the past. The library acts on a day timeframe, and doesn't
 * try to assume anything about any other unit of measurement other than the current
 * set day.
 * 
 * You can instantiate this class a few different ways, depending on your needs. It's possible
 * to set your location one time and forget about doing that again if you don't plan to move.
 * Then you only need to change the date and timezone to get correct data. Or, you can
 * simply create an object with no location or time data and then do that later. This is
 * a good mechanism for the setup()/loop() construct.
 * 
 * The most important thing to remember is to make sure the library knows the exact date and
 * timezone for the sunrise/sunset you are trying to calculate. Not doing so means you are going
 * to have very odd results. It's reasonably easy to know when you've forgotten one or the other
 * by looking at the time the sun would rise and noticing that it is X hours earlier or later.
 * That is, if you get a return of 128 minutes (2:08 AM) past midnight when the sun should rise 
 * at 308 (6:08 AM), then you probably forgot to set your EST timezone.
 * 
 * The library also has no idea about daylight savings time. If your timezone changes during the
 * year to account for savings time, you must update your timezone accordingly.
 */
class SunSet {
public:
    SunSet();
    SunSet(double, double, int);
    SunSet(double, double, double);
    ~SunSet();

    static constexpr double SUNSET_OFFICIAL = 90.833;       /**< Standard sun angle for sunset */
    static constexpr double SUNSET_NAUTICAL = 102.0;        /**< Nautical sun angle for sunset */
    static constexpr double SUNSET_CIVIL = 96.0;            /**< Civil sun angle for sunset */
    static constexpr double SUNSET_ASTONOMICAL = 108.0;     /**< Astronomical sun angle for sunset */
    
    void setPosition(double, double, int);
    void setPosition(double, double, double);
    void setTZOffset(int);
    void setTZOffset(double);
    double setCurrentDate(int, int, int);
    double calcNauticalSunrise() const;
    double calcNauticalSunset() const;
    double calcCivilSunrise() const;
    double calcCivilSunset() const;
    double calcAstronomicalSunrise() const;
    double calcAstronomicalSunset() const;
    double calcCustomSunrise(double) const;
    double calcCustomSunset(double) const;
    [[deprecated("UTC specific calls may not be supported in the future")]] double calcSunriseUTC();
    [[deprecated("UTC specific calls may not be supported in the future")]] double calcSunsetUTC();
    double calcSunrise() const;
    double calcSunset() const;
    int moonPhase(int) const;
    int moonPhase() const;
    
private:
    double degToRad(double) const;
    double radToDeg(double) const;
    double calcMeanObliquityOfEcliptic(double) const;
    double calcGeomMeanLongSun(double) const;
    double calcObliquityCorrection(double) const;
    double calcEccentricityEarthOrbit(double) const;
    double calcGeomMeanAnomalySun(double) const;
    double calcEquationOfTime(double) const;
    double calcTimeJulianCent(double) const;
    double calcSunTrueLong(double) const;
    double calcSunApparentLong(double) const;
    double calcSunDeclination(double) const;
    double calcHourAngleSunrise(double, double, double) const;
    double calcHourAngleSunset(double, double, double) const;
    double calcJD(int,int,int) const;
    double calcJDFromJulianCent(double) const;
    double calcSunEqOfCenter(double) const;
    double calcAbsSunrise(double) const;
    double calcAbsSunset(double) const;

    double m_latitude;
    double m_longitude;
    double m_julianDate;
    double m_tzOffset;
    int m_year;
    int m_month;
    int m_day;
};

#endif
