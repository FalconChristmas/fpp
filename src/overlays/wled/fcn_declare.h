#ifndef WLED_FCN_DECLARE_H
#define WLED_FCN_DECLARE_H

/*
 * All globally accessible functions are declared here
 */

#include "color.h"
#include "pixeltypes.h"
#include "colorpalettes.h"
#include "noise.h"


inline uint8_t pgm_read_byte_near(uint8_t *p) {
    return *p;
}
inline uint8_t pgm_read_byte_near(const unsigned char *p) {
    return *p;
}

//colors.cpp
uint32_t color_blend(uint32_t,uint32_t,uint16_t,bool b16=false);
uint32_t color_add(uint32_t,uint32_t);
inline uint32_t colorFromRgbw(byte* rgbw) { return uint32_t((byte(rgbw[3]) << 24) | (byte(rgbw[0]) << 16) | (byte(rgbw[1]) << 8) | (byte(rgbw[2]))); }
void colorHStoRGB(uint16_t hue, byte sat, byte* rgb); //hue, sat to rgb
void colorKtoRGB(uint16_t kelvin, byte* rgb);
void colorCTtoRGB(uint16_t mired, byte* rgb); //white spectrum to rgb
void colorXYtoRGB(float x, float y, byte* rgb); // only defined if huesync disabled TODO
void colorRGBtoXY(byte* rgb, float* xy); // only defined if huesync disabled TODO
void colorFromDecOrHexString(byte* rgb, char* in);
bool colorFromHexString(byte* rgb, const char* in);
uint32_t colorBalanceFromKelvin(uint16_t kelvin, uint32_t rgb);
uint16_t approximateKelvinFromRGB(uint32_t rgb);
void setRandomColor(byte* rgb);
uint8_t gamma8_cal(uint8_t b, float gamma);
void calcGammaTable(float gamma);
uint8_t gamma8(uint8_t b);
int16_t extractModeDefaults(uint8_t mode, const char *segVar);
void colorHStoRGB(uint16_t hue, byte sat, byte* rgb);
CRGB getCRGBForBand(int x, uint8_t *fftResult, int pal);

#include <math.h>
#define sin_t sin
#define cos_t cos
#define tan_t tan
#define asin_t asin
#define acos_t acos
#define atan_t atan
#define fmod_t fmod
#define floor_t floor

uint16_t crc16(const unsigned char* data_p, size_t length);





//um_manager.cpp
typedef enum UM_Data_Types {
  UMT_BYTE = 0,
  UMT_UINT16,
  UMT_INT16,
  UMT_UINT32,
  UMT_INT32,
  UMT_FLOAT,
  UMT_DOUBLE,
  UMT_BYTE_ARR,
  UMT_UINT16_ARR,
  UMT_INT16_ARR,
  UMT_UINT32_ARR,
  UMT_INT32_ARR,
  UMT_FLOAT_ARR,
  UMT_DOUBLE_ARR
} um_types_t;
typedef struct UM_Exchange_Data {
  // should just use: size_t arr_size, void **arr_ptr, byte *ptr_type
  size_t       u_size;                 // size of u_data array
  um_types_t  *u_type;                 // array of data types
  void       **u_data;                 // array of pointers to data
  UM_Exchange_Data() {
    u_size = 0;
    u_type = nullptr;
    u_data = nullptr;
  }
  ~UM_Exchange_Data() {
    if (u_type) delete[] u_type;
    if (u_data) delete[] u_data;
  }
} um_data_t;
const unsigned int um_data_size = sizeof(um_data_t);  // 12 bytes


class Usermods {
  protected:
  public:
    Usermods() { }
    virtual ~Usermods() { }
    virtual bool getUMData(um_data_t **data, int id) { return false; };
};
extern Usermods usermods;
um_data_t* simulateSound(uint8_t simulationId);
CHSV rgb2hsv_approximate( const CRGB& rgb);



inline uint16_t random(uint16_t a, uint16_t b) { return random16(a, b); }
inline uint16_t random(uint16_t a) { return random16(a); }

#endif
