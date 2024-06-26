//
// BitBank Capactive Touch Sensor Library
// Written by Larry Bank
//
// Copyright 2023 BitBank Software, Inc. All Rights Reserved.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//===========================================================================
#include "bb_captouch.h"
void BBCapTouch::reset(int iRST)
{
     pinMode(iRST, OUTPUT);
     digitalWrite(iRST, LOW);
     delay(100);
     digitalWrite(iRST, HIGH);
     delay(100);
}  /* reset() */

//
// Initialize the library
// It only needs to initialize the I2C interface; the chip is ready
//
int BBCapTouch::init(int iSDA, int iSCL, int iRST, int iINT, uint32_t u32Speed)
{
uint8_t ucTemp[4];

    Wire.begin(iSDA, iSCL); // this is specific to ESP32 MCUs
    Wire.setClock(u32Speed);
    Wire.setTimeout(100);
    _iType = CT_TYPE_UNKNOWN;

#ifdef FUTURE
    if (I2CTest(AXS15231_ADDR)) {
       _iType = CT_TYPE_AXS15231;
       _iAddr = AXS15231_ADDR;
       if (iRST != -1) {
          reset(iRST);
       }
       return CT_SUCCESS;
    } // AXS15231
#endif

    if (I2CTest(CST226_ADDR)) {
       _iType = CT_TYPE_CST226;
       _iAddr = CST226_ADDR;
       if (iINT != -1) {
          pinMode(iINT, INPUT);
       }
       if (iRST != -1) {
          reset(iRST);
       }
    }
    if (I2CTest(GT911_ADDR1) || I2CTest(GT911_ADDR2)) {
       _iType = CT_TYPE_GT911;
    }
    if (_iType == CT_TYPE_GT911) { // reset the sensor to start it
       pinMode(iRST, OUTPUT);
       pinMode(iINT, OUTPUT);
       digitalWrite(iINT, LOW);
       digitalWrite(iRST, LOW);
       delay(5);
       digitalWrite(iINT, LOW); // set I2C addr to ADDR1
       delay(1);
       digitalWrite(iRST, HIGH); // when it comes out of reset, it samples INT
       delay(10);
       digitalWrite(iINT, LOW);
       delay(50);
       pinMode(iINT, INPUT);
       // double check the I2C addr in case it changed
       if (I2CTest(GT911_ADDR1)) {
          _iAddr = GT911_ADDR1;
       } else if (I2CTest(GT911_ADDR2)) {
          _iAddr = GT911_ADDR2;
       }
    } else if (I2CTest(FT6X36_ADDR)) {
       _iType = CT_TYPE_FT6X36;
       _iAddr = FT6X36_ADDR;
       if (iRST != -1) {
          reset(iRST);
       }
    } else if (I2CTest(CST820_ADDR)) {
       _iType = CT_TYPE_CST820;
       _iAddr = CST820_ADDR;
       if (iRST != -1) {
          reset(iRST);
       }
    } else {
       Wire.end();
       return CT_ERROR; // no device found
    }
    return CT_SUCCESS;
} /* init() */
//
// Test if an I2C device is monitoring an address
// return true if it responds, false if no response
//
bool BBCapTouch::I2CTest(uint8_t u8Addr)
{

  // Check if a device acknowledges the address.
  Wire.beginTransmission(u8Addr);
  return(Wire.endTransmission(true) == 0);
} /* I2CTest() */
//
// Write I2C data
// quits if a NACK is received and returns 0
// otherwise returns the number of bytes written
//
int BBCapTouch::I2CWrite(uint8_t u8Addr, uint8_t *pData, int iLen)
{
  int rc = 0;

    Wire.beginTransmission(u8Addr);
    Wire.write(pData, (uint8_t)iLen);
    rc = !Wire.endTransmission();
    return rc;
} /* I2CWrite() */
//
// Read N bytes starting at a specific 16-bit I2C register
//
int BBCapTouch::I2CReadRegister16(uint8_t u8Addr, uint16_t u16Register, uint8_t *pData, int iLen)
{
  int i = 0;

  Wire.beginTransmission(u8Addr);
  Wire.write((uint8_t)(u16Register>>8)); // high byte
  Wire.write((uint8_t)u16Register); // low byte
  Wire.endTransmission();
  Wire.requestFrom(u8Addr, (uint8_t)iLen);
  while (i < iLen)
  {
      pData[i++] = Wire.read();
  }
  return i;

} /* I2CReadRegister16() */
//
// Read N bytes starting at a specific I2C internal register
// returns 1 for success, 0 for error
//
int BBCapTouch::I2CReadRegister(uint8_t u8Addr, uint8_t u8Register, uint8_t *pData, int iLen)
{
  int rc;
  int i = 0;

  Wire.beginTransmission(u8Addr);
  Wire.write(u8Register);
  Wire.endTransmission();
  Wire.requestFrom(u8Addr, (uint8_t)iLen);
 // i = Wire.readBytes(pData, iLen);
  while (Wire.available() && i < iLen)
  {
      pData[i++] = Wire.read();
  }
  return i;
} /* I2CReadRegister() */
//
// Read N bytes
//
int BBCapTouch::I2CRead(uint8_t u8Addr, uint8_t *pData, int iLen)
{
  int rc;
  int i = 0;

  Wire.requestFrom(u8Addr, (uint8_t)iLen);
  while (i < iLen)
  {
     pData[i++] = Wire.read();
  }
  return i;
} /* I2CRead() */
//
// Private function to rotate touch samples if the user
// specified a new display orientation
//
void BBCapTouch::fixSamples(TOUCHINFO *pTI)
{
int i, x, y;

   for (i=0; i<pTI->count; i++) {
       switch (_iOrientation) {
           case 90:
               x = pTI->y[i];
               y = _iWidth - 1 - pTI->x[i];
               pTI->x[i] = x;
               pTI->y[i] = y;
               break;
           case 180:
               pTI->x[i] = _iWidth - 1 - pTI->x[i];
               pTI->y[i] = _iHeight - 1 - pTI->y[i];
               break;
           case 270:
               x = _iHeight - 1 - pTI->y[i];
               y = pTI->x[i];
               pTI->x[i] = x;
               pTI->y[i] = y;
               break;
           default: // do nothing
               break;
       }
   }
} /* fixSamples() */

