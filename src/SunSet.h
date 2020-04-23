#pragma once
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


#include <math.h>
#include <time.h>

class SunSet {
public:
  SunSet();
  SunSet(double, double, int);
  ~SunSet();

  void setPosition(double, double, int);
  void setTZOffset(int);
  double setCurrentDate(int, int, int);
  double calcSunriseUTC();
  double calcSunsetUTC();
  double calcSunrise();
  double calcSunset();
  int moonPhase(int);
  int moonPhase();

private:
  double degToRad(double);
  double radToDeg(double);
  double calcMeanObliquityOfEcliptic(double);
  double calcGeomMeanLongSun(double);
  double calcObliquityCorrection(double);
  double calcEccentricityEarthOrbit(double);
  double calcGeomMeanAnomalySun(double);
  double calcEquationOfTime(double);
  double calcTimeJulianCent(double);
  double calcSunTrueLong(double);
  double calcSunApparentLong(double);
  double calcSunDeclination(double);
  double calcHourAngleSunrise(double, double);
  double calcHourAngleSunset(double, double);
  double calcJD(int,int,int);
  double calcJDFromJulianCent(double);
  double calcSunEqOfCenter(double);

  double latitude;
  double longitude;
  double julianDate;
  int m_year;
  int m_month;
  int m_day;
  int tzOffset;
};