//
// Read the touch points
// returns 0 (none), 1 if touch points are available
// The point count and info is returned in the TOUCHINFO structure
//
int BBCapTouch::getSamples(TOUCHINFO *pTI)
{
uint8_t c, *s, ucTemp[32];
int i, j, rc;
    
    if (!pTI)
       return 0;
    pTI->count = 0;

    if (_iType == CT_TYPE_AXS15231) {
        uint8_t ucReadCMD[8] = {0xb5,0xab,0xa5,0x5a,0,0,0,0x8};
        I2CWrite(_iAddr, (uint8_t *)ucReadCMD, 8);
        I2CRead(_iAddr, ucTemp, 14); // read up to 2 touch points
        c = ucTemp[1]; // number of touch points
        if (c == 0 || c > 2 || ucTemp[0] != 0) return 0;
        pTI->count = c;
        j = 0; // buffer offset
        for (i=0; i<c; i++) {
             pTI->x[i] = ((ucTemp[j+2] & 0xf) << 8) + ucTemp[j+3];
             pTI->y[i] = ((ucTemp[j+4] & 0xf) << 8) + ucTemp[j+5];
             pTI->area[i] = 1;
             j += 6; 
        }
        if (_iOrientation != 0) fixSamples(pTI);
        return (c > 0);
    } // AXS15231
 
    if (_iType == CT_TYPE_CST226) {
        i = I2CReadRegister(_iAddr, 0, ucTemp, 28); // read the whole block of regs
      Serial.printf("I2CReadRegister returned %d\n", i);
// #ifdef FUTURE
        if (ucTemp[0] == 0x83 && ucTemp[1] == 0x17 && ucTemp[5] == 0x80) {
        // home button pressed
            return 0;
        }
        if (ucTemp[6] != 0xab) return 0;
        if (ucTemp[0] == 0xab) return 0;
        if (ucTemp[5] == 0x80) return 0;
        c = ucTemp[5] & 0x7f;
        if (c > 5 || c == 0) { // invalid point count
           ucTemp[0] = 0;
           ucTemp[1] = 0xab;
           I2CWrite(_iAddr, ucTemp, 2); // reset
           return 0;
        }
// #endif
      //   c = 1; // debug
        pTI->count = c;
        Serial.printf("count = %d\n", c);
        j = 0;
        for (i=0; i<c; i++) {
           pTI->x[i] = (uint16_t)((ucTemp[j+1] << 4) | ((ucTemp[j+3] >> 4) & 0xf));
           pTI->y[i] = (uint16_t)((ucTemp[j+2] << 4) | (ucTemp[j+3] & 0xf));
           pTI->pressure[i] = ucTemp[j+4];
           j = (i == 0) ? (j+7) : (j+5);
        }
        if (_iOrientation != 0) fixSamples(pTI);
        return (c > 0);
    }

    if (_iType == CT_TYPE_CST820) {
       I2CReadRegister(_iAddr, CST820_TOUCH_REGS+1, ucTemp, 1); // read touch count
       if (ucTemp[0] < 1 || ucTemp[0] > 5) { // something went wrong
           return 0;
       }
       if (ucTemp[0] >= 1) { // touch data available, read it
           pTI->count = ucTemp[0];
           I2CReadRegister(_iAddr, CST820_TOUCH_REGS+2, ucTemp, pTI->count * 6);
           s = ucTemp;
           for (i=0; i<pTI->count; i++) {
              pTI->x[i] = ((s[0] & 0xf) << 8) | s[1];
              pTI->y[i] = ((s[2] & 0xf) << 8) | s[3];
              pTI->area[i] = 1; // no data available
              s += 6;
           }
           if (_iOrientation != 0) fixSamples(pTI);
           return 1;
       }
    }
    if (_iType == CT_TYPE_FT6X36) {
       rc = I2CReadRegister(_iAddr, TOUCH_REG_STATUS, ucTemp, 1); // read touch status
       if (rc == 0) { // something went wrong
           return 0;
       }
       i = ucTemp[0]; // number of touch points available
       if (i >= 1) { // get data
           rc = I2CReadRegister(_iAddr, TOUCH_REG_XH, ucTemp, 6*i); // read X+Y position(s)
           if ((ucTemp[0] & 0x40) == 0 && (ucTemp[2] & 0xf0) != 0xf0) { // finger is down
               pTI->x[0] = ((ucTemp[0] & 0xf) << 8) | ucTemp[1];
               pTI->y[0] = ((ucTemp[2] & 0xf) << 8) | ucTemp[3];
               // get touch pressure and area
               pTI->pressure[0] = ucTemp[4];
               pTI->area[0] = ucTemp[5];
               pTI->count++;
           }
           if (i > 1) { // get second point
               if ((ucTemp[6] & 0x40) == 0 && (ucTemp[8] & 0xf0) != 0xf0) { // finger is down
                   pTI->x[1] = ((ucTemp[6] & 0xf) << 8) | ucTemp[7];
                   pTI->y[1] = ((ucTemp[8] & 0xf) << 8) | ucTemp[9];
                   // get touch pressure and area
                   pTI->pressure[1] = ucTemp[10];
                   pTI->area[1] = ucTemp[11];
                   pTI->count++;
               }
           }
       } // if touch points available
       if (_iOrientation != 0) fixSamples(pTI);
       return (i > 0); // Avoid returning empty data
    } else { // GT911
      I2CReadRegister16(_iAddr, GT911_POINT_INFO, ucTemp, 1); // get number of touch points
      i = ucTemp[0] & 0xf; // number of touches
      if (i <= 5 && ucTemp[0] & 0x80) { // if buffer status is good + >= 1 touch points
         ucTemp[0] = (uint8_t)(GT911_POINT_INFO >> 8);
         ucTemp[1] = (uint8_t)GT911_POINT_INFO;
         ucTemp[2] = 0; // clear touch info for next time
         I2CWrite(_iAddr, ucTemp, 3);

         pTI->count = i;
         for (int j=0; j<i; j++) { // read each touch point block
             I2CReadRegister16(_iAddr, GT911_POINT_1 + (j*8), ucTemp, 7);
             pTI->x[j] = ucTemp[1] + (ucTemp[2] << 8);
             pTI->y[j] = ucTemp[3] + (ucTemp[4] << 8);
             pTI->area[j] = ucTemp[5] + (ucTemp[6] << 8);
             pTI->pressure[j] = 0; 
         }
         if (i && _iOrientation != 0) fixSamples(pTI);
         return (i > 0);
      }
    } // GT911
    return 0;
} /* getSamples() */

int BBCapTouch::setOrientation(int iOrientation, int iWidth, int iHeight)
{
    if (iOrientation != 0 && iOrientation != 90 && iOrientation != 180 && iOrientation != 270) {
       return CT_ERROR;
    }
    _iOrientation = iOrientation;
    _iWidth = iWidth;
    _iHeight = iHeight;
    return CT_SUCCESS;
} /* setOrientation() */

int BBCapTouch::sensorType(void)
{
   return _iType;
} /* type() */
